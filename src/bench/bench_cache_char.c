/*
 * Cache characterization benchmark — v0.7.1 Phase 3 addition.
 *
 * Three probes inferring L1 cache parameters:
 *
 *   1. L1 size sweep        — stride-read throughput at 2/4/8/16/32 KB
 *                             working sets. Inflection = L1 boundary.
 *   2. Line size sweep      — stride-read throughput over a larger-than-L1
 *                             working set with varying stride (4..64 B).
 *                             Plateau point = line size.
 *   3. Write policy probe   — sequential write vs sequential read on a
 *                             small (fits-in-L1) buffer. Close rates =
 *                             write-back; write << read = write-through.
 *
 * Associativity is NOT probed in v0.7.1: reliable detection requires
 * precise physical-address control that medium-model DOS doesn't offer,
 * and emulators invalidate the result anyway. Reported as "not_probed"
 * with a follow-up note. See docs/plans/cache-char.md (future).
 *
 * Compiled at -od -oi (CFLAGS_NOOPT) for the same reason bench_cache /
 * bench_dhrystone / bench_whetstone are: prevent Watcom's optimizer from
 * hoisting loads or eliminating sinks. The per-iteration volatile observer
 * + external-linkage sinks are belt-and-braces.
 *
 * Skip rules mirror bench_cache / diag_cache:
 *   - fpu-irrelevant; runs regardless of FPU presence
 *   - SKIP if cache.present=no (XT-class / pre-486 hardware)
 *   - SKIP on 286/386 where the inner loops run multiple minutes
 *   - Emulator captures clamp confidence to CONF_LOW
 *
 * Uses cache_buffers (shared 2 KB + 32 KB FAR allocation) to avoid
 * duplicating storage with bench_cache and diag_cache.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"
#include "../core/cache_buffers.h"

/* --- Inference helpers (pure, host-testable) -------------------- */

/* Given five throughput numbers for increasing working-set sizes
 * (2/4/8/16/32 KB), return the working-set size (in KB) at which
 * throughput first dropped by > 15% vs the smallest working set. A drop
 * of this magnitude indicates the working set no longer fits in L1.
 *
 * Returns 0 if no drop was detected (everything fits in L1 or all data
 * missed). Returns the size at the first drop point otherwise.
 *
 * Calibration: 15% threshold set from the first real-iron capture on
 * BEK-V409 (486 DX2-66). The size_8kb -> size_16kb transition there
 * measured 1880 -> 1512 KB/s, a 20% drop clearly indicating the L1=8KB
 * boundary. A 30% threshold missed it. 15% is still well above the
 * run-to-run jitter we observed (<5% on static working sets) so false
 * positives on a steady-state are unlikely. */
unsigned int bench_cc_infer_l1_kb(const unsigned long *kbps_by_size,
                                  unsigned int n_sizes)
{
    static const unsigned int sizes_kb[] = { 2, 4, 8, 16, 32 };
    unsigned int i;
    unsigned long baseline;
    unsigned long threshold;

    if (!kbps_by_size || n_sizes < 2U) return 0;
    if (n_sizes > 5U) n_sizes = 5U;

    baseline = kbps_by_size[0];
    if (baseline == 0UL) return 0;

    /* Threshold = 85% of baseline. First point below this is the drop. */
    threshold = (baseline * 85UL) / 100UL;
    for (i = 1U; i < n_sizes; i++) {
        if (kbps_by_size[i] < threshold) {
            /* The L1 *fits* the previous size; *exceeds* this one.
             * Report the previous (last-fitting) size. */
            return sizes_kb[i - 1U];
        }
    }
    /* No drop found: either L1 is >= our largest probe, or we're running
     * cached throughout (shouldn't happen with 32 KB exceeding typical
     * 486 L1). Return 0 to signal "inconclusive". */
    return 0;
}

/* Given five throughput numbers for strides 4/8/16/32/64 on a
 * larger-than-L1 working set, infer the cache line size by finding
 * the plateau point.
 *
 * Shape: for strides < line_size, each line is touched multiple times
 * (some hits), so throughput is stride-sensitive. For strides >=
 * line_size, each access fetches a new line (all misses at this
 * working-set size), so throughput plateaus at a stride-invariant
 * floor.
 *
 * Detection: find the smallest stride S where kbps[S+1] / kbps[S]
 * and kbps[S+2] / kbps[S+1] are both within +/-8% of 1.0 (plateau
 * detected across two consecutive stride steps, guarding against a
 * single-point coincidence). The first stride that begins such a
 * plateau is the line size.
 *
 * Returns one of {4, 8, 16, 32, 64} bytes, or 0 if no plateau found. */
