/*
 * Host-side unit test for the timing module's pure-math helpers.
 * Compiled with -DCERBERUS_HOST_TEST so the hardware-specific code is
 * excluded and we don't drag in <conio.h>.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif
#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/core/timing.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define EXPECT_EQ_UL(actual, expected, label)                                 \
    do {                                                                      \
        unsigned long a_ = (unsigned long)(actual);                           \
        unsigned long e_ = (unsigned long)(expected);                         \
        if (a_ == e_) {                                                       \
            printf("  OK   %s: %lu\n", (label), a_);                          \
        } else {                                                              \
            printf("  FAIL %s: got %lu expected %lu\n", (label), a_, e_);     \
            failures++;                                                       \
        }                                                                     \
    } while (0)

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { printf("  OK   %s\n", (msg)); }                           \
        else      { printf("  FAIL %s\n", (msg)); failures++; }               \
    } while (0)

static const result_t *lookup_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/*
 * NOTE: Host tests use wcl386 which has 32-bit int. The bug class these
 * tests can NOT catch is "ticks * 838" without the UL suffix — on the
 * real-mode DOS target, 16-bit int multiplication overflows at ticks>=78.
 * Guard against regression via code review, not this test. The presence
 * of the UL suffix in timing_ticks_to_us is load-bearing and must not
 * be removed. Rule 4a (timing_independence) is the runtime backstop.
 */
static void test_ticks_to_us(void)
{
    printf("timing_ticks_to_us corner cases:\n");
    /* Formula: (ticks * 838 + 500) / 1000
     * These expected values are computed by the formula directly, not by
     * the "true" 838.095 ns value — the 0.01% systematic error is accepted
     * per the plan's methodology note. */
    EXPECT_EQ_UL(timing_ticks_to_us(0UL),         0UL,       "0 ticks");
    EXPECT_EQ_UL(timing_ticks_to_us(1UL),         1UL,       "1 tick");         /* (838+500)/1000 */
    EXPECT_EQ_UL(timing_ticks_to_us(100UL),       84UL,      "100 ticks");      /* (83800+500)/1000 = 84 */
    EXPECT_EQ_UL(timing_ticks_to_us(1000UL),      838UL,     "1000 ticks");     /* (838000+500)/1000 */
    EXPECT_EQ_UL(timing_ticks_to_us(65535UL),     54918UL,   "65535 ticks");    /* (54918330+500)/1000 = 54918 */
    EXPECT_EQ_UL(timing_ticks_to_us(1000000UL),   838000UL,  "1,000,000 ticks");
}

static void test_elapsed_ticks(void)
{
    printf("timing_elapsed_ticks (rollover math) — BOTH branches:\n");

    /* Normal case — counter went DOWN (start > stop) */
    EXPECT_EQ_UL(timing_elapsed_ticks(0xFFFF, 0x8000), 0x7FFFUL, "start=FFFF stop=8000 (no wrap)");
    EXPECT_EQ_UL(timing_elapsed_ticks(0xFFFF, 0xFFFE), 0x0001UL, "start=FFFF stop=FFFE (1 tick)");
    EXPECT_EQ_UL(timing_elapsed_ticks(0x8000, 0x0000), 0x8000UL, "start=8000 stop=0000 (halfway)");

    /* Wrap case — counter went down past zero and reloaded (stop > start) */
    EXPECT_EQ_UL(timing_elapsed_ticks(0x1000, 0xE000), 0x3000UL, "start=1000 stop=E000 (wrapped)");
    EXPECT_EQ_UL(timing_elapsed_ticks(0x0001, 0x0002), 0xFFFFUL, "start=0001 stop=0002 (max wrap)");
    EXPECT_EQ_UL(timing_elapsed_ticks(0x0000, 0xFFFF), 0x0001UL, "start=0000 stop=FFFF (min wrap)");

    /* Zero-elapsed edge case */
    EXPECT_EQ_UL(timing_elapsed_ticks(0x1234, 0x1234), 0UL,      "start=stop (zero elapsed)");
}

