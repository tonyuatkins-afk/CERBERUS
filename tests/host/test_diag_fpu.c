/*
 * Host-side test for diag_fpu's any_failed aggregation path.
 *
 * The Phase 4 refactor changed diag_fpu so the "diagnose.fpu.compound"
 * row is ALWAYS emitted (PASS when every sub-test passed, FAIL when any
 * earlier test failed). Rule 5 in consist.c keys off that compound row
 * as its single-source-of-truth "FPU diagnostic ran cleanly" signal, so
 * any regression that reverts the early-return behavior would silently
 * neuter the rule on exactly the machines it's meant to catch.
 *
 * We use the CERBERUS_HOST_TEST fault-injection hook in diag_fpu.c
 * (force_fail_test_idx) to flip each of the five bit-exact comparisons
 * to a synthetic fail, one at a time, and assert that:
 *
 *   1. All-pass path emits fpu.compound=PASS and sets fpu.detected=PASS
 *   2. Test 1 (add) fail  → fpu.add=FAIL + fpu.compound=FAIL with the
 *                           "earlier bit-exact check failed" aggregation
 *                           message (not "compound expr failed")
 *   3. Test 5 (compound) fail → fpu.compound=FAIL with the
 *                               "compound expr failed" message
 *   4. fpu.detected verdict reflects any_failed (PASS vs FAIL)
 *
 * Without this test, the any_failed aggregation logic (diag_fpu.c lines
 * 74-150 in the post-refactor source) has zero coverage — the happy
 * path never exercises it, and dropping the compound-on-fail emit would
 * pass the rest of the test suite.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/diag/diag_fpu.c"

#include <stdio.h>
#include <string.h>

/* The force-fail hook lives in diag_fpu.c under CERBERUS_HOST_TEST. We
 * reach into it directly rather than adding a setter, because the test
 * is the only caller and a function wrapper would just add noise. */
extern int force_fail_test_idx;
extern int fail_call_counter;  /* reset between fixtures */

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

static const result_t *lookup_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

static verdict_t v_of(const result_t *r) { return r ? r->verdict : VERDICT_UNKNOWN; }

static const char *s_of(const result_t *r)
{
    if (!r) return "";
    if (r->display) return r->display;
    if (r->type == V_STR && r->v.s) return r->v.s;
    return "";
}

/* Rebuild a minimal table with fpu.detected set so diag_fpu runs past the
 * "no FPU" skip. We also reset the fault-injection state each fixture so
 * the fail_call_counter starts at 0. */
static void reset_fixture(result_table_t *t, int fail_idx)
{
    memset(t, 0, sizeof(*t));
    report_add_str(t, "fpu.detected", "integrated-486",
                   CONF_HIGH, VERDICT_UNKNOWN);
    force_fail_test_idx = fail_idx;
    fail_call_counter = 0;
}

