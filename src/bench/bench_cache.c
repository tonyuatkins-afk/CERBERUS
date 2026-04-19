/*
 * Cache bandwidth benchmark. Four throughput numbers — L1 and RAM, read
 * and write — all measured against the shared 2 KB / 32 KB FAR buffers
 * that diag_cache also consumes. See docs/plans/v0.4-benchmarks-and-polish.md §1.
 *
 * The L1/RAM distinction is size-based, not cache-aware: the small buffer
 * fits comfortably inside any 486-class L1, the large buffer exceeds it.
 * On hardware without an L1 cache (8088/V20/V30 floor class) both
 * measurements collapse to raw RAM and cease to discriminate — we skip
 * the bench entirely when detect_cache reports cache.present=no, matching
 * diag_cache's skip rule.
 *
 * What this benchmark measures, deliberately:
 *   - Sustained byte-stride throughput of C-compiled inner loops, which
 *     is representative of what typical DOS applications see — the same
 *     path Watcom's code generator produces for most loops.
 *
 * What it does NOT measure:
 *   - Peak cache bus bandwidth. REP STOSB over FAR pointers would show
 *     higher numbers but diverges from how real programs access memory.
 *     v0.5 can add a "peak" mode alongside the "sustained" numbers here.
 *   - Latency per line. diag_cache's stride-ratio test already covers
 *     that and publishes a PASS/WARN/FAIL verdict; bench_cache adds a
 *     continuous KB/s measurement for relative comparison.
 *
 * Timing choice: uses timing_start_long / timing_stop_long (BIOS-tick
 * based, ~55 ms resolution) because each of the four timed passes runs
 * for 1-3 seconds on a 486 DX-2, well past the 55 ms PIT single-wrap
 * limit that timing_start / timing_stop handles.
 *
 * Anti-DCE strategy mirrors diag_cache's stride_read_loop + diag_cache's
 * bench_dhrystone: a volatile in-loop accumulator plus a file-scope sink
 * that holds the final accumulator value, plus a per-iteration pattern
 * keyed off the iter index for the write loop. Watcom's optimizer cannot
 * prove the sinks are unread across translation units, so the loops stay
 * live under -ox.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"
#include "../core/cache_buffers.h"

/* Iteration counts calibrated for ~1-2 second runs on a 486 DX-2 so that
 * BIOS-tick resolution (~55 ms) becomes ~3% granularity. Slower hardware
 * takes longer and becomes more precise rather than less; the bench_cache
 * entry point skips entirely on XT-class floor hardware where the RAM
 * loop would take ~40 seconds. */
#define BENCH_CACHE_L1_ITERS    50000UL
#define BENCH_CACHE_RAM_ITERS     200UL

/* External-linkage sinks — same anti-DCE convention as diag_cache's
 * stride_sink and bench_dhrystone's checksum. Watcom cannot prove these
 * are unread across TUs. */
unsigned int  bench_cache_read_sink;
unsigned char bench_cache_write_sink;

