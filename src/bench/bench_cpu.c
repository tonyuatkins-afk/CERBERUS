/*
 * CPU integer benchmark — Phase 3 Task 3.1 + 3.5.
 *
 * Quick mode (opts.mode == MODE_QUICK): run the inner loop once, emit
 * iters_per_sec + us_per_iter.
 *
 * Calibrated mode (opts.mode == MODE_CALIBRATED, runs up to MAX_PASSES):
 * run the inner loop N times, capture each pass's elapsed_us into a
 * static array, emit per-pass values plus min/max/median/range. This
 * per-pass series is what Phase 4's thermal stability module consumes.
 *
 * Not claiming Dhrystone equivalence — the canonical instruction mix is
 * reverse-engineered folklore. We report what CERBERUS measured
 * honestly; the Phase 4 consistency engine will compare against
 * cpu_db's class_ipc ranges when those land.
 *
 * Per-iteration instruction mix (6 observable ops):
 *   ADD with variable b, SUB with loop index, AND, OR, XOR, CMP + conditional INC.
 * The `volatile` qualifiers force Watcom to emit each operation
 * individually rather than hoisting the loop body to a compile-time
 * constant.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"

#define BENCH_ITERS   100000L
#define MAX_PASSES    16      /* hard cap on calibrated runs */

static us_t pass_results[MAX_PASSES];

static unsigned long run_int_loop(unsigned long iters)
{
    volatile unsigned int a = 0x1234;
    volatile unsigned int b = 0x5678;
    volatile unsigned int c;
    volatile unsigned long hits = 0;
    unsigned long i;

    for (i = 0; i < iters; i++) {
        c = (unsigned int)(a + b);
        c = (unsigned int)(c - (unsigned int)i);
        c = c & 0xFEDC;
        c = c | 0x0123;
        c = c ^ 0xA5A5;
        if (c == 0) hits++;
    }
    return hits;
}

/* Insertion sort on a small array of us_t. N up to MAX_PASSES = 16,
 * so O(n²) is fine. */
static void sort_us(us_t *arr, int n)
{
    int i, j;
    us_t tmp;
    for (i = 1; i < n; i++) {
        tmp = arr[i];
        j = i - 1;
        while (j >= 0 && arr[j] > tmp) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = tmp;
    }
}

static void emit_single_pass(result_table_t *t, us_t elapsed)
{
    unsigned long us_x1000_per_iter;
    unsigned long iters_per_sec;
    char buf[32];

    sprintf(buf, "%lu", (unsigned long)elapsed);
    report_add_u32(t, "bench.cpu.int_elapsed_us",
                   (unsigned long)elapsed, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);

    sprintf(buf, "%lu", BENCH_ITERS);
    report_add_u32(t, "bench.cpu.int_iterations",
                   BENCH_ITERS, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);

    us_x1000_per_iter = ((unsigned long)elapsed * 1000UL) / BENCH_ITERS;
    if (us_x1000_per_iter > 0) {
        iters_per_sec = 1000000000UL / us_x1000_per_iter;
        sprintf(buf, "%lu", iters_per_sec);
        report_add_u32(t, "bench.cpu.int_iters_per_sec",
                       iters_per_sec, buf,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    sprintf(buf, "%lu.%03lu",
            us_x1000_per_iter / 1000UL,
            us_x1000_per_iter % 1000UL);
    report_add_str(t, "bench.cpu.int_us_per_iter", buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
}

static void emit_calibrated(result_table_t *t, int runs)
{
    us_t sorted[MAX_PASSES];
    us_t median, lo, hi;
    unsigned long range_pct_x10;
    int i;
    char buf[32];
    char key[40];

    /* Emit per-pass values first — this is what Phase 4 thermal
     * walks for monotonic drift detection. */
    for (i = 0; i < runs; i++) {
        sprintf(key, "bench.cpu.int_pass_%d_us", i);
        sprintf(buf, "%lu", (unsigned long)pass_results[i]);
        report_add_u32(t, key, (unsigned long)pass_results[i], buf,
                       CONF_HIGH, VERDICT_UNKNOWN);
        sorted[i] = pass_results[i];
    }

    sort_us(sorted, runs);
    lo     = sorted[0];
    hi     = sorted[runs - 1];
    median = sorted[runs / 2];

    sprintf(buf, "%lu", (unsigned long)lo);
    report_add_u32(t, "bench.cpu.int_min_us", (unsigned long)lo, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
    sprintf(buf, "%lu", (unsigned long)hi);
    report_add_u32(t, "bench.cpu.int_max_us", (unsigned long)hi, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
    sprintf(buf, "%lu", (unsigned long)median);
    report_add_u32(t, "bench.cpu.int_median_us", (unsigned long)median, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);

    /* range_pct_x10 = (hi - lo) * 1000 / median — gives one decimal place
     * of percent range with no floating point. Cheap substitute for CoV
     * until Phase 4's Mann-Kendall needs real stats. */
    if (median > 0) {
        range_pct_x10 = ((unsigned long)(hi - lo) * 1000UL) / (unsigned long)median;
        sprintf(buf, "%lu.%lu",
                range_pct_x10 / 10UL, range_pct_x10 % 10UL);
        report_add_str(t, "bench.cpu.int_range_pct", buf,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    /* Summary iters_per_sec derived from median for the canonical-
     * signature compatibility with quick mode. */
    {
        unsigned long us_x1000_per_iter =
            ((unsigned long)median * 1000UL) / BENCH_ITERS;
        if (us_x1000_per_iter > 0) {
            unsigned long iters_per_sec = 1000000000UL / us_x1000_per_iter;
            sprintf(buf, "%lu", iters_per_sec);
            report_add_u32(t, "bench.cpu.int_iters_per_sec",
                           iters_per_sec, buf,
                           CONF_HIGH, VERDICT_UNKNOWN);
        }
    }
}

void bench_cpu(result_table_t *t, const opts_t *o)
{
    int runs;
    int i;

    /* Always run at least one pass */
    if (o->mode == MODE_CALIBRATED && o->runs > 1) {
        runs = (int)o->runs;
        if (runs > MAX_PASSES) runs = MAX_PASSES;
    } else {
        runs = 1;
    }

    for (i = 0; i < runs; i++) {
        timing_start();
        (void)run_int_loop(BENCH_ITERS);
        pass_results[i] = timing_stop();

        if (pass_results[i] == 0) {
            /* One inconclusive pass in calibrated mode — note it, keep
             * going. A single-pass quick run that comes back zero is
             * harder to call; degrade to warn. */
            if (runs == 1) {
                report_add_str(t, "bench.cpu.int_ops",
                               "inconclusive (elapsed=0)",
                               CONF_LOW, VERDICT_WARN);
                return;
            }
        }
    }

    if (runs == 1) {
        emit_single_pass(t, pass_results[0]);
    } else {
        emit_calibrated(t, runs);
    }
}
