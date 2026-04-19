/*
 * Host-side test for diag_cache's pure-math verdict kernel. The timing
 * path itself is hardware-dependent (PIT-measured on real iron / DOSBox-X)
 * and is not host-testable — we exercise only the classify-the-ratio
 * kernel that decides PASS / WARN / FAIL from the stride-ratio input.
 *
 * Verdict table (ratio in ×100 units so 200 = 2.00×):
 *   ratio == 0          -> VERDICT_WARN  "no_measurement"
 *   ratio < 100         -> VERDICT_WARN  "anomaly"         (< 1.0×, physically impossible)
 *   ratio < 2000        -> VERDICT_FAIL  "no_cache_effect" (< 20×)
 *   ratio < 4000        -> VERDICT_WARN  "partial"         (20-40×)
 *   ratio >= 4000       -> VERDICT_PASS  "cache_working"   (≥ 40×)
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"

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

    /* Scenario B: ratio=0.5× (impossible — large faster than small) -> WARN anomaly */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(50UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario B: ratio=0.5× -> WARN");
    CHECK(tag != NULL && strcmp(tag, "anomaly") == 0,
          "Scenario B: tag == anomaly");

    /* Scenario C: ratio=1.0× (no cache effect at all) -> FAIL */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(100UL, &tag);
    CHECK(v == VERDICT_FAIL, "Scenario C: ratio=1.0× -> FAIL");
    CHECK(tag != NULL && strcmp(tag, "no_cache_effect") == 0,
          "Scenario C: tag == no_cache_effect");

    /* Scenario D: ratio=15× (linear buffer-size scaling, cache off) -> FAIL */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(1500UL, &tag);
    CHECK(v == VERDICT_FAIL, "Scenario D: ratio=15× (buffer scaling) -> FAIL");

    /* Scenario E: ratio=19.99× (just below FAIL threshold) -> FAIL */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(1999UL, &tag);
    CHECK(v == VERDICT_FAIL, "Scenario E: ratio=19.99× -> FAIL (boundary)");

    /* Scenario F: ratio=20× (right at WARN boundary) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(2000UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario F: ratio=20× -> WARN (boundary)");
    CHECK(tag != NULL && strcmp(tag, "partial") == 0,
          "Scenario F: tag == partial");

    /* Scenario G: ratio=30× (partial cache effect) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(3000UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario G: ratio=30× -> WARN");

    /* Scenario H: ratio=39.99× (just below PASS threshold) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(3999UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario H: ratio=39.99× -> WARN (boundary)");

    /* Scenario I: ratio=40× (right at PASS boundary) -> PASS */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(4000UL, &tag);
    CHECK(v == VERDICT_PASS, "Scenario I: ratio=40× -> PASS (boundary)");
    CHECK(tag != NULL && strcmp(tag, "cache_working") == 0,
          "Scenario I: tag == cache_working");

    /* Scenario J: ratio=120× (typical healthy 486 DX-2 with 8 KB L1) -> PASS */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(12000UL, &tag);
    CHECK(v == VERDICT_PASS, "Scenario J: ratio=120× (healthy 486) -> PASS");

    /* Scenario K: ratio=500× (unusually large cache effect — maybe L2 amplifies) -> PASS */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(50000UL, &tag);
    CHECK(v == VERDICT_PASS, "Scenario K: ratio=500× -> PASS");

    /* Scenario L: ratio=99 (just under anomaly threshold, < 1.0×) -> WARN */
    tag = NULL;
    v = diag_cache_classify_ratio_x100(99UL, &tag);
    CHECK(v == VERDICT_WARN, "Scenario L: ratio=0.99× -> WARN (anomaly)");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
