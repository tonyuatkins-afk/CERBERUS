/*
 * DMA controller diagnostic — Phase 2 Task 2.6. See
 * docs/plans/v0.3-diagnose-completion.md.
 *
 * Probes the 8237 DMA controller(s) by writing a test pattern to each
 * channel's word-count register and reading it back. Healthy channels
 * return the pattern. Stuck-at-one (0xFFFF) or unrelated values indicate
 * either controller failure or, for channels 4-7 on XT-class machines,
 * absence of the slave controller.
 *
 * Safety model:
 *   - Channel 0 (master) is DRAM refresh on AT-class. Writing to its
 *     count register could cause a refresh hiccup and corrupt memory.
 *     HARD-SKIP.
 *   - Channel 4 (slave ch 0) is the cascade link on AT — tying into
 *     master ch 0's cascade pin. Writing here would propagate into the
 *     cascade logic. HARD-SKIP.
 *   - Mask register (0x0A master, 0xD4 slave) is NOT touched. A channel
 *     masked by the BIOS stays masked; we only probe the count register
 *     (non-destructive read+restore).
 *   - Byte-pointer flip-flop (0x0C master, 0xD8 slave) is reset before
 *     each probe so the high/low byte pair is in a known state.
 *
 * Detection logic for XT-class:
 *   - `cpu.class` = "8088" / "8086" / "v20" / "v30" → master only.
 *   - `bus.class` = "isa8" → also implies XT-class, skip slave probe.
 *   - Otherwise probe all 6 channels (1/2/3 on master, 5/6/7 on slave).
 *
 * What this diagnostic does NOT catch:
 *   - DMA transfer correctness. Running a real transfer to test it is
 *     destructive (could corrupt memory). Out of scope.
 *   - Channel-in-use detection. Would need to hook INT 8 to observe
 *     BIOS/driver DMA activity. Invasive and not a diagnostic scope.
 *   - Cascade logic between master ch 0 and slave. Touching either is
 *     unsafe (see safety model).
 *   - DMA bus-mastering on VLB/PCI. Different controller family.
 *
 * Verdict on `diagnose.dma.summary`:
 *   - All probed channels respond → PASS
 *   - Some channels respond, some don't → WARN
 *   - No probed channels respond → FAIL (controller dead or port locked)
 *   - No channels probed (e.g., CPU class missing from detect path) →
 *     no emit, rule-not-applicable
 */

#include <stdio.h>
#include <string.h>
#ifndef CERBERUS_HOST_TEST
#include <conio.h>                  /* inp()/outp() for port probes */
#endif
#include "diag.h"
#include "../core/report.h"

/* DMA master controller (channels 0-3) port map — safe-to-probe subset. */
#define DMA_M_PORT_CH1_COUNT    0x03
#define DMA_M_PORT_CH2_COUNT    0x05
#define DMA_M_PORT_CH3_COUNT    0x07
#define DMA_M_PORT_CLEAR_FF     0x0C   /* reset byte-pointer flip-flop */

/* DMA slave controller (channels 4-7, AT only) port map. */
#define DMA_S_PORT_CH5_COUNT    0xC6
#define DMA_S_PORT_CH6_COUNT    0xCA
#define DMA_S_PORT_CH7_COUNT    0xCE
#define DMA_S_PORT_CLEAR_FF     0xD8

/* Test pattern chosen for alternating bit coverage — catches stuck-high,
 * stuck-low, and adjacent-line-short faults. */
#define DMA_PROBE_PATTERN       0xA55A

/* Display string — one summary, max ~70 chars. */
static char diag_dma_summary_detail[96];

/* Per-channel static status strings (short enough to be literal values). */
static const char *const DMA_STATUS_PASS    = "pass";
static const char *const DMA_STATUS_FAIL    = "FAIL: count register readback mismatch";
static const char *const DMA_STATUS_SKIPPED = "skipped (safety: refresh/cascade channel)";
static const char *const DMA_STATUS_NO_SLV  = "skipped_no_slave (XT-class machine)";

/* ----------------------------------------------------------------------- */
/* Pure-math summary kernel — host-testable.                                */
/*                                                                          */
/* Inputs: count of probed-pass, probed-fail, and skipped channels.        */
/* Output: verdict for the dma.summary row.                                 */
/* ----------------------------------------------------------------------- */

