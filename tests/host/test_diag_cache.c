/*
 * Host-side test for diag_cache's pure-math verdict kernel. Semantics
 * per issue #5 resolution: the classifier now consumes a PER-LINE time
 * ratio rather than a per-traversal ratio. See diag_cache.c's comment
 * block above diag_cache_classify_ratio_x100 for the threshold
 * calibration rationale based on first-real-iron data (BEK-V409 at
 * 2026-04-19 showed per-line ratio 1.71×, correctly PASSing under the
 * recalibrated thresholds).
 *
 * Verdict table (ratio in ×100 units so 130 = 1.30×):
 *   ratio == 0          -> VERDICT_WARN  "no_measurement"
 *   ratio < 100         -> VERDICT_WARN  "anomaly"         (< 1.0×, physically impossible)
 *   ratio < 120         -> VERDICT_FAIL  "no_cache_effect" (< 1.2×)
 *   ratio < 130         -> VERDICT_WARN  "partial"         (1.2-1.3×)
 *   ratio >= 130        -> VERDICT_PASS  "cache_working"   (≥ 1.3×)
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/core/cache_buffers.c"

/* Stub the hardware-touching timing primitives that diag_cache.c calls.
 * The real diag_cache() path uses timing_start / timing_stop for the
 * stride-loop wall-clock measurement; host tests never exercise that
 * path, only the pure-math classifier kernel. Providing stubs here lets
 * Watcom link the TU without dragging in core/timing.c (which pokes I/O
 * ports that don't exist in a Win32 host). */
#include "../../src/core/timing.h"
void timing_start(void) { /* no-op stub */ }
us_t timing_stop(void)  { return (us_t)0; }

#include "../../src/diag/diag_cache.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

int main(void)
{
    const char *tag;
    verdict_t v;
    printf("=== CERBERUS host unit test: diag_cache classifier ===\n");

    /* Scenario A: ratio=0 (timing measurement failed) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(0UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario A: ratio=0 -> WARN");
    CHECK(tag != NULL && strcmp(tag, "no_measurement") == 0,
          "Scenario A: tag == no_measurement");

    /* Scenario B: ratio=0.5× (impossible — large faster than small per line) */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(50UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario B: ratio=0.5× -> WARN");
    CHECK(tag != NULL && strcmp(tag, "anomaly") == 0,
          "Scenario B: tag == anomaly");

    /* Scenario C: ratio=1.00× (equal per-line — no cache effect) -> FAIL */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(100UL, &tag);
    CHECK(v == VERDICT_FAIL, "Scenario C: ratio=1.00× -> FAIL");
    CHECK(tag != NULL && strcmp(tag, "no_cache_effect") == 0,
          "Scenario C: tag == no_cache_effect");

    /* Scenario D: ratio=1.10× (marginal, still FAIL-band — noise floor) */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(110UL, &tag);
    CHECK(v == VERDICT_FAIL, "Scenario D: ratio=1.10× -> FAIL (noise floor)");

    /* Scenario E: ratio=1.19× (just below FAIL boundary) -> FAIL */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(119UL, &tag);
    CHECK(v == VERDICT_FAIL, "Scenario E: ratio=1.19× -> FAIL (boundary)");

    /* Scenario F: ratio=1.20× (at WARN boundary) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(120UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario F: ratio=1.20× -> WARN (boundary)");
    CHECK(tag != NULL && strcmp(tag, "partial") == 0,
          "Scenario F: tag == partial");

    /* Scenario G: ratio=1.25× (partial, mid-WARN) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(125UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario G: ratio=1.25× -> WARN");

    /* Scenario H: ratio=1.29× (just below PASS boundary) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(129UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario H: ratio=1.29× -> WARN (boundary)");

    /* Scenario I: ratio=1.30× (PASS boundary) -> PASS */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(130UL, &tag);
    CHECK(v == VERDICT_PASS, "Scenario I: ratio=1.30× -> PASS (boundary)");
    CHECK(tag != NULL && strcmp(tag, "cache_working") == 0,
          "Scenario I: tag == cache_working");

    /* Scenario J: ratio=1.71× — first real-iron data point on BEK-V409
     * i486DX-2 (2026-04-19 Sunday validation). Was the direct motivator
     * for the threshold recalibration; MUST PASS under the corrected
     * semantics. If this scenario regresses, issue #5's fix has been
     * undone. */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(171UL, &tag);
    CHECK(v == VERDICT_PASS, "Scenario J: ratio=1.71× (BEK-V409 real-iron) -> PASS");
    CHECK(tag != NULL && strcmp(tag, "cache_working") == 0,
          "Scenario J: tag == cache_working");

    /* Scenario K: ratio=5.00× (well-optimized system — 5× per-line speedup) */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(500UL, &tag);
    CHECK(v == VERDICT_PASS, "Scenario K: ratio=5.00× -> PASS (fast system)");

    /* Scenario L: ratio=10.00× (theoretical upper bound for L1-only speedup) */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(1000UL, &tag);
    CHECK(v == VERDICT_PASS, "Scenario L: ratio=10.00× -> PASS (theoretical upper)");

    /* Scenario M: ratio=99 (just under anomaly threshold, < 1.0×) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(99UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario M: ratio=0.99× -> WARN (anomaly)");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
