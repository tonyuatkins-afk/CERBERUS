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

    /* Scenario 8: thermal.cpu.direction emitted with correct label.
     * A strictly ascending series has S > 0 → direction "up".
     * (The direction key is informational and carries VERDICT_UNKNOWN;
     * it coexists with the PASS/WARN verdict on thermal.cpu so a
     * reviewer scanning the INI can see cold-start warmup vs thermal
     * degradation even when the verdict is PASS.) */
    {
        static const unsigned long x[] = { 1000, 1010, 1020, 1030, 1040, 1050, 1060 };
        const result_t *dir;
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        dir = k(&t, "thermal.cpu.direction");
        CHECK(dir != NULL && strcmp(dir->v.s, "up") == 0,
              "Ascending → direction=\"up\"");
    }

    /* Scenario 9: direction "down" on descending series (cold-start
     * warmup — verdict is still PASS, direction exposes the context). */
    {
        static const unsigned long x[] = { 1100, 1080, 1060, 1045, 1030, 1020, 1015 };
        const result_t *dir;
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        dir = k(&t, "thermal.cpu.direction");
        CHECK(dir != NULL && strcmp(dir->v.s, "down") == 0,
              "Descending → direction=\"down\"");
    }

    /* Scenario 10: direction "flat" on perfectly-steady series (S=0). */
    {
        static const unsigned long x[] = { 1000, 1000, 1000, 1000, 1000, 1000, 1000 };
        const result_t *dir;
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        dir = k(&t, "thermal.cpu.direction");
        CHECK(dir != NULL && strcmp(dir->v.s, "flat") == 0,
              "Flat → direction=\"flat\"");
    }

    /* Scenario 11: negative-S stringification. A strictly descending
     * series produces S < 0, which analyze_cpu_series formats with
     * sprintf("%d", s) into thermal_cpu_s_val. Verify the stored
     * display string for thermal.cpu.s starts with '-' — guards against
     * a future refactor that accidentally uses %u and silently strips
     * the sign (making warmup and degradation indistinguishable by
     * numeric read). */
    {
        static const unsigned long x[] = { 1100, 1080, 1060, 1045, 1030, 1020, 1015 };
        const result_t *r;
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        r = k(&t, "thermal.cpu.s");
        CHECK(r != NULL && r->v.s != NULL && r->v.s[0] == '-',
              "Descending series: thermal.cpu.s string starts with '-'");
    }

    /* Scenario 12: numeric-S exact-value assertion for strict-descending
     * N=7. All C(7,2)=21 pair comparisons yield -1, so S = -21. This is
     * a belt-and-suspenders guard against sign or arithmetic drift in
     * the Mann-Kendall S accumulator — the S-starts-with-minus check
     * above would still pass if, e.g., a refactor inverted the sign of
     * the increments and emitted "-14" instead of "-21". */
    {
        static const unsigned long x[] = { 1100, 1080, 1060, 1045, 1030, 1020, 1015 };
        const result_t *r;
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        r = k(&t, "thermal.cpu.s");
        CHECK(r != NULL && r->v.s != NULL && strcmp(r->v.s, "-21") == 0,
              "Descending series: thermal.cpu.s display == \"-21\"");
    }

    /* Scenario 13: tied-values series — ascending with repeats.
     * {1000,1000,1050,1050,1100,1100,1150} produces S = +18 (21 pair
     * comparisons minus 3 ties). Critical at N=7 is 17, so 18 >= 17
     * → WARN, direction up. Verifies the emit happens and the direction
     * key is set even when tied values are present in the series. */
    {
        static const unsigned long x[] = { 1000, 1000, 1050, 1050, 1100, 1100, 1150 };
        const result_t *r, *dir;
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 7);
        thermal_check(&t);
        r = k(&t, "thermal.cpu");
        CHECK(r != NULL, "Tied-values series: thermal.cpu row emitted");
        dir = k(&t, "thermal.cpu.direction");
        CHECK(dir != NULL && strcmp(dir->v.s, "up") == 0,
              "Tied-values series: direction=\"up\"");
    }

    /* Scenario 14: MAX_SERIES boundary (N=16). 16-pass strictly
     * ascending series, S = C(16,2) = 120, critical at N=16 is 54.
     * Must emit WARN without crashing the series reader or overflowing
     * any fixed buffer. Guards against future refactors that change
     * MAX_SERIES without updating the critical-value table. */
    {
        static const unsigned long x[] = {
            1000, 1010, 1020, 1030, 1040, 1050, 1060, 1070,
            1080, 1090, 1100, 1110, 1120, 1130, 1140, 1150
        };
        const result_t *r;
        memset(&t, 0, sizeof(t));
        add_series(&t, x, 16);
        thermal_check(&t);
        r = k(&t, "thermal.cpu");
        CHECK(r != NULL && r->verdict == VERDICT_WARN,
              "N=16 (MAX_SERIES) strict ascending → WARN");
    }

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
