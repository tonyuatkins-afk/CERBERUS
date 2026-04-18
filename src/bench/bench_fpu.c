/*
 * FPU benchmark — Phase 3 Task 3.4.
 *
 * Whetstone-adjacent x87 operation mix — FADD/FSUB/FMUL/FDIV on
 * exactly-representable doubles so the work doesn't degenerate into
 * NaN or underflow handling on edge cases. Not claiming Whetstone
 * equivalence for the same reasons bench_cpu isn't claiming Dhrystone
 * equivalence.
 *
 * Skipped entirely if detect_fpu reported `fpu.detected=none` —
 * running this on a no-FPU system would silently invoke Watcom's FPU
 * emulator and report the emulator's speed, not the target hardware.
 *
 * Shares the FPU runtime that was already linked in by diag_fpu — so
 * adding this module costs only its own instruction-mix code, not the
 * full emulator pull.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"

/* V_U32 emits pass NULL as display and let format_result_value format
 * from r->v.u directly — identical output ("%lu") and eliminates a
 * class of sprintf/static-buffer lifetime bugs. The V_STR emit
 * (us_per_op below) still needs its own static because report's V_STR
 * path stores the string pointer and has no fallback. */
static char bench_fpu_us_per_op_val[32];

#define FPU_ITERS 10000L

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

static double run_fpu_loop(unsigned long iters)
{
    /* volatile on the accumulator so the optimizer can't precompute */
    volatile double acc = 1.0;
    double a = 2.0;
    double b = 4.0;
    double c = 0.5;
    unsigned long i;

    for (i = 0; i < iters; i++) {
        acc = acc + a;   /* FADD */
        acc = acc * c;   /* FMUL */
        acc = acc - b;   /* FSUB */
        acc = acc / a;   /* FDIV */
    }
    return acc;
}

void bench_fpu(result_table_t *t, const opts_t *o)
{
    /* Calibrated FPU bench lands as follow-up — pattern is shared with
     * bench_cpu's N-pass loop. */
    const result_t *fpu_entry;
    const char *fpu_val;
    us_t elapsed;
    unsigned long ops_per_sec;
    unsigned long total_ops;
    unsigned long us_x1000_per_op;

    (void)o;
    fpu_entry = find_key(t, "fpu.detected");
    if (!fpu_entry) return;
    fpu_val = fpu_entry->display ? fpu_entry->display :
              (fpu_entry->type == V_STR ? fpu_entry->v.s : "");
    if (!fpu_val || strcmp(fpu_val, "none") == 0) {
        report_add_str(t, "bench.fpu", "skipped (no FPU)",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }

    timing_start();
    (void)run_fpu_loop(FPU_ITERS);
    elapsed = timing_stop();

    if (elapsed == 0) {
        report_add_str(t, "bench.fpu.mix", "inconclusive (elapsed=0)",
                       CONF_LOW, VERDICT_WARN);
        return;
    }

    /* 4 FPU ops per iteration */
    total_ops = FPU_ITERS * 4UL;
    us_x1000_per_op = ((unsigned long)elapsed * 1000UL) / total_ops;

    report_add_u32(t, "bench.fpu.elapsed_us", (unsigned long)elapsed,
                   (const char *)0, CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.fpu.total_ops", total_ops,
                   (const char *)0, CONF_HIGH, VERDICT_UNKNOWN);

    if (us_x1000_per_op > 0) {
        ops_per_sec = 1000000000UL / us_x1000_per_op;
        report_add_u32(t, "bench.fpu.ops_per_sec", ops_per_sec,
                       (const char *)0, CONF_HIGH, VERDICT_UNKNOWN);
    }

    sprintf(bench_fpu_us_per_op_val, "%lu.%03lu",
            us_x1000_per_op / 1000UL,
            us_x1000_per_op % 1000UL);
    report_add_str(t, "bench.fpu.us_per_op", bench_fpu_us_per_op_val,
                   CONF_HIGH, VERDICT_UNKNOWN);
}