static void test_bios_ticks_to_us(void)
{
    printf("timing_bios_ticks_to_us (54925 us/tick):\n");
    EXPECT_EQ_UL(timing_bios_ticks_to_us(0UL),      0UL,        "0 bios ticks");
    EXPECT_EQ_UL(timing_bios_ticks_to_us(1UL),      54925UL,    "1 bios tick");
    EXPECT_EQ_UL(timing_bios_ticks_to_us(4UL),      219700UL,   "4 bios ticks (target interval)");
    EXPECT_EQ_UL(timing_bios_ticks_to_us(18UL),     988650UL,   "18 bios ticks (~1 sec)");
    /* Sanity — stay under the 32-bit overflow boundary. */
    EXPECT_EQ_UL(timing_bios_ticks_to_us(1000UL),   54925000UL, "1000 bios ticks (~55 sec)");

    /* Overflow boundary probe: 2^16 = 65536 is a comfortable round
     * number that exercises the high end of any plausible caller.
     * Expected = 65536 * 54925 = 3,599,564,800 (fits 32-bit unsigned).
     * The helper is safe up to ~78000 ticks (~71 minutes of real time),
     * well beyond any plausible call site — any larger value would
     * approach the 32-bit unsigned long limit of 4,294,967,295 and
     * produce wrap-around. Document-only: no caller ever generates a
     * bios-tick delta anywhere near this. */
    EXPECT_EQ_UL(timing_bios_ticks_to_us(65536UL),  3599564800UL,
                 "65536 bios ticks (2^16 boundary probe, ~60 min)");
    /* Last fully-safe round-ish value: 78000 * 54925 = 4,284,150,000
     * which still fits. Anything above ~78196 starts approaching the
     * 32-bit limit. */
    EXPECT_EQ_UL(timing_bios_ticks_to_us(78000UL),  4284150000UL,
                 "78000 bios ticks (near the upper safe boundary)");
}

/* ----------------------------------------------------------------------- */
/* timing_compute_dual — pure-math kernel extracted from timing_dual_measure */
/* ----------------------------------------------------------------------- */

