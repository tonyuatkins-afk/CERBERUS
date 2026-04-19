/*
 * Host-side test for diag_dma's pure-math summary-verdict kernel. The
 * per-channel probe (I/O port writes) is hardware-only and not host-
 * testable — we exercise only the kernel that maps (pass-count,
 * fail-count, skip-count) to a summary verdict.
 *
 * Summary-verdict semantics:
 *   probed == 0                         -> VERDICT_UNKNOWN (rule not applicable)
 *   ch_fail == 0 (all probed passed)    -> VERDICT_PASS
 *   ch_pass == 0 (all probed failed)    -> VERDICT_FAIL
 *   mixed (some pass, some fail)        -> VERDICT_WARN
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/diag/diag_dma.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

int main(void)
{
    printf("=== CERBERUS host unit test: diag_dma summary kernel ===\n");

    /* Scenario A: no probed channels (no CPU class detected or /ONLY:DET) — no-op */
    CHECK(diag_dma_summary_verdict(0, 0, 2) == VERDICT_UNKNOWN,
          "Scenario A: no probed channels -> UNKNOWN (no-op)");

    /* Scenario B: AT-class healthy — 6 probed pass + 2 skipped (ch0 refresh, ch4 cascade) */
    CHECK(diag_dma_summary_verdict(6, 0, 2) == VERDICT_PASS,
          "Scenario B: AT-class 6/6 pass + 2 safety-skip -> PASS");

    /* Scenario C: XT-class healthy — 3 probed pass, 5 skipped (ch0 + ch4 + ch5/6/7 no-slave) */
    CHECK(diag_dma_summary_verdict(3, 0, 5) == VERDICT_PASS,
          "Scenario C: XT-class 3/3 pass + 5 skip -> PASS");

    /* Scenario D: AT-class, one channel dead — 5 pass, 1 fail -> WARN */
    CHECK(diag_dma_summary_verdict(5, 1, 2) == VERDICT_WARN,
          "Scenario D: 5 pass + 1 fail -> WARN (partial controller damage)");

    /* Scenario E: AT-class, slave dead entirely — 3 master pass, 3 slave fail -> WARN */
    CHECK(diag_dma_summary_verdict(3, 3, 2) == VERDICT_WARN,
          "Scenario E: 3 pass + 3 fail -> WARN (slave dead)");

    /* Scenario F: controller totally dead — 0 pass, 6 fail -> FAIL */
    CHECK(diag_dma_summary_verdict(0, 6, 2) == VERDICT_FAIL,
          "Scenario F: 0 pass + 6 fail -> FAIL (both controllers dead)");

    /* Scenario G: XT-class master dead — 0 pass, 3 fail -> FAIL */
    CHECK(diag_dma_summary_verdict(0, 3, 5) == VERDICT_FAIL,
          "Scenario G: XT-class 0 pass + 3 fail -> FAIL");

    /* Scenario H: single pass + single fail -> WARN (boundary) */
    CHECK(diag_dma_summary_verdict(1, 1, 0) == VERDICT_WARN,
          "Scenario H: 1 pass + 1 fail -> WARN");

    /* Scenario I: single pass only -> PASS */
    CHECK(diag_dma_summary_verdict(1, 0, 0) == VERDICT_PASS,
          "Scenario I: single pass -> PASS");

    /* Scenario J: single fail only -> FAIL */
    CHECK(diag_dma_summary_verdict(0, 1, 0) == VERDICT_FAIL,
          "Scenario J: single fail -> FAIL");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
