/*
 * Host-side test for diag_fpu_edges (0.8.1 M1.1, research gap L).
 *
 * Covers the pure fp_classify_double() bucketing and a functional
 * assertion that diag_fpu_edges() emits the expected aggregate row
 * when run against the host's IEEE-754 double-precision FPU (which
 * must be conformant for the test harness to be useful).
 *
 * On a host with an IEEE-754-conformant double-precision FPU, every
 * edge case should pass and emit "14_of_14". Failing that indicates
 * either (a) the classifier itself has a bug, or (b) the host's
 * FPU is non-conformant, or (c) a regression in the edge-case table.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/diag/diag_fpu_edges.c"

#include <stdio.h>
#include <string.h>

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

static void test_classifier_pure(void)
{
    typedef union { double d; unsigned long u[2]; } dp_t;
    dp_t v;

    printf("\n[test_classifier_pure]\n");

    v.d = 0.0;
    CHECK(fp_classify_double(v.d) == FPC_POS_ZERO, "+0.0 classifies as POS_ZERO");

    /* -0.0 via bit-pattern construction */
    v.u[0] = 0UL; v.u[1] = 0x80000000UL;
    CHECK(fp_classify_double(v.d) == FPC_NEG_ZERO, "-0.0 classifies as NEG_ZERO");

    /* +inf */
    v.u[0] = 0UL; v.u[1] = 0x7FF00000UL;
    CHECK(fp_classify_double(v.d) == FPC_POS_INF, "+inf classifies as POS_INF");

    /* -inf */
    v.u[0] = 0UL; v.u[1] = 0xFFF00000UL;
    CHECK(fp_classify_double(v.d) == FPC_NEG_INF, "-inf classifies as NEG_INF");

    /* QNaN */
    v.u[0] = 0UL; v.u[1] = 0x7FF80000UL;
    CHECK(fp_classify_double(v.d) == FPC_NAN, "QNaN classifies as NAN");

    /* Positive denormal (smallest) */
    v.u[0] = 1UL; v.u[1] = 0x00000000UL;
    CHECK(fp_classify_double(v.d) == FPC_POS_DENORM, "+denorm classifies as POS_DENORM");

    /* Negative denormal */
    v.u[0] = 1UL; v.u[1] = 0x80000000UL;
    CHECK(fp_classify_double(v.d) == FPC_NEG_DENORM, "-denorm classifies as NEG_DENORM");

    v.d = 1.0;
    CHECK(fp_classify_double(v.d) == FPC_POS_NORMAL, "+1.0 classifies as POS_NORMAL");

    v.d = -3.14;
    CHECK(fp_classify_double(v.d) == FPC_NEG_NORMAL, "-3.14 classifies as NEG_NORMAL");
}

/* Seed the result table with a "fpu.detected=integrated..." row so
 * diag_fpu_edges runs its full probe set instead of skipping. */
static void seed_fpu_present(result_table_t *t)
{
    report_add_str(t, "fpu.detected", "integrated",
                   CONF_HIGH, VERDICT_UNKNOWN);
}

static void test_aggregate_all_pass(void)
{
    result_table_t t;
    const result_t *agg;

    printf("\n[test_aggregate_all_pass]\n");

    memset(&t, 0, sizeof(t));
    seed_fpu_present(&t);
    diag_fpu_edges(&t);

    agg = lookup_key(&t, "diagnose.fpu.edge_cases_ok");
    CHECK(agg != NULL, "aggregate row emitted");
    if (agg) {
        const char *s = agg->display ? agg->display :
                        (agg->type == V_STR ? agg->v.s : "");
        const result_t *op;
        printf("  ...aggregate value = '%s'\n", s);
        op = lookup_key(&t, "diagnose.fpu.edge.add_ok");
        if (op) printf("    add_ok  = '%s'\n", op->display ? op->display : op->v.s);
        op = lookup_key(&t, "diagnose.fpu.edge.sub_ok");
        if (op) printf("    sub_ok  = '%s'\n", op->display ? op->display : op->v.s);
        op = lookup_key(&t, "diagnose.fpu.edge.mul_ok");
        if (op) printf("    mul_ok  = '%s'\n", op->display ? op->display : op->v.s);
        op = lookup_key(&t, "diagnose.fpu.edge.div_ok");
        if (op) printf("    div_ok  = '%s'\n", op->display ? op->display : op->v.s);
        op = lookup_key(&t, "diagnose.fpu.edge.sqrt_ok");
        if (op) printf("    sqrt_ok = '%s'\n", op->display ? op->display : op->v.s);
        CHECK(strcmp(s, "14_of_14") == 0,
              "all 14 cases pass on IEEE-conformant host FPU");
        CHECK(agg->verdict == VERDICT_PASS,
              "aggregate verdict is PASS when all cases pass");
    }
}

static void test_per_op_rows_emitted(void)
{
    result_table_t t;

    printf("\n[test_per_op_rows_emitted]\n");

    memset(&t, 0, sizeof(t));
    seed_fpu_present(&t);
    diag_fpu_edges(&t);

    CHECK(lookup_key(&t, "diagnose.fpu.edge.add_ok") != NULL,
          "diagnose.fpu.edge.add_ok row emitted");
    CHECK(lookup_key(&t, "diagnose.fpu.edge.sub_ok") != NULL,
          "diagnose.fpu.edge.sub_ok row emitted");
    CHECK(lookup_key(&t, "diagnose.fpu.edge.mul_ok") != NULL,
          "diagnose.fpu.edge.mul_ok row emitted");
    CHECK(lookup_key(&t, "diagnose.fpu.edge.div_ok") != NULL,
          "diagnose.fpu.edge.div_ok row emitted");
    CHECK(lookup_key(&t, "diagnose.fpu.edge.sqrt_ok") != NULL,
          "diagnose.fpu.edge.sqrt_ok row emitted");
}

static void test_skipped_when_no_fpu(void)
{
    result_table_t t;
    const result_t *agg;

    printf("\n[test_skipped_when_no_fpu]\n");

    memset(&t, 0, sizeof(t));
    report_add_str(&t, "fpu.detected", "none",
                   CONF_HIGH, VERDICT_UNKNOWN);
    diag_fpu_edges(&t);

    agg = lookup_key(&t, "diagnose.fpu.edge_cases_ok");
    CHECK(agg != NULL, "skip path still emits aggregate row");
    if (agg) {
        const char *s = agg->display ? agg->display :
                        (agg->type == V_STR ? agg->v.s : "");
        CHECK(strstr(s, "skipped") != NULL,
              "skip path value contains 'skipped'");
    }

    CHECK(lookup_key(&t, "diagnose.fpu.edge.add_ok") == NULL,
          "per-op rows absent when FPU absent");
}

static void test_skipped_when_fpu_key_missing(void)
{
    result_table_t t;

    printf("\n[test_skipped_when_fpu_key_missing]\n");

    memset(&t, 0, sizeof(t));
    diag_fpu_edges(&t);

    CHECK(t.count == 0, "no rows emitted when fpu.detected absent");
}

int main(void)
{
    test_classifier_pure();
    test_aggregate_all_pass();
    test_per_op_rows_emitted();
    test_skipped_when_no_fpu();
    test_skipped_when_fpu_key_missing();

    printf("\n=== %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
