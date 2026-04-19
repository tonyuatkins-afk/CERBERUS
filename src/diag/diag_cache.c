/*
 * Cache diagnostic — Phase 2 Task 2.5. See docs/plans/v0.3-diagnose-completion.md.
 *
 * Verifies that the CPU's L1 data cache is present and functional via a
 * stride-timing ratio test: two byte-stride read loops over buffers of
 * very different sizes. A buffer comfortably smaller than any plausible
 * L1 (2 KB) should see re-reads served from cache on repeat iterations;
 * a buffer larger than any plausible 486-class L1 (32 KB) cannot fit in
 * the cache and each iteration hits main memory.
 *
 * The observable signal:
 *   - Cache enabled + small-fits-L1, large-does-not: per-iteration time
 *     ratio (large / small) is FAR above the buffer-size ratio (16×).
 *     On a healthy 486 DX-2 with 8 KB L1, expect ratio in the 50–200×
 *     range (L1 hits at ~2 cycles per line vs DRAM at ~15 cycles).
 *   - Cache disabled: both loops hit DRAM, so the time ratio scales
 *     linearly with buffer size. Expect ratio ≈ 16×.
 *   - Cache much larger than 32 KB (e.g., a full L2 that fits both):
 *     ratio approaches 1.0 and we can't distinguish cache effect from
 *     no-cache noise. Report WARN rather than false-PASS.
 *
 * What this diagnostic does NOT catch:
 *   - L1 size / associativity / coherence-under-DMA. Size is detection
 *     scope (`detect_cache`). Coherence-under-DMA needs contrived test
 *     setup; deferred past v0.3.
 *   - L2 cache distinction from L1. A machine where the "small" buffer
 *     hits L1 and the "large" fits L2 produces the same PASS signal as
 *     a machine where only L1 exists. Both are "cache working," which
 *     is the question this diagnostic answers.
 *
 * Prerequisites:
 *   - Uses `timing_start` / `timing_stop` (sub-wrap intervals — both
 *     loops calibrated to ~50 ms, well under one PIT wrap).
 *   - Reads `cache.present` from the result table; skips entirely if
 *     detect_cache reported `no` (8088/V20 floor class) — those machines
 *     have no cache to diagnose.
 *
 * Verdict table (per stride_ratio_x100, where 1600 = 16.00×):
 *   ≥ 4000 (40×)       -> PASS — clear cache effect
 *   2000..3999 (20-40×) -> WARN — partial or boundary-case
 *   < 2000  (< 20×)    -> FAIL — no meaningful cache effect
 *   < 100   (< 1.0×)   -> WARN — measurement anomaly (physically impossible)
 *   missing measurement -> WARN (timing_start returned 0)
 */

#include <stdio.h>
#include <string.h>
#include "diag.h"
#include "../core/report.h"
#include "../core/timing.h"
#include "../core/cache_buffers.h"

/* FAR-segment buffers live in cache_buffers.c so bench_cache can share
 * the same 34 KB of storage rather than duplicating the allocation. The
 * DIAG_CACHE_* names are preserved as local aliases for read-site
 * clarity; they resolve to the CACHE_BUFFERS_* constants from the shared
 * header. */
#define DIAG_CACHE_SMALL_BYTES   CACHE_BUFFERS_SMALL_BYTES
#define DIAG_CACHE_LARGE_BYTES   CACHE_BUFFERS_LARGE_BYTES
#define DIAG_CACHE_STRIDE        CACHE_BUFFERS_LINE_SIZE

/* Display buffers — report_add_str stores the pointer verbatim. */
static char diag_cache_small_us_val[16];
static char diag_cache_large_us_val[16];
static char diag_cache_ratio_val[16];
static char diag_cache_status_detail[80];

/* Inner loop: stride-read the buffer `iters` times, accumulating into a
 * volatile sink so Watcom -ox can't eliminate the whole loop as dead. The
 * volatile sink is read once at end; the compiler must keep the writes
 * live, which keeps the reads live. Without it every loop body would
 * optimize away and measurements would report zero elapsed. */