int main(void)
{
    result_table_t t;
    const result_t *r;

    printf("=== CERBERUS host unit test: diag_fpu any_failed aggregation ===\n");

    /* Fixture 1: no injection — happy path, every test passes. */
    reset_fixture(&t, 0);
    diag_fpu(&t);
    r = lookup_key(&t, "diagnose.fpu.add");
    CHECK(v_of(r) == VERDICT_PASS, "Fixture 1 (no inject): fpu.add PASS");
    r = lookup_key(&t, "diagnose.fpu.sub");
    CHECK(v_of(r) == VERDICT_PASS, "Fixture 1 (no inject): fpu.sub PASS");
    r = lookup_key(&t, "diagnose.fpu.mul");
    CHECK(v_of(r) == VERDICT_PASS, "Fixture 1 (no inject): fpu.mul PASS");
    r = lookup_key(&t, "diagnose.fpu.div");
    CHECK(v_of(r) == VERDICT_PASS, "Fixture 1 (no inject): fpu.div PASS");
    r = lookup_key(&t, "diagnose.fpu.compound");
    CHECK(v_of(r) == VERDICT_PASS, "Fixture 1 (no inject): fpu.compound PASS");
    CHECK(strcmp(s_of(r), "pass") == 0,
          "Fixture 1: compound row message = 'pass'");
    r = lookup_key(&t, "fpu.detected");
    CHECK(v_of(r) == VERDICT_PASS,
          "Fixture 1: fpu.detected verdict promoted to PASS");

    /* Fixture 2: inject fault on test 1 (add). Expect fpu.add=FAIL and
     * fpu.compound=FAIL with the "earlier bit-exact check failed"
     * aggregation message — that's the branch that proves the refactor
     * behaves. fpu.detected should downgrade to FAIL. */
    reset_fixture(&t, 1);
    diag_fpu(&t);
    r = lookup_key(&t, "diagnose.fpu.add");
    CHECK(v_of(r) == VERDICT_FAIL, "Fixture 2 (add inject): fpu.add FAIL");
    CHECK(strcmp(s_of(r), "1.0+2.0 failed") == 0,
          "Fixture 2: fpu.add message = '1.0+2.0 failed'");
    r = lookup_key(&t, "diagnose.fpu.sub");
    CHECK(v_of(r) == VERDICT_PASS,
          "Fixture 2: fpu.sub still PASS (no early return)");
    r = lookup_key(&t, "diagnose.fpu.compound");
    CHECK(v_of(r) == VERDICT_FAIL,
          "Fixture 2: fpu.compound FAIL (any_failed aggregation)");
    CHECK(strcmp(s_of(r), "earlier bit-exact check failed") == 0,
          "Fixture 2: compound message = 'earlier bit-exact check failed'");
    r = lookup_key(&t, "fpu.detected");
    CHECK(v_of(r) == VERDICT_FAIL,
          "Fixture 2: fpu.detected downgraded to FAIL");

    /* Fixture 3: inject on test 5 (compound) only. Expect tests 1-4
     * PASS, fpu.compound=FAIL with the direct "compound expr failed"
     * message (not the aggregation message — nothing earlier failed). */
    reset_fixture(&t, 5);
    diag_fpu(&t);
    r = lookup_key(&t, "diagnose.fpu.add");
    CHECK(v_of(r) == VERDICT_PASS, "Fixture 3 (compound inject): fpu.add PASS");
    r = lookup_key(&t, "diagnose.fpu.div");
    CHECK(v_of(r) == VERDICT_PASS, "Fixture 3: fpu.div PASS");
    r = lookup_key(&t, "diagnose.fpu.compound");
    CHECK(v_of(r) == VERDICT_FAIL, "Fixture 3: fpu.compound FAIL");
    CHECK(strcmp(s_of(r), "compound expr failed") == 0,
          "Fixture 3: compound message = 'compound expr failed'");
    r = lookup_key(&t, "fpu.detected");
    CHECK(v_of(r) == VERDICT_FAIL, "Fixture 3: fpu.detected FAIL");

    /* Fixture 4: no FPU detected → the whole function short-circuits to
     * a "skipped" row and NEVER emits fpu.compound. This guards the
     * "skipped" branch so a future refactor can't accidentally run the
     * tests on emulated FPUs (Watcom -fpi would produce false PASS). */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "fpu.detected", "none", CONF_HIGH, VERDICT_UNKNOWN);
    force_fail_test_idx = 0;
    fail_call_counter = 0;
    diag_fpu(&t);
    r = lookup_key(&t, "diagnose.fpu");
    CHECK(v_of(r) == VERDICT_UNKNOWN, "Fixture 4 (no FPU): diagnose.fpu UNKNOWN");
    CHECK(strcmp(s_of(r), "skipped (no FPU detected)") == 0,
          "Fixture 4: diagnose.fpu message = 'skipped (no FPU detected)'");
    r = lookup_key(&t, "diagnose.fpu.compound");
    CHECK(r == (const result_t *)0,
          "Fixture 4: NO fpu.compound row emitted when FPU absent");

    /* Reset in case any teardown runs later. */
    force_fail_test_idx = 0;
    fail_call_counter = 0;

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
