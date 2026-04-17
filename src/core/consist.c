/*
 * Consistency engine — Phase 4 Task 4.1.
 *
 * Seed rules for v0.5. More rules land as Phase 3 calibrated mode
 * populates per-pass bench data (rule 4a/4b MIPS vs class_ipc), as
 * Task 1.4 cache stride work lands (rule 8), and as Phase 2 deferred
 * diagnostics add coverage.
 *
 * Each rule is structured as a small function that:
 *   1. Reads the specific keys it depends on via find_key()
 *   2. Returns early with no emit if a prerequisite key is absent
 *      (the rule doesn't apply to this run)
 *   3. Emits exactly one consistency.<name> row with PASS / WARN /
 *      FAIL and a human-readable explanation
 *
 * Rules NEVER emit multiple rows, NEVER modify existing entries, and
 * NEVER crash the orchestrator if a detect module skipped something.
 * Absence of a required key is always "rule not applicable" — never
 * a fault in itself.
 */

#include <stdio.h>
#include <string.h>
#include "consist.h"
#include "report.h"

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

static const char *key_value(const result_t *r)
{
    if (!r) return (const char *)0;
    if (r->display) return r->display;
    if (r->type == V_STR && r->v.s) return r->v.s;
    return (const char *)0;
}

static int value_contains(const result_t *r, const char *substring)
{
    const char *v = key_value(r);
    return v && strstr(v, substring) != (char *)0;
}

/* ----------------------------------------------------------------------- */
/* Rule 1: CPU reported as 486DX (or similar DX variant) → FPU must be
 *         integrated.
 *
 * Detects: counterfeit "486DX" that is really an SX with no FPU, or a
 *          486SX with the ID silkscreen mislabeled.
 * Does NOT detect: an actual 486DX with a broken FPU (that's the
 *          diag_fpu domain).                                              */
/* ----------------------------------------------------------------------- */