unsigned int bench_cc_infer_line_bytes(const unsigned long *kbps_by_stride,
                                       unsigned int n_strides)
{
    static const unsigned int strides[] = { 4, 8, 16, 32, 64 };
    unsigned int i;

    if (!kbps_by_stride || n_strides < 3U) return 0;
    if (n_strides > 5U) n_strides = 5U;

    /* Walk strides looking for the first index i where the ratios
     * kbps[i+1]/kbps[i] and kbps[i+2]/kbps[i+1] are both in [0.92, 1.08].
     * Integer math: a/b in [0.92, 1.08] iff |a-b| * 100 <= 8 * b. */
    for (i = 0U; i + 2U < n_strides; i++) {
        unsigned long a = kbps_by_stride[i];
        unsigned long b = kbps_by_stride[i + 1U];
        unsigned long c = kbps_by_stride[i + 2U];
        unsigned long diff_ab, diff_bc;

        if (a == 0UL || b == 0UL || c == 0UL) continue;

        diff_ab = (a > b) ? (a - b) : (b - a);
        diff_bc = (b > c) ? (b - c) : (c - b);

        if (diff_ab * 100UL <= 8UL * a &&
            diff_bc * 100UL <= 8UL * b) {
            return strides[i];
        }
    }
    return 0;
}

/* Classify write policy by the ratio of write throughput to read
 * throughput at the same stride. Return values correspond directly
 * to the emitted bench.cache.char.write_policy key.
 *
 *   write > 1.2x read     -> "wb"  (write-back with store buffer:
 *                                  writes queue up while the CPU
 *                                  continues; reads must complete.
 *                                  Observed 1.41x on BEK-V409 486DX2)
 *   write >= 75% of read  -> "wb"  (write-back: stores hit L1,
 *                                  comparable bandwidth to reads)
 *   write <  60% of read  -> "wt"  (write-through: every store
 *                                  pushes to DRAM)
 *   60..75%               -> "unknown" (ambiguous middle band)
 *
 * Thresholds: the write > read case is NOT pathological on a
 * write-back cache with a store buffer — it's the expected signature
 * when the buffer absorbs writes faster than L1 can serve reads.
 * Real-iron data from BEK-V409 (486 DX2-66, AMI BIOS WB strap):
 * wp_read=1960 KB/s, wp_write=2759 KB/s, ratio 1.41x. Classified as wb.
 *
 * 75% upper-bound of WB-like tracks real 486DX WB-strap and Pentium.
 * 60% lower-bound of WT-like matches Cyrix Cx486 and Intel WT-strapped
 * 486s where every store fences through to DRAM. */
const char *bench_cc_infer_write_policy(unsigned long read_kbps,
                                        unsigned long write_kbps)
{
    if (read_kbps == 0UL || write_kbps == 0UL) return "unknown";

    /* write > read: store buffer flushing during the write loop.
     * Still write-back; the buffer is the giveaway. */
    if (write_kbps > read_kbps) return "wb";

    /* write >= 75% of read  <=>  write * 4 >= read * 3 */
    if (write_kbps * 4UL >= read_kbps * 3UL) return "wb";

    /* write < 60% of read   <=>  write * 5 < read * 3 */
    if (write_kbps * 5UL < read_kbps * 3UL)  return "wt";

    return "unknown";
}

/* --- Host-test stop point --------------------------------------- */
#ifndef CERBERUS_HOST_TEST

/* External-linkage sinks: stronger than static sinks for -od defense —
 * Watcom cannot prove these reads are dead across TUs, so the load is
 * forced to complete. Volatile qualifier on the accumulator belts the
 * per-iteration visibility. */
volatile unsigned long bench_cc_read_sink  = 0;
volatile unsigned long bench_cc_write_sink = 0;
volatile unsigned int  bench_cc_iter_obs   = 0;

/* --- Inner kernels ----------------------------------------------- */

/* Stride-read loop over an N-byte working set with STRIDE-byte jumps.
 * Runs ITERS total iterations (wrapping modulo the working-set size).
 * Writes the final accumulated value into bench_cc_read_sink so Watcom
 * cannot prove the reads dead. */
static void stride_read(unsigned char __far *buf, unsigned int working_set,
                        unsigned int stride, unsigned long iters)
{
    unsigned long acc = 0;
    unsigned int  off = 0;
    unsigned long i;

    for (i = 0UL; i < iters; i++) {
        bench_cc_iter_obs = (unsigned int)i;
        acc += buf[off];
        off += stride;
        if (off >= working_set) off -= working_set;
    }
    bench_cc_read_sink = acc;
}

/* Stride-write loop — writes a byte derived from the iteration index
 * into each slot. */
static void stride_write(unsigned char __far *buf, unsigned int working_set,
                         unsigned int stride, unsigned long iters)
{
    unsigned int  off = 0;
    unsigned long i;

    for (i = 0UL; i < iters; i++) {
        bench_cc_iter_obs = (unsigned int)i;
        buf[off] = (unsigned char)(i & 0xFFU);
        off += stride;
        if (off >= working_set) off -= working_set;
    }
    bench_cc_write_sink = iters;
}

