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

/* All six emits are V_U32 — pass NULL as display and let
 * format_result_value format from r->v.u using "%lu". Identical output,
 * no static buffers to misallocate, no sprintf to fail. The same
 * approach in bench_fpu.c fixed a real-iron corruption (garbage bytes
 * in fpu.total_ops) that the R6 sweep missed — generalizing it here
 * closes the same bug class for bench_memory's copy_kbps (observed
 * garbage on the 486DX2-66 bench box). */

/* Compute KB/s given bytes-moved and elapsed microseconds.
 *
 * Previous implementation truncated `elapsed / 1000` (µs → ms), which
 * floored every sub-millisecond measurement to 0 then clamped to 1 ms.
 * Net effect: any memcpy/memset fast enough to finish in <1 ms reported
 * the same 4000 KB/s regardless of how fast it actually was. Observed
 * on the 486 DX-2 at turbo-on: write_us=189 and read_us=921 BOTH came
 * out as 4000 KB/s even though they were ~5× apart in wall time.
 *
 * New math works at microsecond precision end-to-end:
 *   KB/s = (bytes / 1024) * 1e6 / elapsed_us
 *
 * To stay in 32-bit: compute in two steps with intermediate scaling.
 *   step1 = bytes * 1000 / 1024   -- bytes-to-KB at 1-ms resolution
 *   result = step1 * 1000 / elapsed_us
 * For 4 KB at 189 µs:  step1 = 4000 → result = 4000*1000/189 ≈ 21164 KB/s.
 * For 16 KB at 100 µs: step1 = 15625 → 15625*1000/100 ≈ 156250 KB/s.
 * Largest intermediate: 65536*1000/1024 = 64000, then *1000 = 64M — fits
 * in unsigned long (max ~4.29 B). No overflow for any realistic bench. */
static unsigned long kb_per_sec(unsigned long bytes, us_t elapsed)
{
    unsigned long elapsed_us, step1;
    if (elapsed == 0) return 0;
    elapsed_us = (unsigned long)elapsed;
    step1 = (bytes * 1000UL) / 1024UL;
    return (step1 * 1000UL) / elapsed_us;
}

static void bench_write(result_table_t *t)
{
    us_t elapsed;
    unsigned long rate;

    timing_start();
    memset(mem_dst, 0xA5, MEM_BUF_BYTES);
    elapsed = timing_stop();

    rate = kb_per_sec((unsigned long)MEM_BUF_BYTES, elapsed);
    report_add_u32(t, "bench.memory.write_kbps", rate, (const char *)0,
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.memory.write_us", (unsigned long)elapsed,
                   (const char *)0, CONF_HIGH, VERDICT_UNKNOWN);
}

static void bench_copy(result_table_t *t)
{
    us_t elapsed;
    unsigned long rate;

    /* Seed src so we're copying real data (not zeros that might short-
     * circuit on some memory controllers) */
    memset(mem_src, 0x5A, MEM_BUF_BYTES);

    timing_start();
    memcpy(mem_dst, mem_src, MEM_BUF_BYTES);
    elapsed = timing_stop();

    rate = kb_per_sec((unsigned long)MEM_BUF_BYTES, elapsed);
    report_add_u32(t, "bench.memory.copy_kbps", rate, (const char *)0,
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.memory.copy_us", (unsigned long)elapsed,
                   (const char *)0, CONF_HIGH, VERDICT_UNKNOWN);
}

/* File-scope sink prevents DCE without the overhead of volatile-in-loop. */
static unsigned char bench_read_sink;

/* REP LODSB read helper. Symmetric to memset/memcpy which compile to
 * REP STOSB / REP MOVSB respectively; prior pure-C checksum loop added
 * ~20 cycles of spill/reload per byte that swamped the memory itself.
 *
 * Returns AL = last byte read. Caller MUST use the return value
 * (assigned to bench_read_sink below) so DCE can't elide the call.
 *
 * Watcom medium model: DS already points at DGROUP where mem_src lives,
 * so near-pointer SI suffices. CX = count. No register clobbers beyond
 * SI (moves) and AL (load target). */
extern unsigned char mem_rep_lodsb(const unsigned char *buf, unsigned int count);
#pragma aux mem_rep_lodsb = \
    "cld" \
    "rep lodsb" \
    parm [si] [cx] \
    value [al] \
    modify [];

static void bench_read(result_table_t *t)
{
    us_t elapsed;
    unsigned long rate;
    unsigned char last;

    /* Pre-fill so we're reading defined values */
    memset(mem_src, 0x3C, MEM_BUF_BYTES);

    timing_start();
    last = mem_rep_lodsb(mem_src, MEM_BUF_BYTES);
    elapsed = timing_stop();

    bench_read_sink = last;   /* prevent DCE of the call */

    rate = kb_per_sec((unsigned long)MEM_BUF_BYTES, elapsed);
    report_add_u32(t, "bench.memory.read_kbps", rate, (const char *)0,
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.memory.read_us", (unsigned long)elapsed,
                   (const char *)0, CONF_HIGH, VERDICT_UNKNOWN);
}

void bench_memory(result_table_t *t, const opts_t *o)
{
    /* Calibrated mode for memory bench lands as a follow-up — pattern
     * is the same as bench_cpu's N-pass loop with per-metric median
     * computation. Quick mode is used regardless of opts.mode for now. */
    (void)o;
    bench_write(t);
    bench_copy(t);
    bench_read(t);
}
