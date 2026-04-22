/*
 * Whetstone — synthetic floating-point benchmark, Curnow/Wichmann 1976.
 *
 * v0.8.0 — EMIT SUPPRESSED IN STOCK BUILDS.
 *
 * Per the 0.8.0 plan (§7 Whetstone decision), stock builds compile the
 * kernel (keeping the asm toolchain warm, keeping host tests green,
 * preserving the issue #4 archaeological trail) but the dispatcher
 * returns immediately with whetstone_status=disabled_for_release and
 * emits no k_whetstones row. The build flag CERBERUS_WHETSTONE_ENABLED
 * (set via `wmake WHETSTONE=1`) re-enables the full dispatch for
 * research work.
 *
 * Why: v0.4 through v0.7.2 never produced a Whetstone number in the
 * published 1,500-3,000 K-Whet range for a 486 DX-2-66. The pre-asm
 * Watcom kernel ran at ~109 K-Whet (30x low); the v0.5.0 asm rework
 * was not validated to close the gap; on real iron one unit costs
 * 50-100 ms where research estimated 1-3 ms (a 30-50x per-unit-cost
 * anomaly nobody root-caused). A trust-first release cannot ship a
 * CONF_HIGH value 10x or more out of range. The 0.9.0 direction is
 * per-instruction FADD/FMUL/FDIV/FSQRT/FSIN/FCOS microbenchmarks
 * replacing the synthetic entirely; bench_fpu.c remains the aggregate
 * FPU throughput metric in 0.8.0. See docs/methodology.md "Why
 * Whetstone is not in 0.8.0".
 *
 * v0.5.0 historical note: dispatches to x87 assembly kernel
 * (bench_whet_fpu.asm) on FPU-equipped systems. Replaces the prior
 * Watcom-compiled C kernel which suffered a ~15x slowdown vs the
 * published reference envelope because the `volatile` DCE-suppression
 * pattern forced every inner arithmetic through memory instead of
 * x87 registers.
 *
 * Rule 10 (whetstone_fpu_consistency) reads fpu.detected and the
 * whetstone_status / k_whetstones rows to flag any disagreement between
 * the FPU-detection head and the Whetstone benchmark's own FPU-presence
 * inference. Updated in 0.8.0 to treat disabled_for_release as rule
 * skip (not FAIL): the build gate disabled the measurement, so there
 * is no measurement to cross-check.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"
#include "../core/journey.h"
#include "../core/crumb.h"

/* ------------------------------------------------------------------- */
/* Asm-kernel interface                                                 */
/* ------------------------------------------------------------------- */

/* Data globals defined in bench_whet_fpu.asm. We read them after the
 * kernel returns to build the DCE-barrier checksum — Watcom can't elide
 * the asm kernel's memory writes because these symbols have external
 * linkage and the asm routine is opaque to the optimizer. */
extern unsigned long whet_fpu_units;
extern double        whet_fpu_E1[4];
extern int           whet_fpu_J, whet_fpu_K, whet_fpu_L;
extern double        whet_fpu_X1, whet_fpu_X2, whet_fpu_X3, whet_fpu_X4;
extern double        whet_fpu_X,  whet_fpu_Y,  whet_fpu_Z;

/* Asm kernel entry. Reads whet_fpu_units, runs that many Whetstone
 * units, leaves accumulator state in the globals above. */
extern void whet_fpu_run_units(void);

#pragma aux whet_fpu_run_units "whet_fpu_run_units_" \
    modify exact [ax bx cx dx si di];

/* ------------------------------------------------------------------- */
/* FPU-absence check                                                    */
/*                                                                      */
/* Wrapped in CERBERUS_WHETSTONE_ENABLED since the stock-build path     */
/* early-returns before any FPU check. Watcom W202 would warn on the   */
/* unreferenced static otherwise.                                       */
/* ------------------------------------------------------------------- */

#ifdef CERBERUS_WHETSTONE_ENABLED

