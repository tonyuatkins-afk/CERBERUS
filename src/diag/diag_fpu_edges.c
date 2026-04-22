/*
 * FPU IEEE-754 edge-case diagnostic — v0.8.1 M1.1, research gap L.
 *
 * Feeds a focused set of IEEE-754 edge-case operands through FADD /
 * FSUB / FMUL / FDIV / FSQRT and classifies the result bit pattern
 * against the IEEE-754-conformant expectation. This catches broken
 * FPU edge-case handling that the bit-exact tests in diag_fpu.c cannot:
 * signed zero preservation, infinity arithmetic, NaN propagation,
 * 0/0 and inf/inf special forms, sqrt of negative zero.
 *
 * Kept deliberately compact (14 tests total) to fit DGROUP budget.
 * Per-op counters emitted as diagnose.fpu.edge.<op>_ok=<N>_of_<M>;
 * aggregate as diagnose.fpu.edge_cases_ok=<N>_of_<M>.
 *
 * SNaN is not exercised: pre-387 FPUs silent-accept SNaN (no IE raised,
 * just treated as QNaN), and the discrimination is already covered by
 * diag_fpu_fingerprint. Including SNaN here would force a family-aware
 * skip path that is worth more in 0.8.1 M1 than it saves in coverage.
 */

#include <stdio.h>
#include <string.h>
#include "diag.h"
#include "../core/report.h"

/* IEEE-754 double-precision bit-pattern union. Matches the layout in
 * diag_fpu.c so the code conventions are consistent. */
typedef union {
    double        d;
    unsigned long u[2];   /* u[0] = low 32 bits, u[1] = high 32 bits */
} dpf_t;

/* Classification buckets. Kept as a small integer enum so the
 * expected-vs-observed comparison is a single equality test. */
#define FPC_POS_ZERO   0
#define FPC_NEG_ZERO   1
#define FPC_POS_INF    2
#define FPC_NEG_INF    3
#define FPC_NAN        4   /* QNaN or SNaN conflated (we never test SNaN) */
#define FPC_POS_DENORM 5
#define FPC_NEG_DENORM 6
#define FPC_POS_NORMAL 7
#define FPC_NEG_NORMAL 8

/* Pure, host-testable: bucket a double by its bit pattern. */
int fp_classify_double(double d)
{
    dpf_t t;
    unsigned long hi, lo, exp_bits, mant_hi;
    int sign, mant_any;

    t.d = d;
    lo = t.u[0];
    hi = t.u[1];
    sign = (int)((hi >> 31) & 1UL);
    exp_bits = (hi >> 20) & 0x7FFUL;
    mant_hi = hi & 0xFFFFFUL;
    mant_any = (mant_hi != 0UL) || (lo != 0UL);

    if (exp_bits == 0UL) {
        if (!mant_any) return sign ? FPC_NEG_ZERO : FPC_POS_ZERO;
        return sign ? FPC_NEG_DENORM : FPC_POS_DENORM;
    }
    if (exp_bits == 0x7FFUL) {
        if (!mant_any) return sign ? FPC_NEG_INF : FPC_POS_INF;
        return FPC_NAN;
    }
    return sign ? FPC_NEG_NORMAL : FPC_POS_NORMAL;
}

/* FSQRT helper. Emits the bare FPU instruction via Watcom pragma aux
 * on both DOS 16-bit target and NT host-test builds. Avoids pulling
 * <math.h> (which adds ~20 KB to the DOS binary, per diag_fpu.c) and
 * avoids Watcom libc's sqrt() which clamps domain-error inputs to 0.0
 * rather than propagating NaN per IEEE-754. */
extern double fpu_fsqrt(double x);
#pragma aux fpu_fsqrt =      \
    "fsqrt"                   \
    parm [8087]               \
    value [8087]              \
    modify exact [];

