/*
 * Host-side test for the Mann-Kendall thermal stability tracker.
 * Synthesize per-pass series representing steady / drifting / warming
 * machines and verify each produces the right verdict.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/core/thermal.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

static const result_t *k(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

static verdict_t v_of(const result_t *r) { return r ? r->verdict : VERDICT_UNKNOWN; }

/* Static buffer storage so the key/value pointers we store in the
 * result table remain valid for the duration of the test. (Same
 * lifetime contract the real code follows — stack-local sprintf
 * targets would dangle.) */
static char test_keys[16][64];
static char test_vals[16][32];

static void add_series(result_table_t *t, const unsigned long *xs, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        sprintf(test_keys[i], "bench.cpu.int_pass_%d_us", i);
        sprintf(test_vals[i], "%lu", xs[i]);
        report_add_u32(t, test_keys[i], xs[i], test_vals[i],
                       CONF_HIGH, VERDICT_UNKNOWN);
    }
}

int main(void)
{
    result_table_t t;
    printf("=== CERBERUS host unit test: thermal stability (Mann-Kendall) ===\n");

    /* Scenario 1: Perfectly stable series — S = 0, pass */
    {
        static const unsigned long x[] = { 1000, 1000, 1000, 1000, 1000, 1000, 1000 };
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        CHECK(v_of(k(&t, "thermal.cpu")) == VERDICT_PASS,
              "Flat series N=7 → PASS");
    }

    /* Scenario 2: Pure noise, non-monotonic — |S| < critical, pass */
    {
        static const unsigned long x[] = { 1000, 1020, 990, 1010, 995, 1005, 1000 };
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        CHECK(v_of(k(&t, "thermal.cpu")) == VERDICT_PASS,
              "Noisy non-monotonic series → PASS");
    }

    /* Scenario 3: Strong upward trend — S = +21 for N=7 strictly
     * increasing, above critical 17, FAIL/WARN (thermal degradation) */
    {
        static const unsigned long x[] = { 1000, 1010, 1020, 1030, 1040, 1050, 1060 };
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        CHECK(v_of(k(&t, "thermal.cpu")) == VERDICT_WARN,
              "Strict ascending series N=7 → WARN (thermal)");
    }

    /* Scenario 4: Strong downward trend — cold-start warmup, benign PASS */
    {
        static const unsigned long x[] = { 1100, 1080, 1060, 1045, 1030, 1020, 1015 };
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        CHECK(v_of(k(&t, "thermal.cpu")) == VERDICT_PASS,
              "Strict descending (warmup) → PASS");
    }

    /* Scenario 5: Insufficient samples — no emit (rule not applicable) */
    {
        static const unsigned long x[] = { 1000, 1020, 1040 };
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 3);
        thermal_check(&t);
        CHECK(k(&t, "thermal.cpu") == NULL,
              "N=3 (below MIN_SERIES=5) → no emit");
    }

    /* Scenario 6: Minimum workable N=5 with upward trend */
    {
        static const unsigned long x[] = { 1000, 1020, 1040, 1060, 1080 };
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 5);
        thermal_check(&t);
        /* S = 10 for N=5 strictly increasing = exactly the critical
         * value, so the test's |S| < critical is false — WARN. */
        CHECK(v_of(k(&t, "thermal.cpu")) == VERDICT_WARN,
              "N=5 strict ascending → WARN");
    }

    /* Scenario 7: Quick-mode (no per-pass series emitted) */
    {
        memset(&t, 0, sizeof(t));
        thermal_check(&t);
        CHECK(k(&t, "thermal.cpu") == NULL,
              "No per-pass data → no emit");
    }

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