static void test_compute_dual(void)
{
    us_t pit_us, bios_us;
    int  rc;

    printf("timing_compute_dual (pure math kernel):\n");

    /* Success case: 4 BIOS ticks, 4 wraps, small sub-tick delta.
     * Target is the timing_self_check call site. initial_c2=0xE000,
     * final_c2=0xD000 → sub_ticks = 0x1000 = 4096.
     * c2_total = 4 * 65536 + 4096 = 266240
     * pit_us = (266240 * 838 + 500) / 1000 = 223109
     * bios_us = 4 * 54925 = 219700 */
    pit_us = 0; bios_us = 0;
    rc = timing_compute_dual(0xE000, 0xD000, 4UL, 4UL, 4, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 0UL, "success: rc=0");
    EXPECT_EQ_UL(pit_us,  223109UL, "success: pit_us");
    EXPECT_EQ_UL(bios_us, 219700UL, "success: bios_us");

    /* Sanity-check bail: target=4, observed only 2 wraps.
     * c2_wraps + 1 = 3 < 4 → return 1 and leave outputs untouched. */
    pit_us = 0xDEADUL; bios_us = 0xBEEFUL;
    rc = timing_compute_dual(0xFFFF, 0x0001, 2UL, 4UL, 4, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 1UL, "sanity-bail: rc=1 when wraps too low");
    EXPECT_EQ_UL(pit_us,  0xDEADUL, "sanity-bail: pit_us untouched");
    EXPECT_EQ_UL(bios_us, 0xBEEFUL, "sanity-bail: bios_us untouched");

    /* Exactly on the +1 tolerance line: target=4, 3 wraps.
     * c2_wraps + 1 = 4 NOT < 4 → passes the check. */
    pit_us = 0; bios_us = 0;
    rc = timing_compute_dual(0x8000, 0x7000, 3UL, 4UL, 4, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 0UL, "tolerance-edge: rc=0 at exactly target-1 wraps");

    /* Defensive missed-wrap branch: final_c2 > initial_c2, meaning the
     * loop exited between a wrap event and the next sample. The kernel
     * promotes c2_wraps by 1 and uses the 65536+initial-final formula.
     * initial_c2=0x2000, final_c2=0xF000, c2_wraps_observed=3, target=4.
     * After promotion c2_wraps=4, sub_ticks = 65536 + 0x2000 - 0xF000
     * = 65536 + 8192 - 61440 = 12288.
     * c2_total = 4 * 65536 + 12288 = 274432
     * pit_us = (274432 * 838 + 500) / 1000 = 229974 */
    pit_us = 0; bios_us = 0;
    rc = timing_compute_dual(0x2000, 0xF000, 3UL, 4UL, 4, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 0UL, "missed-wrap: rc=0 (defensive branch)");
    EXPECT_EQ_UL(pit_us, 229974UL, "missed-wrap: pit_us includes extra 65536");

    /* target_bios_ticks=1 edge: should NOT bail even with 0 wraps.
     * c2_wraps + 1 = 1 NOT < 1. */
    pit_us = 0; bios_us = 0;
    rc = timing_compute_dual(0xF000, 0x8000, 0UL, 1UL, 1, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 0UL, "target=1: rc=0 even with 0 wraps");
    /* sub_ticks = 0xF000 - 0x8000 = 0x7000 = 28672.
     * pit_us = (28672 * 838 + 500) / 1000 = 24027
     * bios_us = 1 * 54925 = 54925 */
    EXPECT_EQ_UL(pit_us,  24027UL, "target=1: pit_us");
    EXPECT_EQ_UL(bios_us, 54925UL, "target=1: bios_us");

    /* target_bios_ticks=0 is treated as 1 by the kernel (defensive —
     * the HW layer has the same clamp at the top of timing_dual_measure
     * but the pure kernel enforces it too so it's safe to call directly). */
    pit_us = 0; bios_us = 0;
    rc = timing_compute_dual(0xE000, 0xD000, 0UL, 1UL, 0, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 0UL, "target=0 treated as 1: rc=0");

    /* NULL output pointers are tolerated. */
    rc = timing_compute_dual(0xE000, 0xD000, 4UL, 4UL, 4,
                             (us_t *)0, (us_t *)0);
    EXPECT_EQ_UL(rc, 0UL, "NULL outs: rc=0, no crash");

    /* Upper wrap-count sanity (defense-in-depth for direct callers).
     * The HW layer already bails at c2_wraps>8; this independent cap at
     * >16 in the pure kernel protects host tests and any future refactor
     * that hands us a pathological wrap count. 100 is a clearly-bogus
     * value (would represent ~5.5 seconds of measurement with a 220ms
     * target). Outputs must be left untouched. */
    pit_us = 0xFEEDUL; bios_us = 0xFACEUL;
    rc = timing_compute_dual(0xE000, 0xD000, 100UL, 4UL, 4, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 1UL, "upper-bail: rc=1 when wraps=100 (pathological)");
    EXPECT_EQ_UL(pit_us,  0xFEEDUL, "upper-bail: pit_us untouched");
    EXPECT_EQ_UL(bios_us, 0xFACEUL, "upper-bail: bios_us untouched");

    /* Boundary: wraps=16 passes (still sane), wraps=17 bails. */
    pit_us = 0; bios_us = 0;
    rc = timing_compute_dual(0xE000, 0xD000, 16UL, 16UL, 16, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 0UL, "upper-bail edge: wraps=16 still passes");
    pit_us = 0xAAAAUL; bios_us = 0xBBBBUL;
    rc = timing_compute_dual(0xE000, 0xD000, 17UL, 16UL, 16, &pit_us, &bios_us);
    EXPECT_EQ_UL(rc, 1UL, "upper-bail edge: wraps=17 bails");
}

/* ----------------------------------------------------------------------- */
/* timing_emit_self_check — pure emit helper used by timing_self_check     */
/* ----------------------------------------------------------------------- */

