/*
 * Cache bandwidth benchmark. Four throughput numbers — small-buffer and
 * large-buffer, read and write — all measured against the shared 2 KB /
 * 32 KB FAR buffers that diag_cache also consumes. See
 * docs/plans/v0.4-benchmarks-and-polish.md §1.
 *
 * KEYS ARE SIZE-BASED, NOT CACHE-AWARE.
 *
 * The small/large naming is deliberate. An earlier iteration used l1_*
 * and ram_* keys, which implied that small_* always measures L1 and
 * large_* always measures DRAM. That is true on write-BACK 486 caches
 * (where stores hit L1 and only write back on eviction) but FALSE on
 * write-THROUGH 486 caches (the Cyrix Cx486DX line, some Intel i486 SX
 * steppings, and every 486 variant with the BIOS cache-policy strap set
 * to write-through): every store pushes through to DRAM regardless of
 * cache presence, so "small_write" throughput collapses to raw DRAM-
 * write bandwidth and the l1_write label would mislead.
 *
 * What the keys actually measure:
 *   - bench.cache.small_{read,write}_kbps: stride-byte loop over a 2 KB
 *     buffer that fits comfortably inside any 486-class L1. Reads hit
 *     L1; writes hit L1 on write-back, DRAM on write-through.
 *   - bench.cache.large_{read,write}_kbps: stride-byte loop over a 32 KB
 *     buffer that exceeds any plausible 486-class L1. Both reads and
 *     writes miss L1 and hit main memory.
 *
 * Consumers that need to INFER cache policy cross-reference the four
 * rates: large_read/small_read gives the read cache multiplier, while
 * the presence-or-absence of a comparable small_write/large_write gap
 * reveals write-back vs write-through (rule is deferred to v0.5).
 *
 * On hardware without an L1 cache (8088/V20/V30 floor class) both
 * measurements collapse to raw RAM and cease to discriminate — we skip
 * the bench entirely when detect_cache reports cache.present=no, matching
 * diag_cache's skip rule. We also skip on XT-class and pre-486 AT-class
 * CPUs (286/386) where the large-buffer loop would take multiple minutes
 * at 1-8 MHz.
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
 * Anti-DCE strategy mirrors bench_memory's bench_read_sink (file-scope,
 * external linkage sink for the accumulator) for read loops, and adds a
 * per-iteration volatile observer for write loops so Watcom's -ox DSE
 * cannot hoist all but the final iteration's writes outside the outer
 * loop. Read loops do NOT use volatile-in-loop; see stride_read_loop.
 *
 * The final `bench.cache.checksum` value is a 16-bit composite
 * reproducibility signal (read-sink low byte << 8 | write-sink byte) —
 * useful for detecting run-over-run drift, but NOT a byte-level
 * fingerprint of the buffer contents. See the comment at the publish
 * site below.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"
#include "../core/cache_buffers.h"

/* Iteration counts calibrated for ~1-5 second runs across the 486 family
 * so that BIOS-tick resolution (~55 ms) stays below ~10% granularity
 * even on the fastest 486-class hardware:
 *
 *   - Write-back 486 DX-2 (Intel i486DX2): small_read / small_write
 *     ~1.2 sec (~22 BIOS ticks = ~4.5% resolution). large_* at 400
 *     iters ~2-3 sec (plenty of ticks = <2% resolution).
 *
 *   - Write-through 486 (Cyrix Cx486DX, strap-set write-through Intel):
 *     small_write goes through to DRAM at ~15 MB/s sustained, producing
 *     a ~4-5 sec runtime at 40k iters (~80-90 ticks = ~1.2% resolution).
 *     Still inside the "quick" bench budget.
 *
 *   - AMD 5x86 / 486 DX-4-100 (fastest 486-class silicon): small_*
 *     ~0.6 sec (~11 BIOS ticks = ~9% resolution). Stays above the
 *     single-digit-tick floor where ±1-tick edge-race noise would
 *     dominate. A prior 20k-iter calibration dropped this class to
 *     ~5 ticks and ~20% noise.
 *
 *   - XT-class / 286 / 386: SKIPPED via the gates in bench_cache(). The
 *     large-buffer loop at 400 iters would run multiple minutes at 4.77
 *     to 33 MHz. Any auto-calibration would still need a skip for these
 *     classes; explicit skip is clearer.
 *
 * TODO (v0.5): add auto-calibration that picks iters at runtime based on
 * an initial short warm-up pass, then scales to hit a target ~1.5 sec
 * wall-time per measurement. Until then, the fixed-iters approach above
 * is tuned for the 486 hardware that forms the bulk of real-iron captures.
 */