verdict_t diag_dma_summary_verdict(int ch_pass, int ch_fail, int ch_skip)
{
    int probed;
    (void)ch_skip;
    probed = ch_pass + ch_fail;
    if (probed == 0)        return VERDICT_UNKNOWN;  /* rule not applicable */
    if (ch_fail == 0)       return VERDICT_PASS;
    if (ch_pass == 0)       return VERDICT_FAIL;
    return VERDICT_WARN;
}

/* ----------------------------------------------------------------------- */
/* Low-level probe                                                          */
/* ----------------------------------------------------------------------- */

/* Probes one channel by:
 *   1. resetting the byte-pointer flip-flop
 *   2. saving current count (word read — low then high byte)
 *   3. resetting flip-flop
 *   4. writing test pattern (low byte then high byte)
 *   5. resetting flip-flop
 *   6. reading back (low then high byte)
 *   7. resetting flip-flop
 *   8. restoring the original value (low byte then high byte)
 *
 * Returns 1 if readback matched DMA_PROBE_PATTERN, 0 otherwise. */
static int probe_dma_channel(unsigned int count_port, unsigned int clear_ff_port)
{
    unsigned char saved_lo, saved_hi;
    unsigned char rb_lo, rb_hi;
    unsigned int readback;

    /* Save current state */
    outp(clear_ff_port, 0);
    saved_lo = (unsigned char)inp(count_port);
    saved_hi = (unsigned char)inp(count_port);

    /* Write pattern */
    outp(clear_ff_port, 0);
    outp(count_port, (unsigned char)(DMA_PROBE_PATTERN & 0xFFU));
    outp(count_port, (unsigned char)((DMA_PROBE_PATTERN >> 8) & 0xFFU));

    /* Read back */
    outp(clear_ff_port, 0);
    rb_lo = (unsigned char)inp(count_port);
    rb_hi = (unsigned char)inp(count_port);
    readback = (unsigned int)rb_lo | ((unsigned int)rb_hi << 8);

    /* Restore */
    outp(clear_ff_port, 0);
    outp(count_port, saved_lo);
    outp(count_port, saved_hi);

    return (readback == DMA_PROBE_PATTERN) ? 1 : 0;
}

/* ----------------------------------------------------------------------- */
/* Orchestration                                                            */
/* ----------------------------------------------------------------------- */