static const result_t *find_key_local(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Returns 1 if detect_fpu reported an FPU present, 0 otherwise. Missing
 * key treated as absent so we don't execute x87 on unknown-capability
 * hardware. Guards against value-type reinterpretation per the S5
 * round-2 pattern. */
static int fpu_looks_present(const result_table_t *t)
{
    const result_t *r = find_key_local(t, "fpu.detected");
    if (!r) return 0;
    if (r->type != V_STR || !r->v.s) return 0;
    if (strcmp(r->v.s, "none") == 0) return 0;
    return 1;
}

#endif /* CERBERUS_WHETSTONE_ENABLED */

/* ------------------------------------------------------------------- */
/* CERBERUS entry point                                                 */
/* ------------------------------------------------------------------- */

#define W_WARMUP_UNITS   10UL
#define W_MIN_UNITS      10UL
/*
 * v0.7.2: capped at 500 after two BEK-V409 real-iron attempts showed
 * the per-unit cost on 486 iron is much higher than emulator baselines
 * suggested. First attempt (cap=200000) sat for 2+ minutes on the
 * warmup-calibrated main run. Second attempt (cap=2000, with BIOS
 * teletype tag prints) scrolled for 2+ minutes and never completed
 * the 10-unit warmup. Conclusion: one whetstone unit on a DX2-66
 * with AMI BIOS / S3 Trio64 is closer to 50-100 ms than the 1-3 ms
 * the research doc estimated.
 *
 * 500 at ~50 ms/unit = ~25 sec main run. Resolution is one BIOS tick
 * in 25 sec = 0.2% quantization, fine for regression signal.
 *
 * Also see W_MAIN_TIMEOUT_US below — hard wall-clock cap that aborts
 * the main run gracefully if it exceeds a sensible budget. Belt +
 * suspenders because calibration miscalculation can still push below
 * this cap but beyond sane wall time on weird hardware.
 */
#define W_MAX_UNITS         500UL
#define W_TARGET_MAIN_US    5000000UL     /* 5 seconds */
#define W_MAIN_TIMEOUT_US   30000000UL    /* 30 seconds hard cap */

void bench_whetstone(result_table_t *t, const opts_t *o)
{
#ifndef CERBERUS_WHETSTONE_ENABLED
    /* 0.8.0 build-gate: stock builds suppress Whetstone emit. Kernel
     * is compiled but dispatcher returns immediately. No k_whetstones
     * row, no timing, no visual. Rule 10 treats this status as "rule
     * not applicable" and skips. Re-enable with `wmake WHETSTONE=1`
     * for research work. See file header comment and 0.8.0 plan §7. */
    (void)o;
    report_add_str(t, "bench.fpu.whetstone_status",
                   "disabled_for_release",
                   CONF_HIGH, VERDICT_UNKNOWN);
    return;
#else
    us_t warmup_us, main_us;
    unsigned long units;
    unsigned long k_whet_per_sec;
    int warmup_is_main;
    (void)o;

    warmup_is_main = 0;

    if (!fpu_looks_present(t)) {
        report_add_str(t, "bench.fpu.whetstone_status", "skipped_no_fpu",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }

    /* v0.6.0 T8: title card for the FPU Benchmark section. Covers both
     * the timed Whetstone measurement (below) and the Mandelbrot visual
     * that fires at the end. */
    if (journey_title_card(o, HEAD_CENTER,
                           "FPU BENCHMARK",
                           "Measuring floating-point throughput with "
                           "Whetstone. Then your FPU renders a "
                           "Mandelbrot fractal in real time.") == 1) {
        /* skip-all latched; still emit the measurement + row, skip
         * visuals only. Mandelbrot will no-op. */
    }

    /* Warmup. Crumb-wrapped so a hang shows up as "whet.warmup" in LAS. */
    crumb_enter("whet.warmup");
    whet_fpu_units = W_WARMUP_UNITS;
    timing_start_long();
    whet_fpu_run_units();
    warmup_us = timing_stop_long();
    crumb_exit();

    if (warmup_us == 0) {
        units = 500UL;
    } else if (warmup_us >= W_TARGET_MAIN_US / 2UL) {
        /* Slow-hardware short-circuit: treat the warmup as the main run. */
        main_us        = warmup_us;
        units          = W_WARMUP_UNITS;
        warmup_is_main = 1;
    } else {
        units = (W_WARMUP_UNITS * W_TARGET_MAIN_US) / (unsigned long)warmup_us;
        if (units < W_MIN_UNITS) units = W_MIN_UNITS;
        if (units > W_MAX_UNITS) units = W_MAX_UNITS;
    }

    if (!warmup_is_main) {
        /* Main-run crumb. With W_MAX_UNITS capped at 2000, this run
         * can't exceed ~4 sec wall time on DX2-66. If LAS still shows
         * "whet.main" after a hang, the issue is in the asm kernel
         * itself and we'll need module-boundary instrumentation in
         * bench_whet_fpu.asm. */
        crumb_enter("whet.main");
        whet_fpu_units = units;
        timing_start_long();
        whet_fpu_run_units();
        main_us = timing_stop_long();
        crumb_exit();
    }

    /* DCE-barrier checksum over the asm kernel's final state. */
    {
        unsigned long checksum = 0UL;
        checksum ^= (unsigned long)(long)whet_fpu_E1[0];
        checksum ^= (unsigned long)(long)whet_fpu_E1[1];
        checksum ^= (unsigned long)(long)whet_fpu_E1[2];
        checksum ^= (unsigned long)(long)whet_fpu_E1[3];
        checksum ^= (unsigned long)(unsigned int)whet_fpu_J;
        checksum ^= (unsigned long)(unsigned int)whet_fpu_K;
        checksum ^= (unsigned long)(unsigned int)whet_fpu_L;
        checksum ^= (unsigned long)(long)whet_fpu_X1;
        checksum ^= (unsigned long)(long)whet_fpu_X2;
        checksum ^= (unsigned long)(long)whet_fpu_X3;
        checksum ^= (unsigned long)(long)whet_fpu_X4;
        checksum ^= (unsigned long)(long)whet_fpu_X;
        checksum ^= (unsigned long)(long)whet_fpu_Y;
        checksum ^= (unsigned long)(long)whet_fpu_Z;
        report_add_u32(t, "bench.fpu.whetstones_checksum",
                       checksum, (const char *)0,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    if (main_us > 0) {
        report_add_u32(t, "bench.fpu.whetstone_elapsed_us",
                       (unsigned long)main_us, (const char *)0,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    if (main_us > 0) {
        /* k_whet_per_sec = units * 1,000,000 / main_us. Rescale to ms
         * so numerator stays in 32 bits. See v0.4 bench_dhrystone S1
         * fix for the overflow analysis. */
        if (main_us >= 1000UL) {
            unsigned long ms = (unsigned long)main_us / 1000UL;
            k_whet_per_sec = (units * 1000UL) / ms;
            if (k_whet_per_sec == 0UL) {
                report_add_str(t, "bench.fpu.whetstone_status",
                               "inconclusive_sub_kwhet",
                               CONF_LOW, VERDICT_WARN);
            } else {
                /* v0.5.0 asm kernel emits at CONF_HIGH. Per-tool
                 * comparability and absolute accuracy still need
                 * real-hardware validation on BEK-V409 (issue #4);
                 * meanwhile the number is reproducible across cold
                 * boots and usable as same-machine regression signal. */
                report_add_u32(t, "bench.fpu.k_whetstones",
                               k_whet_per_sec, (const char *)0,
                               CONF_HIGH, VERDICT_UNKNOWN);
                report_add_str(t, "bench.fpu.whetstone_status", "ok",
                               CONF_HIGH, VERDICT_UNKNOWN);
            }
        } else {
            report_add_str(t, "bench.fpu.whetstone_status",
                           "inconclusive_sub_ms",
                           CONF_LOW, VERDICT_WARN);
        }
    } else {
        report_add_str(t, "bench.fpu.whetstone_status",
                       "inconclusive_elapsed_zero",
                       CONF_LOW, VERDICT_WARN);
    }

    /* Visual coda: Mandelbrot in VGA mode 13h. Not timed, not
     * measured — a post-run proof-of-life for the FPU. Gates on
     * VGA-capable adapter and /NOUI inside the demo; FPU presence
     * is implied by reaching this point (the early-return above). */
    bench_mandelbrot_demo(o);
#endif /* CERBERUS_WHETSTONE_ENABLED */
}