#define BENCH_CACHE_SMALL_ITERS   40000UL
#define BENCH_CACHE_LARGE_ITERS     400UL

/* External-linkage sinks — same anti-DCE convention as diag_cache's
 * stride_sink and bench_memory's bench_read_sink. Watcom cannot prove
 * these are unread across TUs. */
unsigned int  bench_cache_read_sink;
unsigned char bench_cache_write_sink;

/* Per-iteration volatile observer for the write loop. File-scope AND
 * volatile AND qualified by external linkage — Watcom -ox cannot prove
 * this is unobserved between iterations, so it cannot fold the inner
 * write loop across iterations (classic cross-iteration DSE). Matches
 * bench_dhrystone's per-iter volatile-sink pattern. */
volatile unsigned char bench_cache_iter_observer;

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
 * 40,000 iters × 2 KB = 80 MB on the small loop, which still would
 * overflow bench_memory's (bytes * 1000 / 1024) * 1000 intermediate.
 * Pre-divide by 1024 and by 1000 first so every intermediate stays
 * within 32 bits at the upper envelope (the kernel was originally
 * dimensioned for a 100 MB total-bytes ceiling, so 80 MB is well
 * inside the verified overflow envelope):
 *
 *   kb          = bytes / 1024            max 8×10^4 at 80 MB total
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

/* Byte-stride read loop over a FAR buffer. The accumulator is NON-volatile
 * — the DCE defense is TWO-LEGGED and requires BOTH legs to stand:
 *
 *   1. The buffer is reached only via a cross-TU function call
 *      (cache_buffers_small() / cache_buffers_large() in core/cache_buffers.c)
 *      so Watcom cannot prove the contents compile-time constant and
 *      cannot fold the reads.
 *
 *   2. The final bench_cache_read_sink store has external linkage, so
 *      Watcom cannot prove `sum` is unused cross-TU and must keep the
 *      accumulator live — which keeps the whole loop live.
 *
 * WARNING: if either leg collapses — e.g., cache_buffers_small() becomes
 * `static inline` in a future refactor, OR bench_cache_read_sink picks up
 * the `static` qualifier — the defense collapses and Watcom -ox may
 * eliminate the entire loop. In that case the inner `sum` must be made
 * `volatile`, paying the per-byte DGROUP read-modify-write cost (adds
 * ~3 cycles on an L1-cached ~2-cycle operation, biasing small-buffer
 * read kbps 2-3× low). Matches bench_memory's bench_read_sink pattern
 * (bench_memory.c:112,145). */
static unsigned int stride_read_loop(unsigned char __far *buf,
                                      unsigned int size,
                                      unsigned long iters)
{
    unsigned int sum = 0;  /* non-volatile — DCE defense via external-
                            * linkage bench_cache_read_sink below. */
    unsigned long i;
    unsigned int j;
    for (i = 0UL; i < iters; i++) {
        for (j = 0U; j < size; j++) {
            sum = (unsigned int)(sum + buf[j]);
        }
    }
    bench_cache_read_sink = sum;  /* Watcom cannot prove this sink is
                                   * unread cross-TU — the final store
                                   * keeps the whole loop live. */
    return sum;
}