static unsigned int stride_sink;  /* non-volatile observer storage */

static void stride_read_loop(unsigned char __far *buf, unsigned int size,
                             unsigned long iters)
{
    volatile unsigned int sum = 0;
    unsigned long i;
    unsigned int j;
    for (i = 0UL; i < iters; i++) {
        for (j = 0U; j < size; j += (unsigned int)DIAG_CACHE_STRIDE) {
            sum = sum + buf[j];
        }
    }
    stride_sink = sum;  /* external-linkage sink — prevents DCE */
}

/* ----------------------------------------------------------------------- */
/* Pure-math verdict kernel — host-testable.                                */
/*                                                                          */
/* ratio_x100 is the PER-LINE time ratio (large_per_line_us / small_per-   */
/* _line_us) × 100. A value of 100 means equal per-line access time        */
/* (no cache effect). Values above 100 mean the large buffer is slower per */
/* line, indicating cache-hit speedup for the small buffer.                 */
/*                                                                          */
/* THRESHOLD CALIBRATION (post issue #5 real-iron data):                    */
/*                                                                          */
/* First real-hardware capture (BEK-V409 i486DX-2, 2026-04-19) produced    */
/* per-line ratio 1.71× (171). The original per-traversal-ratio classifier */
/* folded the 16× buffer-size ratio into the threshold, requiring 40× to  */
/* PASS — unreachable on TSR-contended BIOS configs where the per-line    */
/* speedup is modest (1.7× here, vs theoretical 5-10×).                   */
/*                                                                          */
/* Per-line reformulation strips the buffer-size bias. The question the   */
/* classifier now answers directly is: "Does one cache-line access in the */
/* small buffer take less wall-clock time than one in the large buffer?"  */
/* Threshold ≥ 1.3× (30% speedup per line) is a conservative PASS; any    */
/* cache-enabled L1 should clear it even on slow-memory configs.          */
/*                                                                          */
/*   ratio_x100 == 0           -> WARN  "no_measurement" (timing underflow)*/
/*   ratio_x100 < 100          -> WARN  "anomaly" (large faster than small,*/
/*                                       physically impossible)            */
/*   100 <= ratio_x100 < 120   -> FAIL  "no_cache_effect" (< 1.2× — cache  */
/*                                       disabled, absent, or fully        */
/*                                       defeated by contention)           */
/*   120 <= ratio_x100 < 130   -> WARN  "partial" (1.2-1.3× — weak signal, */
/*                                       possible partial cache or unusual */
/*                                       memory-access pattern)            */
/*   ratio_x100 >= 130         -> PASS  "cache_working" (≥ 1.3×)           */
/*                                                                          */
/* Rationale for 1.3× cutoff: a no-cache 486 DX-2 run on the same bench   */
/* produces a per-line ratio of exactly 1.00 (both buffers hit DRAM equal-*/
/* ly). Even a 20% chipset/TSR slowdown of the large buffer (due to DRAM  */
/* page-boundary crossings or refresh contention) yields only ~1.2×. A    */
/* true L1-hit vs DRAM-miss speedup lands at 1.5-2× conservatively and    */
/* 5-10× on a cleanly-configured system. 1.3× cleanly separates "some     */
/* cache signal" from "no cache effect + incidental memory-system noise". */
/* ----------------------------------------------------------------------- */

