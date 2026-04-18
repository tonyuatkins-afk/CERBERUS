/*
 * CPU ALU integrity diagnostic — Phase 2 Task 2.1.
 *
 * Runs a fixed table of 16-bit arithmetic and bitwise operations and
 * compares the runtime results against hand-computed expected values.
 * The test vectors are embedded as const data, so the expected values
 * are baked into the binary at cross-compile time on a known-good host
 * — they are NOT computed by the target CPU at runtime. A stuck bit,
 * wrong flag, or bad multiplier on the target would produce a runtime
 * result that diverges from the const expected value.
 *
 * Scope for v0.3 minimum:
 *   - 16-bit ADD, SUB, AND, OR, XOR (low-word result only)
 *   - 16-bit MUL (check low 16 of 32-bit product)
 *   - Logical shifts SHL, SHR
 *
 * Deferred: flag-register tests (CF, ZF, SF, OF), signed-division edge
 * cases, 32-bit operations on 386+. These need dedicated probes with
 * inline assembly to capture flags, which is scope-inflating for the
 * Phase 2 opening task. The minimum set above catches any gross ALU
 * fault — stuck bits in the register file, bad adder carry chains, or
 * a dead multiplier.
 *
 * A failing vector triggers VERDICT_FAIL on the existing cpu.detected
 * entry AND emits a diagnose.cpu.alu=fail row with details of the
 * specific vector that failed. VERDICT_PASS otherwise.
 */

#include <stdio.h>
#include <string.h>
#include "diag.h"
#include "../core/report.h"

/* FAIL-path detail buffer. report_add_str stores the value pointer
 * verbatim (report.c:55), so a stack-local detail[] would dangle after
 * diag_cpu returns and the INI writer + UI renderer would read garbage.
 * The three FAIL paths (alu / mul / shift) each early-return, so at
 * most one sprintf writes this per call — one shared static is safe. */
static char diag_cpu_detail[64];

typedef struct {
    unsigned int a, b;
    unsigned int sum;
    unsigned int diff;
    unsigned int and_;
    unsigned int or_;
    unsigned int xor_;
} alu_vec_t;

/* Hand-computed 16-bit expected results. Any CPU that disagrees has a
 * real ALU fault. Verification: compile-time constants checked against
 * a separate reference implementation during authoring. */