/* Byte-stride write loop. Pattern is (iter_low_byte XOR offset_low_byte)
 * so every byte stored is a function of both loop indices; Watcom cannot
 * hoist the inner store outside the iter loop body.
 *
 * Cross-iteration DSE defense: a post-loop sink read alone would let
 * Watcom keep only the final iteration's stores (the only ones whose
 * values the sink could observe), collapsing the outer loop to a single
 * iteration and massively inflating the reported throughput. A
 * PER-ITERATION volatile observer read forces each iteration's writes to
 * be independently observable, matching bench_dhrystone's per-iter
 * volatile-sink pattern. The `size - 1U` mask works because both buffer
 * sizes are powers of two. */
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
        /* Per-iter volatile sink — forces each iteration's writes to be
         * observable in isolation; defeats cross-iteration dead-store
         * elimination that would fold the outer loop to one pass. */
        bench_cache_iter_observer = buf[(unsigned int)(i & (unsigned long)(size - 1U))];
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
    const result_t *cache_present;
    const result_t *cpu_class;
    unsigned char __far *small_buf;
    unsigned char __far *large_buf;
    /* bench_cache is quick-mode-only for v0.4 — the (void)o cast below
     * is intentional, not an oversight. Calibrated mode (opts->mode /
     * opts->runs) lands in v0.5 alongside the bar-graph comparison UI.
     * See bench.h's bench_cache prototype note. */
    (void)o;

    cache_present = find_local(t, "cache.present");
    cpu_class     = find_local(t, "cpu.class");

    /* Fail-safe gate: /ONLY:BENCH skips detect, so NEITHER cache.present
     * NOR cpu.class is in the table. Running the benchmark with no
     * detect-derived context is unsafe — an 8088 would take minutes per
     * measurement. Emit a distinct status and return; the user who passed
     * /ONLY:BENCH can re-run without the flag, or pair it with detect
     * via /MODE:FULL. */
    if (!cache_present && !cpu_class) {
        report_add_str(t, "bench.cache.status",
                       "skipped_detect_not_run",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }

    /* First gate: detect_cache reports cache.present=no on 8088/V20/V30
     * floor class AND on any AT-class CPU where CPUID / class-inference
     * cannot establish presence. Either case collapses both measurements
     * to raw RAM, which is already covered by bench_memory. */
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

    /* Second gate: skip on XT-class + pre-486 AT-class CPUs where the
     * large-buffer loop would run multiple minutes at 4.77-33 MHz.
     *
     * Coverage envelope — NOT "defense-in-depth against miscalibrated
     * detect." Both gates read from detect outputs, so a detect bug that
     * mislabels the CPU corrupts both in lockstep; one gate cannot
     * rescue the other from a wrong detect. The real value of this
     * second gate is catching the case where cache.present is MISSING
     * from the report table while cpu.class is PRESENT — e.g., a future
     * detect_cache refactor that drops cache.present emission for some
     * CPU families but leaves cpu.class intact. Without this gate that
     * scenario would fall through the first gate and run the 32 KB loop
     * on whatever CPU is under the hood. */
    if (cpu_class && cpu_class->type == V_STR && cpu_class->v.s) {
        const char *cv = cpu_class->v.s;
        if (strcmp(cv, "8088") == 0 || strcmp(cv, "8086") == 0 ||
            strcmp(cv, "v20")  == 0 || strcmp(cv, "v30")  == 0 ||
            strcmp(cv, "286")  == 0 || strcmp(cv, "386")  == 0) {
            report_add_str(t, "bench.cache.status",
                           "skipped_pre_486_class_cpu",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return;
        }
    }

    /* Explicit inter-caller ownership protocol — establish known-zero
     * state in both shared buffers before our measurement loops, so we
     * don't inherit whatever pattern diag_cache left behind (or, on an
     * /ONLY:BENCH run, uninitialized BSS fill). See cache_buffers.h. */
    cache_buffers_reset();

    small_buf = cache_buffers_small();
    large_buf = cache_buffers_large();

    bench_cache_one(t, "bench.cache.small_write_kbps",
                       "bench.cache.small_write_us",
                    small_buf, CACHE_BUFFERS_SMALL_BYTES,
                    BENCH_CACHE_SMALL_ITERS, 0);
    bench_cache_one(t, "bench.cache.small_read_kbps",
                       "bench.cache.small_read_us",
                    small_buf, CACHE_BUFFERS_SMALL_BYTES,
                    BENCH_CACHE_SMALL_ITERS, 1);
    bench_cache_one(t, "bench.cache.large_write_kbps",
                       "bench.cache.large_write_us",
                    large_buf, CACHE_BUFFERS_LARGE_BYTES,
                    BENCH_CACHE_LARGE_ITERS, 0);
    bench_cache_one(t, "bench.cache.large_read_kbps",
                       "bench.cache.large_read_us",
                    large_buf, CACHE_BUFFERS_LARGE_BYTES,
                    BENCH_CACHE_LARGE_ITERS, 1);

    /* Publish a composite 16-bit reproducibility signal so a downstream
     * consumer can detect run-over-run drift beyond the kbps rate (which
     * carries PIT-tick quantization noise). The read sink is `unsigned
     * int` (16-bit in Watcom medium model), so the top 8 bits of the
     * packed checksum come from the read accumulator's low byte and the
     * bottom 8 bits are the final write-sink XOR — a 16-bit signal, not
     * a per-byte fingerprint. Widening to unsigned long was considered
     * and rejected: it would cost 4 bytes DGROUP per sink and require
     * matching conversions in report_add_u32, which is not worth it for
     * a signal this loose. Also closes the anti-DCE loop: report_add_u32
     * into a separate TU means Watcom cannot prove the sinks are unused
     * even with inter-procedural flow. */
    {
        unsigned long checksum = ((unsigned long)bench_cache_read_sink << 8) |
                                  (unsigned long)bench_cache_write_sink;
        report_add_u32(t, "bench.cache.checksum", checksum, (const char *)0,
                       CONF_LOW, VERDICT_UNKNOWN);
    }

    report_add_str(t, "bench.cache.status", "ok",
                   CONF_HIGH, VERDICT_UNKNOWN);
}
