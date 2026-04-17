/*
 * Memory bandwidth benchmark — Phase 3 Task 3.2.
 *
 * Three measurements on a 16KB DGROUP buffer pair:
 *
 *   write  — byte-fill loop (equivalent to REP STOSB); stresses write
 *            path from register to memory
 *   copy   — src → dst byte copy (equivalent to REP MOVSB); stresses
 *            both read and write simultaneously
 *   read   — scan-over-buffer with running checksum; stresses read
 *            path while forcing result-use so the loop can't be
 *            optimized away
 *
 * Buffer sized at 16KB — fits in L1 on every CERBERUS-target CPU that
 * has L1 cache (486+ typically has 8-16KB). Phase 3 Task 3.3 (cache
 * bandwidth) will run similar tests at escalating buffer sizes to find
 * cache knees.
 *
 * Reports KB/s for each operation. Raw timing in us also emitted so
 * Phase 4 thermal tracking can compute per-pass drift across
 * calibrated runs.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"

#define MEM_BUF_BYTES 4096   /* Kept modest so DGROUP stays under 64KB in
                              * medium model. Phase 3 Task 3.3 (cache
                              * bandwidth) will use FAR buffers outside
                              * DGROUP for the larger working-set
                              * sweeps. */

static unsigned char mem_src[MEM_BUF_BYTES];
static unsigned char mem_dst[MEM_BUF_BYTES];

/* Compute KB/s given bytes-moved and elapsed microseconds.
 * bytes_per_us * 1e6 / 1024 = bytes_per_us * 976.5625
 * Scale to avoid overflow: for 16KB at 10µs (fast), that's 1,600,000
 * KB/s (1.6 GB/s) — well within 32-bit range. For slow 8088 runs at
 * say 50KB/s, we're fine at any realistic buffer/time combo. */
static unsigned long kb_per_sec(unsigned long bytes, us_t elapsed)
{
    unsigned long elapsed_ms;
    if (elapsed == 0) return 0;
    elapsed_ms = (unsigned long)elapsed / 1000UL;
    if (elapsed_ms == 0) elapsed_ms = 1;  /* sub-ms rounds up */
    return (bytes * 1000UL / 1024UL) / elapsed_ms;
}

static void bench_write(result_table_t *t)
{
    us_t elapsed;
    unsigned long rate;
    char buf[32];

    timing_start();
    memset(mem_dst, 0xA5, MEM_BUF_BYTES);
    elapsed = timing_stop();

    rate = kb_per_sec((unsigned long)MEM_BUF_BYTES, elapsed);
    sprintf(buf, "%lu", rate);
    report_add_u32(t, "bench.memory.write_kbps", rate, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
    sprintf(buf, "%lu", (unsigned long)elapsed);
    report_add_u32(t, "bench.memory.write_us", (unsigned long)elapsed, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
}

static void bench_copy(result_table_t *t)
{
    us_t elapsed;
    unsigned long rate;
    char buf[32];

    /* Seed src so we're copying real data (not zeros that might short-
     * circuit on some memory controllers) */
    memset(mem_src, 0x5A, MEM_BUF_BYTES);

    timing_start();
    memcpy(mem_dst, mem_src, MEM_BUF_BYTES);
    elapsed = timing_stop();

    rate = kb_per_sec((unsigned long)MEM_BUF_BYTES, elapsed);
    sprintf(buf, "%lu", rate);
    report_add_u32(t, "bench.memory.copy_kbps", rate, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
    sprintf(buf, "%lu", (unsigned long)elapsed);
    report_add_u32(t, "bench.memory.copy_us", (unsigned long)elapsed, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
}

static void bench_read(result_table_t *t)
{
    us_t elapsed;
    unsigned long rate;
    volatile unsigned long checksum = 0;
    unsigned int i;
    char buf[32];

    /* Pre-fill so we're reading defined values */
    memset(mem_src, 0x3C, MEM_BUF_BYTES);

    timing_start();
    /* Read every byte, fold into volatile checksum so the optimizer
     * can't elide the reads */
    for (i = 0; i < MEM_BUF_BYTES; i++) {
        checksum += mem_src[i];
    }
    elapsed = timing_stop();

    rate = kb_per_sec((unsigned long)MEM_BUF_BYTES, elapsed);
    sprintf(buf, "%lu", rate);
    report_add_u32(t, "bench.memory.read_kbps", rate, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
    sprintf(buf, "%lu", (unsigned long)elapsed);
    report_add_u32(t, "bench.memory.read_us", (unsigned long)elapsed, buf,
                   CONF_HIGH, VERDICT_UNKNOWN);
}

void bench_memory(result_table_t *t)
{
    bench_write(t);
    bench_copy(t);
    bench_read(t);
}