static const result_t *find_local(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Pure math kernel — exposed so host tests exercise the rate arithmetic
 * without touching a PIT. Returns KB/s given bytes moved and microseconds
 * elapsed.
 *
 * bench_cache's envelope is substantially larger than bench_memory's:
 * 50,000 iters × 2 KB = 100 MB on the L1 loop, which would overflow
 * bench_memory's (bytes * 1000 / 1024) * 1000 intermediate (10^11).
 * Pre-divide by 1024 and by 1000 first so every intermediate stays
 * within 32 bits at the upper envelope:
 *
 *   kb          = bytes / 1024            max 10^5 at 100 MB total
 *   elapsed_ms  = elapsed_us / 1000       min 1 at bench_cache's 55 ms
 *                                         BIOS-tick floor
 *   result      = kb * 1000 / elapsed_ms  max 10^8 intermediate, fits
 *
 * Returns zero on degenerate input (zero bytes, zero elapsed, or sub-ms
 * elapsed). bench_cache uses timing_start_long / timing_stop_long whose
 * minimum measurable interval is one BIOS tick (~55,000 us), so the sub-
 * ms branch cannot fire in the real-hardware path. The defensive check
 * is there for host-test inputs and future timing-path changes. */
unsigned long bench_cache_kb_per_sec(unsigned long bytes, unsigned long elapsed_us)
{
    unsigned long kb;
    unsigned long elapsed_ms;
    if (elapsed_us == 0UL || bytes == 0UL) return 0UL;
    kb = bytes / 1024UL;
    if (kb == 0UL) return 0UL;             /* sub-KB — meaningless as KB/s */
    elapsed_ms = elapsed_us / 1000UL;
    if (elapsed_ms == 0UL) return 0UL;     /* sub-ms — bench_cache never hits this */
    return (kb * 1000UL) / elapsed_ms;
}

/* Byte-stride read loop over a FAR buffer. Accumulator is volatile in-
 * loop; file-scope sink holds the final value across TU boundaries. */
static unsigned int stride_read_loop(unsigned char __far *buf,
                                      unsigned int size,
                                      unsigned long iters)
{
    volatile unsigned int sum = 0;
    unsigned long i;
    unsigned int j;
    for (i = 0UL; i < iters; i++) {
        for (j = 0U; j < size; j++) {
            sum = (unsigned int)(sum + buf[j]);
        }
    }
    bench_cache_read_sink = sum;
    return sum;
}

/* Byte-stride write loop. Pattern is (iter_low_byte XOR offset_low_byte)
 * so every byte stored is a function of both loop indices; Watcom cannot
 * hoist the inner store outside the iter loop. After the loop, a sink
 * read of three widely-separated bytes forces the final iter's writes to
 * be observable. */
static void stride_write_loop(unsigned char __far *buf, unsigned int size,
                               unsigned long iters)
{
    unsigned long i;
    unsigned int j;
    unsigned char pattern;
    for (i = 0UL; i < iters; i++) {
        pattern = (unsigned char)(i & 0xFFUL);
        for (j = 0U; j < size; j++) {
            buf[j] = (unsigned char)(pattern ^ (unsigned char)j);
        }
    }
    bench_cache_write_sink = (unsigned char)(buf[0] ^
                                             buf[size / 2U] ^
                                             buf[size - 1U]);
}

static void bench_cache_one(result_table_t *t,
                             const char *key_kbps,
                             const char *key_us,
                             unsigned char __far *buf,
                             unsigned int size,
                             unsigned long iters,
                             int is_read)
{
    us_t elapsed;
    unsigned long total_bytes;
    unsigned long rate;

    timing_start_long();
    if (is_read) (void)stride_read_loop(buf, size, iters);
    else         stride_write_loop(buf, size, iters);
    elapsed = timing_stop_long();

    total_bytes = (unsigned long)size * iters;
    rate = bench_cache_kb_per_sec(total_bytes, (unsigned long)elapsed);
    report_add_u32(t, key_kbps, rate, (const char *)0,
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, key_us,   (unsigned long)elapsed, (const char *)0,
                   CONF_HIGH, VERDICT_UNKNOWN);
}

void bench_cache(result_table_t *t, const opts_t *o)
{
    const result_t *cache_present = find_local(t, "cache.present");
    const result_t *cpu_class;
    unsigned char __far *small_buf;
    unsigned char __far *large_buf;
    (void)o;

    /* Skip on hardware that cannot produce a meaningful L1-vs-RAM split.
     * The detect_cache path emits cache.present=no on 8088/V20/V30 floor
     * class AND on any AT-class CPU where CPUID / class-inference cannot
     * establish presence. Either case collapses both measurements to raw
     * RAM, which is already covered by bench_memory. */
    if (cache_present) {
        const char *cp = cache_present->display ? cache_present->display :
                         (cache_present->type == V_STR ? cache_present->v.s :
                          (const char *)0);
        if (cp && strcmp(cp, "no") == 0) {
            report_add_str(t, "bench.cache.status",
                           "skipped_no_cache_expected",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return;
        }
    }

    /* Second-gate on XT-class CPU class directly. Even if a miscalibrated
     * detect_cache path somehow reported present=yes on an XT board, the
     * 32 KB read loop would take over a minute at 4.77 MHz — skip rather
     * than hang the run. */
    cpu_class = find_local(t, "cpu.class");
    if (cpu_class && cpu_class->type == V_STR && cpu_class->v.s) {
        const char *cv = cpu_class->v.s;
        if (strcmp(cv, "8088") == 0 || strcmp(cv, "8086") == 0 ||
            strcmp(cv, "v20")  == 0 || strcmp(cv, "v30")  == 0) {
            report_add_str(t, "bench.cache.status",
                           "skipped_xt_class_cpu",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return;
        }
    }

    small_buf = cache_buffers_small();
    large_buf = cache_buffers_large();

    bench_cache_one(t, "bench.cache.l1_write_kbps",  "bench.cache.l1_write_us",
                    small_buf, CACHE_BUFFERS_SMALL_BYTES,
                    BENCH_CACHE_L1_ITERS, 0);
    bench_cache_one(t, "bench.cache.l1_read_kbps",   "bench.cache.l1_read_us",
                    small_buf, CACHE_BUFFERS_SMALL_BYTES,
                    BENCH_CACHE_L1_ITERS, 1);
    bench_cache_one(t, "bench.cache.ram_write_kbps", "bench.cache.ram_write_us",
                    large_buf, CACHE_BUFFERS_LARGE_BYTES,
                    BENCH_CACHE_RAM_ITERS, 0);
    bench_cache_one(t, "bench.cache.ram_read_kbps",  "bench.cache.ram_read_us",
                    large_buf, CACHE_BUFFERS_LARGE_BYTES,
                    BENCH_CACHE_RAM_ITERS, 1);

    /* Publish a composite checksum so a downstream consumer can detect
     * run-over-run reproducibility at the byte level (beyond the kbps
     * rate, which carries PIT-tick quantization noise). Also closes the
     * anti-DCE loop: report_add_u32 into a separate TU means Watcom
     * cannot prove the sinks are unused even with inter-procedural flow. */
    {
        unsigned long checksum = ((unsigned long)bench_cache_read_sink << 8) |
                                  (unsigned long)bench_cache_write_sink;
        report_add_u32(t, "bench.cache.checksum", checksum, (const char *)0,
                       CONF_LOW, VERDICT_UNKNOWN);
    }

    report_add_str(t, "bench.cache.status", "ok",
                   CONF_HIGH, VERDICT_UNKNOWN);
}
