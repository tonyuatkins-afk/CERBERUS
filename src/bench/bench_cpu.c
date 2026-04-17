/*
 * CPU integer benchmark — Phase 3 Task 3.1.
 *
 * Quick-mode Dhrystone-adjacent integer-op mix. Not claiming Dhrystone
 * equivalence — that would require matching the canonical instruction
 * mix which is reverse-engineered folklore. Instead we pick a
 * deterministic mix, time it with PIT Channel 2, and report the
 * iteration rate honestly as what-CERBERUS-measured, not "MIPS."
 *
 * Per-iteration instruction mix:
 *   ADD, SUB, AND, OR, XOR, compare + conditional increment
 *
 * The `volatile` qualifiers on the working variables force Watcom to
 * keep each operation in the emitted code path — otherwise the
 * optimizer would hoist the entire loop body to a constant at
 * compile time, defeating the measurement.
 *
 * Iteration count auto-scales in calibrated mode but quick mode uses
 * a fixed 100K. On an 8088 at 4.77 MHz that's ~3 seconds (slow but
 * acceptable feedback); on a 486DX-33 it's ~15 ms; on a Pentium ~3 ms.
 * The 3 ms lower bound gives at least 3500 PIT ticks of resolution
 * (1.19 MHz * 3 ms = 3576 ticks) — plenty of precision.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"

#define BENCH_ITERS 100000L

static unsigned long run_int_loop(unsigned long iters)
{
    /* volatile keeps the optimizer from constant-folding the loop */
    volatile unsigned int a = 0x1234;
    volatile unsigned int b = 0x5678;
    volatile unsigned int c;
    volatile unsigned long hits = 0;
    unsigned long i;

    for (i = 0; i < iters; i++) {
        c = (unsigned int)(a + b);           /* ADD */
        c = (unsigned int)(c - (unsigned int)i); /* SUB with loop var */
        c = c & 0xFEDC;                      /* AND */
        c = c | 0x0123;                      /* OR */
        c = c ^ 0xA5A5;                      /* XOR */
        if (c == 0) hits++;                  /* CMP + conditional INC */
    }
    return hits;  /* returned so the compiler can't dead-code */
}

void bench_cpu(result_table_t *t)
{
    us_t elapsed;
    unsigned long iters_per_sec = 0;
    unsigned long us_x1000_per_iter;
    char buf[32];

    timing_start();
    (void)run_int_loop(BENCH_ITERS);
    elapsed = timing_stop();

    if (elapsed == 0) {
        /* Pathological — measurement came back at zero. Report without
         * a rate so the consistency engine can flag it. */
        report_add_str(t, "bench.cpu.int_ops",
                       "inconclusive (elapsed=0)",
                       CONF_LOW, VERDICT_WARN);
        return;
    }

    sprintf(buf, "%lu", (unsigned long)elapsed);
    report_add_u32(t, "bench.cpu.int_elapsed_us",
                   (unsigned long)elapsed, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);

    sprintf(buf, "%lu", BENCH_ITERS);
    report_add_u32(t, "bench.cpu.int_iterations",
                   BENCH_ITERS, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);

    /* iters_per_sec math designed to not overflow 32-bit unsigned long:
     *   elapsed is microseconds (up to ~4e9 = 71 minutes max).
     *   iters * 1000 fits in 32-bit for iters up to 4.29M.
     *   (iters * 1000) / (elapsed_us / 1000) = iters * 10^6 / elapsed
     *
     * Guard against elapsed < 1000 (sub-millisecond) by computing
     * us-per-iter-x1000 and deriving rate from that. */
    us_x1000_per_iter = ((unsigned long)elapsed * 1000UL) / BENCH_ITERS;
    if (us_x1000_per_iter > 0) {
        iters_per_sec = 1000000000UL / us_x1000_per_iter;
        sprintf(buf, "%lu", iters_per_sec);
        report_add_u32(t, "bench.cpu.int_iters_per_sec",
                       iters_per_sec, buf,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    /* Per-iteration time, three decimals */
    sprintf(buf, "%lu.%03lu",
            us_x1000_per_iter / 1000UL,
            us_x1000_per_iter % 1000UL);
    report_add_str(t, "bench.cpu.int_us_per_iter", buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
}