/* Compute KB/s from bytes + microseconds. 0 on degenerate input.
 * kbps = (bytes / 1024) / (us / 1_000_000) -> approximation
 *      = bytes * 1000 / us
 * Systematic ~2.4% overestimate (1000 vs 1024); fine for relative
 * comparison which is all the inference helpers need. */
static unsigned long bytes_per_us_to_kbps(unsigned long bytes, us_t us)
{
    if (us == 0UL) return 0UL;
    return (bytes * 1000UL) / (unsigned long)us;
}

/* --- Result-table lookup helper ---------------------------------- */

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Return nonzero if this CPU class can't reasonably run a 32 KB cache
 * sweep in <10 sec — matches bench_cache's skip rule. */
static int cpu_too_slow_for_char(const result_table_t *t)
{
    const result_t *r = find_key(t, "cpu.class");
    if (!r || r->type != V_STR) return 0;
    /* Skip XT-class (8088/8086) and 286/386 — stride loops run minutes.
     * Allow 486-no-cpuid and cpuid-class (Pentium+). */
    if (strcmp(r->v.s, "8088") == 0) return 1;
    if (strcmp(r->v.s, "8086") == 0) return 1;
    if (strcmp(r->v.s, "286")  == 0) return 1;
    if (strcmp(r->v.s, "386")  == 0) return 1;
    return 0;
}

/* Return nonzero if Phase 1 flagged this as an emulator capture. */
static int is_emulator(const result_table_t *t)
{
    const result_t *r = find_key(t, "environment.emulator");
    return (r && r->type == V_STR && strcmp(r->v.s, "none") != 0) ? 1 : 0;
}

/* --- Display buffers (static lifetime, per key) ------------------ */

static char l1_kb_display[8];
static char line_bytes_display[8];
static char read_kbps_display[12];
static char write_kbps_display[12];
static char size_kbps_displays[5][12];     /* one per size sweep point */
static char stride_kbps_displays[5][12];   /* one per stride sweep point */

/* --- Entry point ------------------------------------------------- */

