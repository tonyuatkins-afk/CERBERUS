/*
 * FPU correctness diagnostic — Phase 2 Task 2.3.
 *
 * Known-answer tests using values that are EXACTLY representable in
 * IEEE 754 double precision — small integers, powers of 2, and ratios
 * thereof. This lets us compare bit-for-bit against baked-in expected
 * bit patterns and catch any subtle FPU fault (bad multiplier, wrong
 * exponent bias, broken normalization).
 *
 * Skipped entirely if detect_fpu reported `fpu.detected=none` — no
 * point testing hardware that isn't there. Watcom's -fpi emulator
 * would answer the test itself on a no-FPU system and give us a
 * false PASS, which is worse than skipping.
 *
 * Scope for v0.3 minimum: the six basic ops (+, -, *, /, sqrt, load/
 * store round-trip). Pentium FDIV-bug detection and transcendental
 * precision checks land as follow-ups.
 */

#include <stdio.h>
#include <string.h>
#include "diag.h"
#include "../core/report.h"

/* Note: we deliberately do NOT include <math.h>. Pulling in Watcom's math
 * library via sqrt/fabs/etc. adds ~22KB to the DOS binary and blows the
 * 64KB ceiling. Basic +, -, *, / already exercise the FPU via compiler-
 * emitted FADD/FSUB/FMUL/FDIV opcodes under -fpi. Transcendental tests
 * (sqrt, sin, cos, log) are deferred to Phase 3 when the benchmark
 * infrastructure can amortize the library cost. */

/* Union for bit-exact comparison of doubles. On Watcom DOS 16-bit,
 * `double` is 8 bytes (IEEE 754 64-bit). */
typedef union {
    double        d;
    unsigned long u[2];
} dp_t;

static int double_bits_equal(double a, double b)
{
    dp_t da, db;
    da.d = a;
    db.d = b;
    return (da.u[0] == db.u[0]) && (da.u[1] == db.u[1]);
}

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

void diag_fpu(result_table_t *t)
{
    const result_t *fpu_entry = find_key(t, "fpu.detected");
    const char *fpu_val;
    double a, b, result, expected;
    long round_trip;
    int any_failed = 0;

    /* Skip if no FPU */
    if (!fpu_entry) return;
    fpu_val = fpu_entry->display ? fpu_entry->display :
              (fpu_entry->type == V_STR ? fpu_entry->v.s : "");
    if (!fpu_val || strcmp(fpu_val, "none") == 0) {
        report_add_str(t, "diagnose.fpu", "skipped (no FPU detected)",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }

    /* All five tests below set `any_failed` rather than early-returning.
     * Rule 5 in the consistency engine reads `diagnose.fpu.compound`
     * as its aggregated FPU-ran-correctly signal, so the compound row
     * MUST be emitted on every run that reaches this point — including
     * when an earlier test failed. Early-returning before the compound
     * emit made rule 5 no-op on precisely the machines it was designed
     * to catch (failing FPU detected by diag but still exercised by
     * bench_fpu producing a nonzero ops_per_sec number). */

    /* Test 1: addition */
    a = 1.0; b = 2.0; result = a + b; expected = 3.0;
    if (!double_bits_equal(result, expected)) {
        report_add_str(t, "diagnose.fpu.add", "1.0+2.0 failed",
                       CONF_HIGH, VERDICT_FAIL);
        any_failed = 1;
    } else {
        report_add_str(t, "diagnose.fpu.add", "pass", CONF_HIGH, VERDICT_PASS);
    }

    /* Test 2: subtraction */
    a = 5.0; b = 2.0; result = a - b; expected = 3.0;
    if (!double_bits_equal(result, expected)) {
        report_add_str(t, "diagnose.fpu.sub", "5.0-2.0 failed",
                       CONF_HIGH, VERDICT_FAIL);
        any_failed = 1;
    } else {
        report_add_str(t, "diagnose.fpu.sub", "pass", CONF_HIGH, VERDICT_PASS);
    }

    /* Test 3: multiplication */
    a = 4.0; b = 2.5; result = a * b; expected = 10.0;
    if (!double_bits_equal(result, expected)) {
        report_add_str(t, "diagnose.fpu.mul", "4.0*2.5 failed",
                       CONF_HIGH, VERDICT_FAIL);
        any_failed = 1;
    } else {
        report_add_str(t, "diagnose.fpu.mul", "pass", CONF_HIGH, VERDICT_PASS);
    }

    /* Test 4: division */
    a = 10.0; b = 2.0; result = a / b; expected = 5.0;
    if (!double_bits_equal(result, expected)) {
        report_add_str(t, "diagnose.fpu.div", "10.0/2.0 failed",
                       CONF_HIGH, VERDICT_FAIL);
        any_failed = 1;
    } else {
        report_add_str(t, "diagnose.fpu.div", "pass", CONF_HIGH, VERDICT_PASS);
    }

    (void)round_trip;  /* long-to-double round-trip test removed — pulled
                        * in ~20KB of Watcom conversion runtime. Can be
                        * added back in Phase 3 when the benchmark module
                        * amortizes the library cost across many tests. */

    /* Test 5: compound expression — exercises multiple FPU ops and the
     * stack. (2.0 + 3.0) * 4.0 - 5.0 = 15.0, all exactly representable. */
    a = 2.0; b = 3.0;
    result = (a + b) * 4.0 - 5.0;
    expected = 15.0;
    if (!double_bits_equal(result, expected)) {
        report_add_str(t, "diagnose.fpu.compound", "compound expr failed",
                       CONF_HIGH, VERDICT_FAIL);
        any_failed = 1;
    } else if (any_failed) {
        /* Aggregated signal: earlier test(s) failed, compound emits
         * FAIL so rule 5 / downstream readers see a single source of
         * truth for "did the FPU diagnostic pass as a whole." */
        report_add_str(t, "diagnose.fpu.compound",
                       "earlier bit-exact check failed",
                       CONF_HIGH, VERDICT_FAIL);
    } else {
        report_add_str(t, "diagnose.fpu.compound", "pass",
                       CONF_HIGH, VERDICT_PASS);
    }

    report_set_verdict(t, "fpu.detected",
                       any_failed ? VERDICT_FAIL : VERDICT_PASS);
}
