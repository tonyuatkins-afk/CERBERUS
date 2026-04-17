/*
 * Thermal stability tracker — Phase 4 Task 4.5.
 *
 * Mann-Kendall non-parametric trend test on per-pass benchmark
 * series. Detects monotonic drift (upward trend = thermal
 * degradation slowing the CPU; downward trend = cold-start warmup)
 * without assuming Gaussian residuals or a minimum sample size
 * beyond N=5. No sqrt or variance computation required — the test
 * is rank-based.
 *
 * Mann-Kendall S statistic:
 *     S = sum over all pairs (i,j) with j > i of sign(x_j - x_i)
 *
 * Interpretation for benchmark timing (us per iteration — higher =
 * slower):
 *   S >= +critical  -> significantly slower over time (thermal degradation)
 *   S <= -critical  -> significantly faster over time (cold-start warmup)
 *   |S| < critical  -> no significant trend (stable)
 *
 * Two-tailed critical values at alpha=0.05 taken from the standard
 * Mann-Kendall reference table. Below N=5 the test has insufficient
 * statistical power to distinguish trend from noise; skip.
 *
 * Currently checks bench.cpu.int_pass_<i>_us. Additional per-subsystem
 * series (memory, FPU) can be added by extending the series-reader
 * helper to iterate multiple key prefixes.
 */

#include <stdio.h>
#include <string.h>
#include "thermal.h"
#include "report.h"

#define MAX_SERIES  16
#define MIN_SERIES   5

/* Two-tailed Mann-Kendall critical values, alpha = 0.05.
 * Indexed by N. Indexes 0..4 are unused (N<5 not supported). */
static const int mk_crit_2tail[MAX_SERIES + 1] = {
    0, 0, 0, 0, 0,
    10, 13, 17, 20, 24, 27,  /* N = 5..10 */
    31, 36, 40, 45, 49, 54   /* N = 11..16 */
};

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Try to read a consecutive pass series under key prefix "<prefix><i>_us".
 * Returns the number of passes successfully read; fills out[] with the
 * u32 values in order. */
static int read_series(const result_table_t *t,
                       const char *prefix_fmt,
                       unsigned long *out,
                       int max_n)
{
    int i;
    char key[64];
    for (i = 0; i < max_n; i++) {
        const result_t *r;
        sprintf(key, prefix_fmt, i);
        r = find_key(t, key);
        if (!r) break;
        out[i] = r->v.u;
    }
    return i;
}

static int mann_kendall_s(const unsigned long *x, int n)
{
    int s = 0;
    int i, j;
    for (i = 0; i < n - 1; i++) {
        for (j = i + 1; j < n; j++) {
            if      (x[j] > x[i]) s++;
            else if (x[j] < x[i]) s--;
        }
    }
    return s;
}

/* Static message buffers — report_add_str stores the pointer we pass
 * it, so the storage has to outlive the call site. String literals
 * work; stack-local sprintf buffers do not. Giving each analyze_series
 * call its own static keeps the output readable while respecting the
 * lifetime contract. */
static char thermal_cpu_msg[160];
static char thermal_cpu_s_val[16];

static void analyze_cpu_series(result_table_t *t,
                               const unsigned long *series,
                               int n)
{
    int s = mann_kendall_s(series, n);
    int abs_s = (s < 0) ? -s : s;
    int critical = mk_crit_2tail[n];

    sprintf(thermal_cpu_s_val, "%d", s);
    report_add_str(t, "thermal.cpu_s", thermal_cpu_s_val,
                   CONF_HIGH, VERDICT_UNKNOWN);

    if (abs_s < critical) {
        sprintf(thermal_cpu_msg,
                "pass (cpu bench: S=%d, |S|<%d, no significant trend at N=%d)",
                s, critical, n);
        report_add_str(t, "thermal.cpu", thermal_cpu_msg,
                       CONF_HIGH, VERDICT_PASS);
    } else if (s > 0) {
        sprintf(thermal_cpu_msg,
                "WARN: cpu bench: S=%d at N=%d exceeds critical %d (upward trend - possible thermal degradation)",
                s, n, critical);
        report_add_str(t, "thermal.cpu", thermal_cpu_msg,
                       CONF_HIGH, VERDICT_WARN);
    } else {
        sprintf(thermal_cpu_msg,
                "pass (cpu bench: S=%d at N=%d, downward trend consistent with cold-start warmup)",
                s, n);
        report_add_str(t, "thermal.cpu", thermal_cpu_msg,
                       CONF_HIGH, VERDICT_PASS);
    }
}

void thermal_check(result_table_t *t)
{
    unsigned long series[MAX_SERIES];
    int n;

    n = read_series(t, "bench.cpu.int_pass_%d_us", series, MAX_SERIES);
    if (n < MIN_SERIES) {
        /* No calibrated-mode data or not enough passes — rule not
         * applicable. Intentionally silent so quick-mode runs don't
         * carry a misleading "skipped" marker on every INI. */
        return;
    }
    analyze_cpu_series(t, series, n);

    /* Future series (uncomment as the corresponding calibrated-mode
     * benchmarks land):
     *
     *   n = read_series(t, "bench.memory.copy_pass_%d_us", series, MAX_SERIES);
     *   if (n >= MIN_SERIES) analyze_series(t, "thermal.memory_copy", ...);
     *
     *   n = read_series(t, "bench.fpu.pass_%d_us", series, MAX_SERIES);
     *   if (n >= MIN_SERIES) analyze_series(t, "thermal.fpu", ...);
     */
}