void bench_cache_char(result_table_t *t)
{
    static const unsigned int sizes_kb[5]   = { 2, 4, 8, 16, 32 };
    static const unsigned int strides_b[5]  = { 4, 8, 16, 32, 64 };
    /* Iterations tuned for a single-PIT-wrap measurement window
     * (<55 ms per kernel) on a 486 DX-2. The first real-iron v0.7.1
     * capture caught a bug: timing_start_long/stop_long (BIOS tick,
     * ~55 ms resolution) was too coarse — 20K-iter kernels finish in
     * <5 ms, so most measurements rounded to 1 tick and produced
     * identical 364 KB/s "values" that meant "faster than the timer."
     * PIT C2 (838 ns resolution) gives us three orders of magnitude
     * more precision. At ~20 cycles per iter on DX2-66, 20K iters run
     * in ~6 ms — plenty of PIT ticks for a stable read, well clear of
     * the 55 ms C2 wrap limit even at 5x slower CPUs. */
    static const unsigned long iters_per_measure = 20000UL;

    unsigned long size_kbps[5]   = { 0, 0, 0, 0, 0 };
    unsigned long stride_kbps[5] = { 0, 0, 0, 0, 0 };
    unsigned long read_kbps      = 0;
    unsigned long write_kbps     = 0;
    unsigned int  l1_kb;
    unsigned int  line_bytes;
    const char *  write_policy;
    confidence_t  conf;
    unsigned int  i;
    us_t          us;
    unsigned char __far *large_buf;
    unsigned char __far *small_buf;

    /* Skip on hardware too slow. */
    if (cpu_too_slow_for_char(t)) {
        report_add_str(t, "bench.cache.char.status",
                       "skipped (pre-486 CPU)",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }
    /* Skip when cache is reported absent. */
    {
        const result_t *r = find_key(t, "cache.present");
        if (r && r->type == V_STR && strcmp(r->v.s, "no") == 0) {
            report_add_str(t, "bench.cache.char.status",
                           "skipped (no cache)",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return;
        }
    }
    /* Skip on emulator captures: stride probes rely on cache-miss
     * penalty shapes that DOSBox Staging and kin do not faithfully
     * reproduce (the emulator's host cache dominates, and the guest
     * probe measures host-level effects). Real-hardware only for
     * v0.7.1; a follow-up release may add an emulator-aware probe
     * shape that works around the fidelity gap. */
    if (is_emulator(t)) {
        report_add_str(t, "bench.cache.char.status",
                       "skipped (emulator, probe unreliable)",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }

    cache_buffers_reset();
    large_buf = cache_buffers_large();
    small_buf = cache_buffers_small();

    conf = CONF_MEDIUM;

    /* ----- Probe 1: L1 size sweep (strided reads, 2..32 KB) ----- */
    for (i = 0U; i < 5U; i++) {
        unsigned int working_set = sizes_kb[i] * 1024U;
        timing_start();
        stride_read(large_buf, working_set, 16U, iters_per_measure);
        us = timing_stop();
        size_kbps[i] = bytes_per_us_to_kbps(iters_per_measure, us);
        sprintf(size_kbps_displays[i], "%lu", size_kbps[i]);
    }
    /* Emit each measurement under a fixed key. String literals give
     * the required static lifetime since report_add_u32 stores the
     * key pointer, not a copy. */
    report_add_u32(t, "bench.cache.char.size_2kb_kbps",
                   size_kbps[0], size_kbps_displays[0], conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.size_4kb_kbps",
                   size_kbps[1], size_kbps_displays[1], conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.size_8kb_kbps",
                   size_kbps[2], size_kbps_displays[2], conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.size_16kb_kbps",
                   size_kbps[3], size_kbps_displays[3], conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.size_32kb_kbps",
                   size_kbps[4], size_kbps_displays[4], conf, VERDICT_UNKNOWN);

    l1_kb = bench_cc_infer_l1_kb(size_kbps, 5U);
    if (l1_kb > 0U) {
        sprintf(l1_kb_display, "%u", l1_kb);
        report_add_u32(t, "bench.cache.char.l1_kb",
                       (unsigned long)l1_kb, l1_kb_display,
                       conf, VERDICT_UNKNOWN);
    } else {
        report_add_str(t, "bench.cache.char.l1_kb", "inconclusive",
                       CONF_LOW, VERDICT_UNKNOWN);
    }

    /* ----- Probe 2: line size sweep (strides 4..64, 32 KB buffer) ----- */
    for (i = 0U; i < 5U; i++) {
        timing_start();
        stride_read(large_buf, CACHE_BUFFERS_LARGE_BYTES,
                    strides_b[i], iters_per_measure);
        us = timing_stop();
        stride_kbps[i] = bytes_per_us_to_kbps(iters_per_measure, us);
        sprintf(stride_kbps_displays[i], "%lu", stride_kbps[i]);
    }
    report_add_u32(t, "bench.cache.char.stride_4_kbps",
                   stride_kbps[0], stride_kbps_displays[0], conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.stride_8_kbps",
                   stride_kbps[1], stride_kbps_displays[1], conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.stride_16_kbps",
                   stride_kbps[2], stride_kbps_displays[2], conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.stride_32_kbps",
                   stride_kbps[3], stride_kbps_displays[3], conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.stride_64_kbps",
                   stride_kbps[4], stride_kbps_displays[4], conf, VERDICT_UNKNOWN);

    line_bytes = bench_cc_infer_line_bytes(stride_kbps, 5U);
    if (line_bytes > 0U) {
        sprintf(line_bytes_display, "%u", line_bytes);
        report_add_u32(t, "bench.cache.char.line_bytes",
                       (unsigned long)line_bytes, line_bytes_display,
                       conf, VERDICT_UNKNOWN);
    } else {
        report_add_str(t, "bench.cache.char.line_bytes", "inconclusive",
                       CONF_LOW, VERDICT_UNKNOWN);
    }

    /* ----- Probe 3: write policy (read vs write on 2 KB buf) ----- */
    timing_start();
    stride_read(small_buf, CACHE_BUFFERS_SMALL_BYTES,
                16U, iters_per_measure);
    us = timing_stop();
    read_kbps = bytes_per_us_to_kbps(iters_per_measure, us);

    timing_start();
    stride_write(small_buf, CACHE_BUFFERS_SMALL_BYTES,
                 16U, iters_per_measure);
    us = timing_stop();
    write_kbps = bytes_per_us_to_kbps(iters_per_measure, us);

    sprintf(read_kbps_display,  "%lu", read_kbps);
    sprintf(write_kbps_display, "%lu", write_kbps);
    report_add_u32(t, "bench.cache.char.wp_read_kbps",
                   read_kbps,  read_kbps_display,  conf, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.cache.char.wp_write_kbps",
                   write_kbps, write_kbps_display, conf, VERDICT_UNKNOWN);

    write_policy = bench_cc_infer_write_policy(read_kbps, write_kbps);
    report_add_str(t, "bench.cache.char.write_policy", write_policy,
                   conf, VERDICT_UNKNOWN);

    /* ----- Associativity: deferred to a follow-up ---------------- */
    report_add_str(t, "bench.cache.char.assoc", "not_probed",
                   CONF_HIGH, VERDICT_UNKNOWN);

    report_add_str(t, "bench.cache.char.status", "ok",
                   CONF_HIGH, VERDICT_UNKNOWN);
}

#endif  /* CERBERUS_HOST_TEST */