static const result_t *find_key_edges(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Build the special operands once at function entry rather than as
 * static initializers; Watcom C89 does not accept union-field
 * designated initializers and the bit-pattern approach via u[] pairs
 * is clearer as executable assignments. */
static void build_operands(dpf_t *pz, dpf_t *nz,
                           dpf_t *pi, dpf_t *ni,
                           dpf_t *qn, dpf_t *one)
{
    pz->u[0] = 0UL;           pz->u[1] = 0x00000000UL;  /* +0.0 */
    nz->u[0] = 0UL;           nz->u[1] = 0x80000000UL;  /* -0.0 */
    pi->u[0] = 0UL;           pi->u[1] = 0x7FF00000UL;  /* +inf */
    ni->u[0] = 0UL;           ni->u[1] = 0xFFF00000UL;  /* -inf */
    qn->u[0] = 0UL;           qn->u[1] = 0x7FF80000UL;  /* QNaN */
    one->d   = 1.0;                                     /* finite reference */
}

/* Row-emit helper. `report_add_str` stores the pointer without copying,
 * so each emitted row needs its own static buffer with module lifetime.
 * Slot index 0..5 maps to add/sub/mul/div/sqrt/aggregate. Caller picks
 * the slot; collisions within a single run would silently overwrite. */
static char edge_slot_bufs[6][16];

static void emit_count_row(result_table_t *t, int slot, const char *key,
                           int got, int of)
{
    char *buf = edge_slot_bufs[slot];
    sprintf(buf, "%d_of_%d", got, of);
    report_add_str(t, key, buf,
                   CONF_HIGH,
                   got == of ? VERDICT_PASS : VERDICT_FAIL);
}

void diag_fpu_edges(result_table_t *t)
{
    const result_t *fpu_entry;
    const char *fpu_val;
    dpf_t pz, nz, pi, ni, qn, one;
    double r;
    int add_ok = 0, sub_ok = 0, mul_ok = 0, div_ok = 0, sqrt_ok = 0;
    int total_ok, total_of;

    fpu_entry = find_key_edges(t, "fpu.detected");
    if (!fpu_entry) return;
    fpu_val = fpu_entry->display ? fpu_entry->display :
              (fpu_entry->type == V_STR ? fpu_entry->v.s : "");
    if (!fpu_val || strcmp(fpu_val, "none") == 0) {
        report_add_str(t, "diagnose.fpu.edge_cases_ok",
                       "skipped (no FPU)",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }

    build_operands(&pz, &nz, &pi, &ni, &qn, &one);

    /* ----- FADD (4 cases) ----- */
    r = pz.d + pz.d;
    if (fp_classify_double(r) == FPC_POS_ZERO) add_ok++;
    r = pz.d + nz.d;
    /* IEEE-754 default rounding: (+0) + (-0) = +0 */
    if (fp_classify_double(r) == FPC_POS_ZERO) add_ok++;
    r = pi.d + ni.d;
    if (fp_classify_double(r) == FPC_NAN) add_ok++;
    r = one.d + qn.d;
    if (fp_classify_double(r) == FPC_NAN) add_ok++;
    emit_count_row(t, 0, "diagnose.fpu.edge.add_ok", add_ok, 4);

    /* ----- FSUB (1 case) ----- */
    r = pi.d - pi.d;
    if (fp_classify_double(r) == FPC_NAN) sub_ok++;
    emit_count_row(t, 1, "diagnose.fpu.edge.sub_ok", sub_ok, 1);

    /* ----- FMUL (2 cases) ----- */
    r = pz.d * pi.d;
    if (fp_classify_double(r) == FPC_NAN) mul_ok++;
    r = one.d * qn.d;
    if (fp_classify_double(r) == FPC_NAN) mul_ok++;
    emit_count_row(t, 2, "diagnose.fpu.edge.mul_ok", mul_ok, 2);

    /* ----- FDIV (3 cases) ----- */
    r = one.d / pz.d;
    if (fp_classify_double(r) == FPC_POS_INF) div_ok++;
    r = pz.d / pz.d;
    if (fp_classify_double(r) == FPC_NAN) div_ok++;
    r = pi.d / pi.d;
    if (fp_classify_double(r) == FPC_NAN) div_ok++;
    emit_count_row(t, 3, "diagnose.fpu.edge.div_ok", div_ok, 3);

    /* ----- FSQRT (4 cases) -----
     * sqrt(-0) must preserve sign per IEEE-754 (returns -0).
     * sqrt(-1) -> NaN (invalid op). sqrt(+inf) -> +inf.
     * sqrt(QNaN) -> QNaN (propagation). */
    r = fpu_fsqrt(nz.d);
    if (fp_classify_double(r) == FPC_NEG_ZERO) sqrt_ok++;
    {
        dpf_t neg_one;
        neg_one.d = -1.0;
        r = fpu_fsqrt(neg_one.d);
        if (fp_classify_double(r) == FPC_NAN) sqrt_ok++;
    }
    r = fpu_fsqrt(pi.d);
    if (fp_classify_double(r) == FPC_POS_INF) sqrt_ok++;
    r = fpu_fsqrt(qn.d);
    if (fp_classify_double(r) == FPC_NAN) sqrt_ok++;
    emit_count_row(t, 4, "diagnose.fpu.edge.sqrt_ok", sqrt_ok, 4);

    /* ----- Aggregate ----- */
    total_ok = add_ok + sub_ok + mul_ok + div_ok + sqrt_ok;
    total_of = 4 + 1 + 2 + 3 + 4;  /* 14 */
    emit_count_row(t, 5, "diagnose.fpu.edge_cases_ok", total_ok, total_of);
}