static const result_t *find_key_local(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Detect whether this machine is XT-class (master DMA only) or AT-class
 * (both master + slave). Uses cpu.class for primary signal, falls back
 * to bus.class if cpu.class isn't populated. Default: AT (probe slave). */
static int is_xt_class(const result_table_t *t)
{
    const result_t *cls  = find_key_local(t, "cpu.class");
    const result_t *bus  = find_key_local(t, "bus.class");
    const char *cv, *bv;
    if (cls) {
        cv = cls->display ? cls->display :
             (cls->type == V_STR ? cls->v.s : (const char *)0);
        if (cv) {
            if (strcmp(cv, "8088") == 0 || strcmp(cv, "8086") == 0 ||
                strcmp(cv, "v20")  == 0 || strcmp(cv, "v30")  == 0) return 1;
        }
    }
    if (bus) {
        bv = bus->display ? bus->display :
             (bus->type == V_STR ? bus->v.s : (const char *)0);
        if (bv && strcmp(bv, "isa8") == 0) return 1;
    }
    return 0;
}

void diag_dma(result_table_t *t)
{
    int xt_class = is_xt_class(t);
    int ch_pass = 0;
    int ch_fail = 0;
    int ch_skip = 0;
    int rc;
    verdict_t summary_v;
    const char *summary_tag;

    /* Channel 0 + Channel 4 — ALWAYS skipped (refresh / cascade). */
    report_add_str(t, "diagnose.dma.ch0_status", DMA_STATUS_SKIPPED,
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.dma.ch4_status",
                   xt_class ? DMA_STATUS_NO_SLV : DMA_STATUS_SKIPPED,
                   CONF_HIGH, VERDICT_UNKNOWN);
    ch_skip += 2;

    /* Master channels 1-3 — always probed. */
    rc = probe_dma_channel(DMA_M_PORT_CH1_COUNT, DMA_M_PORT_CLEAR_FF);
    if (rc) { ch_pass++; report_add_str(t, "diagnose.dma.ch1_status", DMA_STATUS_PASS, CONF_HIGH, VERDICT_PASS); }
    else    { ch_fail++; report_add_str(t, "diagnose.dma.ch1_status", DMA_STATUS_FAIL, CONF_HIGH, VERDICT_FAIL); }

    rc = probe_dma_channel(DMA_M_PORT_CH2_COUNT, DMA_M_PORT_CLEAR_FF);
    if (rc) { ch_pass++; report_add_str(t, "diagnose.dma.ch2_status", DMA_STATUS_PASS, CONF_HIGH, VERDICT_PASS); }
    else    { ch_fail++; report_add_str(t, "diagnose.dma.ch2_status", DMA_STATUS_FAIL, CONF_HIGH, VERDICT_FAIL); }

    rc = probe_dma_channel(DMA_M_PORT_CH3_COUNT, DMA_M_PORT_CLEAR_FF);
    if (rc) { ch_pass++; report_add_str(t, "diagnose.dma.ch3_status", DMA_STATUS_PASS, CONF_HIGH, VERDICT_PASS); }
    else    { ch_fail++; report_add_str(t, "diagnose.dma.ch3_status", DMA_STATUS_FAIL, CONF_HIGH, VERDICT_FAIL); }

    /* Slave channels 5-7 — probe only if AT-class. */
    if (!xt_class) {
        rc = probe_dma_channel(DMA_S_PORT_CH5_COUNT, DMA_S_PORT_CLEAR_FF);
        if (rc) { ch_pass++; report_add_str(t, "diagnose.dma.ch5_status", DMA_STATUS_PASS, CONF_HIGH, VERDICT_PASS); }
        else    { ch_fail++; report_add_str(t, "diagnose.dma.ch5_status", DMA_STATUS_FAIL, CONF_HIGH, VERDICT_FAIL); }

        rc = probe_dma_channel(DMA_S_PORT_CH6_COUNT, DMA_S_PORT_CLEAR_FF);
        if (rc) { ch_pass++; report_add_str(t, "diagnose.dma.ch6_status", DMA_STATUS_PASS, CONF_HIGH, VERDICT_PASS); }
        else    { ch_fail++; report_add_str(t, "diagnose.dma.ch6_status", DMA_STATUS_FAIL, CONF_HIGH, VERDICT_FAIL); }

        rc = probe_dma_channel(DMA_S_PORT_CH7_COUNT, DMA_S_PORT_CLEAR_FF);
        if (rc) { ch_pass++; report_add_str(t, "diagnose.dma.ch7_status", DMA_STATUS_PASS, CONF_HIGH, VERDICT_PASS); }
        else    { ch_fail++; report_add_str(t, "diagnose.dma.ch7_status", DMA_STATUS_FAIL, CONF_HIGH, VERDICT_FAIL); }
    } else {
        report_add_str(t, "diagnose.dma.ch5_status", DMA_STATUS_NO_SLV, CONF_HIGH, VERDICT_UNKNOWN);
        report_add_str(t, "diagnose.dma.ch6_status", DMA_STATUS_NO_SLV, CONF_HIGH, VERDICT_UNKNOWN);
        report_add_str(t, "diagnose.dma.ch7_status", DMA_STATUS_NO_SLV, CONF_HIGH, VERDICT_UNKNOWN);
        ch_skip += 3;
    }

    /* Summary */
    summary_v = diag_dma_summary_verdict(ch_pass, ch_fail, ch_skip);
    summary_tag = (summary_v == VERDICT_PASS) ? "ok" :
                  (summary_v == VERDICT_FAIL) ? "dead" :
                  (summary_v == VERDICT_WARN) ? "partial" : "skipped";

    sprintf(diag_dma_summary_detail,
            "%s (%d/%d channels responsive, %d safety-skipped)",
            summary_tag, ch_pass, ch_pass + ch_fail, ch_skip);
    report_add_str(t, "diagnose.dma.summary", diag_dma_summary_detail,
                   CONF_HIGH, summary_v);
}