static const alu_vec_t alu_vectors[] = {
    /* a       b       sum     diff    and     or      xor    */
    { 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
    { 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF },
    { 0x0000, 0xFFFF, 0xFFFF, 0x0001, 0x0000, 0xFFFF, 0xFFFF },
    { 0xFFFF, 0xFFFF, 0xFFFE, 0x0000, 0xFFFF, 0xFFFF, 0x0000 },
    { 0xAAAA, 0x5555, 0xFFFF, 0x5555, 0x0000, 0xFFFF, 0xFFFF },
    { 0x1234, 0x5678, 0x68AC, 0xBBBC, 0x1230, 0x567C, 0x444C },
    { 0x5678, 0x1234, 0x68AC, 0x4444, 0x1230, 0x567C, 0x444C },
    { 0x0001, 0x0001, 0x0002, 0x0000, 0x0001, 0x0001, 0x0000 },
    { 0xFFFE, 0x0002, 0x0000, 0xFFFC, 0x0002, 0xFFFE, 0xFFFC },
    { 0x8000, 0x0001, 0x8001, 0x7FFF, 0x0000, 0x8001, 0x8001 },
    { 0xF0F0, 0x0F0F, 0xFFFF, 0xE1E1, 0x0000, 0xFFFF, 0xFFFF },
    { 0x00FF, 0x0100, 0x01FF, 0xFFFF, 0x0000, 0x01FF, 0x01FF },
    { 0x1000, 0x0010, 0x1010, 0x0FF0, 0x0000, 0x1010, 0x1010 },
    { 0xBEEF, 0xCAFE, 0x89ED, 0xF3F1, 0x8AEE, 0xFEFF, 0x7411 },
    { 0xDEAD, 0xBEEF, 0x9D9C, 0x1FBE, 0x9EAD, 0xFEEF, 0x6042 }
};
#define ALU_VEC_COUNT (sizeof(alu_vectors) / sizeof(alu_vectors[0]))

typedef struct {
    unsigned int a, b;
    unsigned int prod_lo;   /* low 16 of (a * b) */
} mul_vec_t;

static const mul_vec_t mul_vectors[] = {
    { 0x0000, 0x0000, 0x0000 },
    { 0x0001, 0x0001, 0x0001 },
    { 0x0010, 0x0010, 0x0100 },
    { 0x00FF, 0x00FF, 0xFE01 },
    { 0x0100, 0x0010, 0x1000 },
    { 0xFF00, 0x0002, 0xFE00 },   /* low 16 of 0x1FE00 */
    { 0xFFFF, 0x0001, 0xFFFF },
    { 0xFFFF, 0xFFFF, 0x0001 }    /* low 16 of 0xFFFE0001 */
};
#define MUL_VEC_COUNT (sizeof(mul_vectors) / sizeof(mul_vectors[0]))

typedef struct {
    unsigned int a;
    unsigned int shift;
    unsigned int shl_result;
    unsigned int shr_result;
} shift_vec_t;

static const shift_vec_t shift_vectors[] = {
    { 0x0001,  1, 0x0002, 0x0000 },
    { 0x0001, 15, 0x8000, 0x0000 },
    { 0x8000,  1, 0x0000, 0x4000 },
    { 0x4321,  4, 0x3210, 0x0432 },
    { 0x8000,  7, 0x0000, 0x0100 },
    { 0xFFFF,  8, 0xFF00, 0x00FF },
    { 0xAAAA,  1, 0x5554, 0x5555 }
};
#define SHIFT_VEC_COUNT (sizeof(shift_vectors) / sizeof(shift_vectors[0]))

static int run_alu_tests(int *out_failed_vector)
{
    unsigned int i;
    for (i = 0; i < ALU_VEC_COUNT; i++) {
        const alu_vec_t *v = &alu_vectors[i];
        unsigned int sum  = (v->a + v->b) & 0xFFFF;
        unsigned int diff = (v->a - v->b) & 0xFFFF;
        unsigned int and_ = v->a & v->b;
        unsigned int or_  = v->a | v->b;
        unsigned int xor_ = v->a ^ v->b;
        if (sum  != v->sum)  { *out_failed_vector = (int)i; return 0; }
        if (diff != v->diff) { *out_failed_vector = (int)i; return 0; }
        if (and_ != v->and_) { *out_failed_vector = (int)i; return 0; }
        if (or_  != v->or_)  { *out_failed_vector = (int)i; return 0; }
        if (xor_ != v->xor_) { *out_failed_vector = (int)i; return 0; }
    }
    return 1;
}

static int run_mul_tests(int *out_failed_vector)
{
    unsigned int i;
    for (i = 0; i < MUL_VEC_COUNT; i++) {
        const mul_vec_t *v = &mul_vectors[i];
        unsigned int product_lo = (v->a * v->b) & 0xFFFF;
        if (product_lo != v->prod_lo) {
            *out_failed_vector = (int)i;
            return 0;
        }
    }
    return 1;
}

static int run_shift_tests(int *out_failed_vector)
{
    unsigned int i;
    for (i = 0; i < SHIFT_VEC_COUNT; i++) {
        const shift_vec_t *v = &shift_vectors[i];
        unsigned int shl = (v->a << v->shift) & 0xFFFF;
        unsigned int shr = (v->a >> v->shift) & 0xFFFF;
        if (shl != v->shl_result) { *out_failed_vector = (int)i; return 0; }
        if (shr != v->shr_result) { *out_failed_vector = (int)i; return 0; }
    }
    return 1;
}

void diag_cpu(result_table_t *t)
{
    int failed_vec = -1;

    if (!run_alu_tests(&failed_vec)) {
        sprintf(diag_cpu_detail, "ALU vector %d failed (a=%04X b=%04X)",
                failed_vec,
                alu_vectors[failed_vec].a, alu_vectors[failed_vec].b);
        report_add_str(t, "diagnose.cpu.alu", diag_cpu_detail,
                       CONF_HIGH, VERDICT_FAIL);
        report_set_verdict(t, "cpu.detected", VERDICT_FAIL);
        return;
    }
    if (!run_mul_tests(&failed_vec)) {
        sprintf(diag_cpu_detail, "MUL vector %d failed (a=%04X b=%04X)",
                failed_vec,
                mul_vectors[failed_vec].a, mul_vectors[failed_vec].b);
        report_add_str(t, "diagnose.cpu.mul", diag_cpu_detail,
                       CONF_HIGH, VERDICT_FAIL);
        report_set_verdict(t, "cpu.detected", VERDICT_FAIL);
        return;
    }
    if (!run_shift_tests(&failed_vec)) {
        sprintf(diag_cpu_detail, "shift vector %d failed (a=%04X n=%u)",
                failed_vec,
                shift_vectors[failed_vec].a, shift_vectors[failed_vec].shift);
        report_add_str(t, "diagnose.cpu.shift", diag_cpu_detail,
                       CONF_HIGH, VERDICT_FAIL);
        report_set_verdict(t, "cpu.detected", VERDICT_FAIL);
        return;
    }

    report_add_str(t, "diagnose.cpu.alu",   "pass", CONF_HIGH, VERDICT_PASS);
    report_add_str(t, "diagnose.cpu.mul",   "pass", CONF_HIGH, VERDICT_PASS);
    report_add_str(t, "diagnose.cpu.shift", "pass", CONF_HIGH, VERDICT_PASS);
    report_set_verdict(t, "cpu.detected", VERDICT_PASS);
}