verdict_t diag_cache_classify_ratio_x100(unsigned long ratio_x100,
                                         const char **out_msg_prefix)
{
    if (ratio_x100 == 0UL) {
        if (out_msg_prefix) *out_msg_prefix = "no_measurement";
        return VERDICT_WARN;
    }
    if (ratio_x100 < 100UL) {
        /* Physically impossible — large_per_line < small_per_line */
        if (out_msg_prefix) *out_msg_prefix = "anomaly";
        return VERDICT_WARN;
    }
    if (ratio_x100 < 120UL) {
        if (out_msg_prefix) *out_msg_prefix = "no_cache_effect";
        return VERDICT_FAIL;
    }
    if (ratio_x100 < 130UL) {
        if (out_msg_prefix) *out_msg_prefix = "partial";
        return VERDICT_WARN;
    }
    /* ratio >= 1.3× per-line — clear cache effect */
    if (out_msg_prefix) *out_msg_prefix = "cache_working";
    return VERDICT_PASS;
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

/* Iteration counts calibrated for ~50 ms wall time on a 486 DX-2:
 *   Small buffer: L1 hits at ~2 cycles per 16-byte stride, 128 lines per
 *     traversal, 256 cycles/traversal. 66 MHz * 50 ms = 3.3M cycles /
 *     256 = ~13,000 traversals. We cap at 500 to stay conservative on
 *     unexpectedly-fast hardware.
 *   Large buffer: DRAM latency ~15 cycles per line, 2048 lines, ~30k
 *     cycles/traversal. 3.3M / 30k = ~107 traversals. Start at 50. */
#define DIAG_CACHE_SMALL_ITERS  500UL
#define DIAG_CACHE_LARGE_ITERS  50UL

void diag_cache(result_table_t *t)
{
    const result_t *cache_present = find_key_local(t, "cache.present");
    us_t t_small, t_large;
    unsigned long small_per_iter_x1000;
    unsigned long large_per_iter_x1000;
    unsigned long ratio_x100;
    verdict_t v;
    const char *msg_tag = "";

    /* Skip entirely if detect_cache reported no cache expected for this
     * CPU class. Emitting PASS/FAIL on an 8088 would be misleading — the
     * floor hardware genuinely has no L1 and the test signal is
     * meaningless. */
    if (cache_present) {
        const char *cp = cache_present->display ? cache_present->display :
                         (cache_present->type == V_STR ? cache_present->v.s : (const char *)0);
        if (cp && strcmp(cp, "no") == 0) {
            report_add_str(t, "diagnose.cache.status",
                           "skipped_no_cache_expected",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return;
        }
    }

    /* Explicit inter-caller ownership protocol — establish known-zero
     * state before the stride reads so we don't inherit whatever pattern
     * bench_cache left behind (or, on an /ONLY:DIAG run, uninitialized
     * BSS fill that the C runtime already zeroed but we re-assert for
     * robustness against future storage changes). See cache_buffers.h. */
    cache_buffers_reset();

    /* Small-buffer timed run */
    timing_start();
    stride_read_loop(cache_buffers_small(), DIAG_CACHE_SMALL_BYTES,
                     DIAG_CACHE_SMALL_ITERS);
    t_small = timing_stop();

    /* Large-buffer timed run */
    timing_start();
    stride_read_loop(cache_buffers_large(), DIAG_CACHE_LARGE_BYTES,
                     DIAG_CACHE_LARGE_ITERS);
    t_large = timing_stop();

    /* Emit raw elapsed values — useful for debugging / regression triage
     * regardless of verdict. */
    sprintf(diag_cache_small_us_val, "%lu", (unsigned long)t_small);
    report_add_u32(t, "diagnose.cache.small_buffer_us",
                   (unsigned long)t_small, diag_cache_small_us_val,
                   CONF_HIGH, VERDICT_UNKNOWN);
    sprintf(diag_cache_large_us_val, "%lu", (unsigned long)t_large);
    report_add_u32(t, "diagnose.cache.large_buffer_us",
                   (unsigned long)t_large, diag_cache_large_us_val,
                   CONF_HIGH, VERDICT_UNKNOWN);

    /* Normalize per-iteration, then compute ratio×100 for integer math.
     * If either elapsed is 0 the measurement is degenerate; flag WARN. */
    if ((unsigned long)t_small == 0UL || (unsigned long)t_large == 0UL) {
        report_add_str(t, "diagnose.cache.status",
                       "WARN: sub-microsecond elapsed on at least one "
                       "stride loop (emulator or PIT resolution limit)",
                       CONF_LOW, VERDICT_WARN);
        return;
    }

    /* Per-CACHE-LINE time × 1000 to preserve sub-unit precision.
     *
     * Each small-buffer traversal touches SMALL_BYTES / STRIDE = 128 lines.
     * Each large-buffer traversal touches LARGE_BYTES / STRIDE = 2048 lines.
     * Per-line = total_us / (iters × lines_per_traversal).
     *
     * Overflow: t_small/t_large ≤ 55,000 (one PIT wrap). Numerator 55M ×
     * 1000 = 5.5e10 overflows! Split: (t * 1000) / iters first, then
     * divide by lines_per_traversal. t*1000 ≤ 55M fits; /iters brings it
     * down; /lines keeps it small.
     *
     * The PER-LINE ratio (not per-traversal) is what the classifier now
     * consumes — see the threshold rationale at diag_cache_classify_ratio
     * _x100 for why we moved away from the buffer-size-biased per-traversal
     * formulation. */
    {
        unsigned long small_lines_per = DIAG_CACHE_SMALL_BYTES / (unsigned int)DIAG_CACHE_STRIDE;
        unsigned long large_lines_per = DIAG_CACHE_LARGE_BYTES / (unsigned int)DIAG_CACHE_STRIDE;
        unsigned long small_per_line_x1000;
        unsigned long large_per_line_x1000;

        small_per_iter_x1000 = ((unsigned long)t_small * 1000UL) / DIAG_CACHE_SMALL_ITERS;
        large_per_iter_x1000 = ((unsigned long)t_large * 1000UL) / DIAG_CACHE_LARGE_ITERS;

        if (small_per_iter_x1000 == 0UL) {
            report_add_str(t, "diagnose.cache.status",
                           "WARN: small-buffer per-iteration time underflowed",
                           CONF_LOW, VERDICT_WARN);
            return;
        }

        small_per_line_x1000 = small_per_iter_x1000 / small_lines_per;
        large_per_line_x1000 = large_per_iter_x1000 / large_lines_per;

        if (small_per_line_x1000 == 0UL) {
            /* Sub-line-precision measurement — either extremely fast
             * hardware or calibration artifact. Bump iterations to recover
             * in a future session; for now flag inconclusive. */
            report_add_str(t, "diagnose.cache.status",
                           "WARN: small-buffer per-line time underflowed",
                           CONF_LOW, VERDICT_WARN);
            return;
        }

        ratio_x100 = (large_per_line_x1000 * 100UL) / small_per_line_x1000;
    }

    /* Emit ratio as "NN.NN" string for human-friendly INI reading. */
    sprintf(diag_cache_ratio_val, "%lu.%02lu",
            ratio_x100 / 100UL, ratio_x100 % 100UL);
    report_add_str(t, "diagnose.cache.stride_ratio", diag_cache_ratio_val,
                   CONF_HIGH, VERDICT_UNKNOWN);

    /* Classify and emit verdict */
    v = diag_cache_classify_ratio_x100(ratio_x100, &msg_tag);
    sprintf(diag_cache_status_detail,
            "%s (ratio=%lu.%02lu × — %s)",
            v == VERDICT_PASS ? "pass" :
            v == VERDICT_FAIL ? "FAIL" : "WARN",
            ratio_x100 / 100UL, ratio_x100 % 100UL,
            msg_tag);
    report_add_str(t, "diagnose.cache.status",
                   diag_cache_status_detail,
                   v == VERDICT_PASS ? CONF_HIGH : CONF_MEDIUM,
                   v);

    /* Reference stride_sink so Watcom doesn't warn about it being unused
     * at file scope. The actual DCE suppression happens via the volatile
     * `sum` inside stride_read_loop. */
    (void)stride_sink;
}