static void test_emit_self_check(void)
{
    result_table_t t;
    const result_t *r;

    printf("timing_emit_self_check (status emit helper):\n");

    /* Case 1: success (rc=0, non-zero values) -> "ok" + numeric rows +
     * NO consistency.timing_self_check row. */
    memset(&t, 0, sizeof(t));
    timing_emit_self_check(&t, 0, (us_t)220000UL, (us_t)219700UL);
    r = lookup_key(&t, "timing.cross_check.status");
    CHECK(r != NULL && r->type == V_STR && strcmp(r->v.s, "ok") == 0,
          "success: status row = \"ok\"");
    r = lookup_key(&t, "timing.cross_check.pit_us");
    CHECK(r != NULL && r->type == V_U32 && r->v.u == 220000UL,
          "success: pit_us row present with numeric value");
    r = lookup_key(&t, "timing.cross_check.bios_us");
    CHECK(r != NULL && r->type == V_U32 && r->v.u == 219700UL,
          "success: bios_us row present with numeric value");
    r = lookup_key(&t, "consistency.timing_self_check");
    CHECK(r == NULL,
          "success: no consistency.timing_self_check row emitted");

    /* Case 2: failure (rc=1, outputs zero) -> "measurement_failed" +
     * consistency.timing_self_check WARN row + no numeric rows. */
    memset(&t, 0, sizeof(t));
    timing_emit_self_check(&t, 1, (us_t)0UL, (us_t)0UL);
    r = lookup_key(&t, "timing.cross_check.status");
    /* Status key carries VERDICT_UNKNOWN — it's purely informational
     * (no UI renderer surfaces timing.cross_check.* rows). The
     * user-visible WARN lives on the consistency.timing_self_check
     * row below, which the TUI alert renderer picks up. */
    CHECK(r != NULL && r->type == V_STR &&
              strcmp(r->v.s, "measurement_failed") == 0 &&
              r->verdict == VERDICT_UNKNOWN,
          "failure: status = \"measurement_failed\" UNKNOWN");
    r = lookup_key(&t, "consistency.timing_self_check");
    CHECK(r != NULL && r->verdict == VERDICT_WARN,
          "failure: consistency.timing_self_check WARN row emitted");
    r = lookup_key(&t, "timing.cross_check.pit_us");
    CHECK(r == NULL, "failure: pit_us row NOT emitted");
    r = lookup_key(&t, "timing.cross_check.bios_us");
    CHECK(r == NULL, "failure: bios_us row NOT emitted");

    /* Case 3: defensive — rc=0 but one or both us_t outputs are zero.
     * Should still take the failure path (downstream cannot trust a
     * zero-us result even if dual_measure claimed success). */
    memset(&t, 0, sizeof(t));
    timing_emit_self_check(&t, 0, (us_t)0UL, (us_t)0UL);
    r = lookup_key(&t, "timing.cross_check.status");
    CHECK(r != NULL && r->type == V_STR &&
              strcmp(r->v.s, "measurement_failed") == 0,
          "defensive: rc=0 + zero outputs -> measurement_failed");
    r = lookup_key(&t, "consistency.timing_self_check");
    CHECK(r != NULL && r->verdict == VERDICT_WARN,
          "defensive: consistency.timing_self_check WARN row emitted");
    r = lookup_key(&t, "timing.cross_check.pit_us");
    CHECK(r == NULL, "defensive: pit_us row NOT emitted");

    /* Case 4: partial — rc=0, non-zero pit_us, but bios_us==0. A real
     * dual_measure path can't produce this on its own, but a future
     * refactor could accidentally succeed at one half of the dual
     * measurement and leave the other half zero. The emit helper
     * must still route to the failure path to avoid a nonsense
     * rule-4a comparison against bios_us=0. */
    memset(&t, 0, sizeof(t));
    timing_emit_self_check(&t, 0, (us_t)220000UL, (us_t)0UL);
    r = lookup_key(&t, "timing.cross_check.status");
    CHECK(r != NULL && r->type == V_STR &&
              strcmp(r->v.s, "measurement_failed") == 0,
          "partial pit-only: status = \"measurement_failed\"");
    r = lookup_key(&t, "consistency.timing_self_check");
    CHECK(r != NULL && r->verdict == VERDICT_WARN,
          "partial pit-only: consistency row WARN emitted");
    r = lookup_key(&t, "timing.cross_check.pit_us");
    CHECK(r == NULL, "partial pit-only: pit_us row NOT emitted");
    r = lookup_key(&t, "timing.cross_check.bios_us");
    CHECK(r == NULL, "partial pit-only: bios_us row NOT emitted");

    /* Case 5: mirror — rc=0, bios_us populated, pit_us zero. Same
     * expected routing as Case 4. */
    memset(&t, 0, sizeof(t));
    timing_emit_self_check(&t, 0, (us_t)0UL, (us_t)220000UL);
    r = lookup_key(&t, "timing.cross_check.status");
    CHECK(r != NULL && r->type == V_STR &&
              strcmp(r->v.s, "measurement_failed") == 0,
          "partial bios-only: status = \"measurement_failed\"");
    r = lookup_key(&t, "consistency.timing_self_check");
    CHECK(r != NULL && r->verdict == VERDICT_WARN,
          "partial bios-only: consistency row WARN emitted");
    r = lookup_key(&t, "timing.cross_check.pit_us");
    CHECK(r == NULL, "partial bios-only: pit_us row NOT emitted");
    r = lookup_key(&t, "timing.cross_check.bios_us");
    CHECK(r == NULL, "partial bios-only: bios_us row NOT emitted");
}

int main(void)
{
    printf("=== CERBERUS host unit test: timing ===\n");
    test_ticks_to_us();
    test_elapsed_ticks();
    test_bios_ticks_to_us();
    test_compute_dual();
    test_emit_self_check();
    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