static void rule_486dx_implies_integrated_fpu(result_table_t *t)
{
    const result_t *cpu = find_key(t, "cpu.detected");
    const result_t *fpu = find_key(t, "fpu.detected");
    const char *cv = key_value(cpu);
    const char *fv = key_value(fpu);

    if (!cv || !fv) return;  /* rule not applicable */

    /* Applies to any 486 variant ending in "DX" (DX/DX2/DX4) but NOT
     * 486SX. The cpu_db friendly names follow the pattern "i486DX..." */
    if (strstr(cv, "486DX") == (char *)0) return;
    if (strstr(cv, "486SX") != (char *)0) return;  /* belt + suspenders */

    if (strstr(fv, "integrated") != (char *)0) {
        report_add_str(t, "consistency.486dx_fpu",
                       "pass (486DX reports integrated FPU)",
                       CONF_HIGH, VERDICT_PASS);
    } else {
        report_add_str(t, "consistency.486dx_fpu",
                       "FAIL: CPU reports 486DX-class but FPU is not integrated",
                       CONF_HIGH, VERDICT_FAIL);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 2: CPU reported as 486SX → FPU must NOT be integrated.
 *
 * Detects: a 486SX report that paradoxically shows integrated FPU
 *          (possible detection confusion or masked 487 scenario).
 * Does NOT detect: a 486SX + external 487 coprocessor combination
 *          (that's a valid state — "non-integrated FPU present" is
 *          the expected behavior and rule 2 passes).                      */
/* ----------------------------------------------------------------------- */

static void rule_486sx_no_integrated_fpu(result_table_t *t)
{
    const result_t *cpu = find_key(t, "cpu.detected");
    const result_t *fpu = find_key(t, "fpu.detected");
    const char *cv = key_value(cpu);
    const char *fv = key_value(fpu);

    if (!cv || !fv) return;
    if (strstr(cv, "486SX") == (char *)0) return;

    if (strstr(fv, "integrated") == (char *)0) {
        report_add_str(t, "consistency.486sx_fpu",
                       "pass (486SX reports non-integrated FPU as expected)",
                       CONF_HIGH, VERDICT_PASS);
    } else {
        report_add_str(t, "consistency.486sx_fpu",
                       "FAIL: CPU is 486SX but FPU reports integrated",
                       CONF_HIGH, VERDICT_FAIL);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 5: FPU diag PASS ↔ FPU bench produced a numeric result.
 *
 * Detects: internal inconsistency where one head claims the FPU works
 *          and another couldn't exercise it, or vice versa.
 * Does NOT detect: both heads failing the same way (correlated fault).   */
/* ----------------------------------------------------------------------- */

static void rule_fpu_diag_bench_agreement(result_table_t *t)
{
    const result_t *diag = find_key(t, "diagnose.fpu.compound");
    const result_t *bench = find_key(t, "bench.fpu.ops_per_sec");
    int diag_pass  = diag  && diag->verdict == VERDICT_PASS;
    int bench_ok   = bench && bench->v.u > 0UL;

    /* Rule only applies if BOTH paths ran — an "FPU absent" run skips
     * both diag and bench, which is internally consistent and outside
     * the scope of this cross-check. */
    if (!diag && !bench) return;

    if (diag_pass == bench_ok) {
        report_add_str(t, "consistency.fpu_diag_bench",
                       "pass (diag and bench agree on FPU liveness)",
                       CONF_HIGH, VERDICT_PASS);
    } else {
        char msg[96];
        sprintf(msg, "WARN: diag.fpu=%s but bench.fpu=%s",
                diag_pass ? "pass" : "no-pass",
                bench_ok  ? "has-result" : "no-result");
        report_add_str(t, "consistency.fpu_diag_bench", msg,
                       CONF_HIGH, VERDICT_WARN);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 6: memory.extended_kb > 0 → CPU class must be ≥ 286.
 *
 * Detects: a detect run that reports extended memory on an 8086-class
 *          CPU (physically impossible; would indicate a detection bug).
 * Does NOT detect: a correctly-detected 286+ with improperly-reported
 *          extended memory size.                                          */
/* ----------------------------------------------------------------------- */

static void rule_extmem_implies_286(result_table_t *t)
{
    const result_t *ext   = find_key(t, "memory.extended_kb");
    const result_t *class = find_key(t, "cpu.class");
    const char *cv = key_value(class);
    unsigned long ext_kb;

    if (!ext || !class) return;

    ext_kb = ext->v.u;
    if (ext_kb == 0) return;  /* rule not applicable */

    /* The CPU class tokens from cpu_db: 8086/8088/v20/v30 are the
     * legacy pre-286 class; anything else is 286 or later. */
    if (!cv) return;
    if (strcmp(cv, "8086") == 0 || strcmp(cv, "8088") == 0 ||
        strcmp(cv, "v20")  == 0 || strcmp(cv, "v30")  == 0) {
        char msg[96];
        sprintf(msg, "FAIL: extended memory %luKB reported on %s",
                ext_kb, cv);
        report_add_str(t, "consistency.extmem_cpu", msg,
                       CONF_HIGH, VERDICT_FAIL);
    } else {
        report_add_str(t, "consistency.extmem_cpu",
                       "pass (extended memory consistent with CPU class)",
                       CONF_HIGH, VERDICT_PASS);
    }
}

/* ----------------------------------------------------------------------- */

void consist_check(result_table_t *t)
{
    rule_486dx_implies_integrated_fpu(t);
    rule_486sx_no_integrated_fpu(t);
    rule_fpu_diag_bench_agreement(t);
    rule_extmem_implies_286(t);
    /*
     * Rules landing as downstream phases complete:
     *
     *   rule_386sx_bus_width              — needs bus.width in cpu_db
     *   rule_cpu_clock_independent_check  — needs BIOS-tick-based clock
     *                                        measurement (Task 4.2)
     *   rule_mips_in_class_ipc_range      — needs class_ipc values in
     *                                        cpu_db, needs bench mode
     *   rule_cache_stride_vs_cpuid_leaf2  — needs cache bench (Task 3.3)
     *                                        and CPUID leaf 2 decode
     *   rule_vga_bench_modes_available    — needs bench_video (Task 3.5)
     *
     * Each new rule should document the failure modes it catches AND
     * the ones it structurally cannot. Opacity is the anti-pattern.
     */
    (void)value_contains;  /* reserved for future string-matching rules */
}
