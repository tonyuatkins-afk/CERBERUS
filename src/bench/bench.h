#ifndef CERBERUS_BENCH_H
#define CERBERUS_BENCH_H

#include "../cerberus.h"

void bench_all(result_table_t *t, const opts_t *o);

/* Per-subsystem benchmark entry points. Each reads opts->mode to
 * decide between quick (one pass) and calibrated (opts->runs passes,
 * with per-pass values + min/max/median emitted for Phase 4 thermal
 * tracking to consume). Results go into the [bench.<subsys>] INI
 * section. */
void bench_cpu(result_table_t *t, const opts_t *o);
void bench_memory(result_table_t *t, const opts_t *o);
void bench_fpu(result_table_t *t, const opts_t *o);

/* v0.4 historical benchmarks — see docs/plans/v0.4-historical-benchmarks.md */
void bench_dhrystone(result_table_t *t, const opts_t *o);
void bench_whetstone(result_table_t *t, const opts_t *o);

/* v0.4 cache bandwidth — see docs/plans/v0.4-benchmarks-and-polish.md §1.
 *
 * Quick-mode ONLY for v0.4. Unlike the other bench_* entry points in
 * this header, bench_cache does NOT consult opts->mode or opts->runs;
 * the `(void)o` cast in its body is intentional. Calibrated-mode support
 * (N-pass with per-pass values + min/max/median) lands in v0.5 alongside
 * the bar-graph comparison UI, where the additional run data has a
 * presentation surface. Until then, a single quick-mode pass produces
 * one KB/s number per (size, direction) pair — four rows total. */
void bench_cache(result_table_t *t, const opts_t *o);

/* Pure math kernel used by bench_cache's emit path. Exposed for host
 * tests so the rate arithmetic can be exercised without PIT hardware.
 * bench_video reuses this kernel — the envelope (bytes ≤ 20 MB per
 * measurement, elapsed ≥ 1 BIOS tick) fits the same overflow bounds. */
unsigned long bench_cache_kb_per_sec(unsigned long bytes,
                                     unsigned long elapsed_us);

/* v0.7.1 cache characterization — see docs/plans/cache-char.md (future).
 * Infers L1 size, line size, and write policy from three throughput-
 * sweep probes. Skipped on pre-486 CPUs (loops too slow) and when
 * cache.present=no. Confidence clamps to LOW on emulator captures. */
void bench_cache_char(result_table_t *t);

/* Pure inference helpers exposed for host-testing. */
unsigned int bench_cc_infer_l1_kb(const unsigned long *kbps_by_size,
                                  unsigned int n_sizes);
unsigned int bench_cc_infer_line_bytes(const unsigned long *kbps_by_stride,
                                       unsigned int n_strides);
const char * bench_cc_infer_write_policy(unsigned long read_kbps,
                                         unsigned long write_kbps);
/* v0.8.1 M2.3 DRAM ns derivation: ns = line_bytes * 1e6 / kbps.
 * Returns 0 on degenerate input. Pure, host-testable. */
unsigned long bench_cc_derive_dram_ns(unsigned long kbps,
                                      unsigned int  line_bytes);

/* v0.4 video bandwidth — see docs/plans/v0.4-benchmarks-and-polish.md §2.
 *
 * Quick-mode only for v0.4 (matches bench_cache's posture — see note
 * above). Emits bench.video.text_write_kbps for every adapter and
 * bench.video.mode13h_kbps for VGA-on-real-iron. Skips mode 13h under
 * emulators (mode-switch mangles the host terminal under DOSBox-X and
 * friends). Writes disturb the live text-mode display for ~1 second
 * then restore it from a FAR save buffer. */
void bench_video(result_table_t *t, const opts_t *o);

/* v0.5.0 Mandelbrot FPU visual demo — T4b. Not a benchmark in the
 * measured-value sense; renders a Mandelbrot set to VGA mode 13h as
 * a post-run visual coda when an FPU is present and the adapter is
 * VGA-capable. Skipped under /NOUI and on non-VGA adapters. Called
 * from bench_whetstone() after its numeric emit completes. */
void bench_mandelbrot_demo(const opts_t *o);

/* v0.6.1 T4: Memory Cache Waterfall visual. Measures write bandwidth
 * across 9 block sizes (1B..64KB) and animates progressive-fill bars
 * whose speed is proportional to measured rate. Cache boundary shows
 * as a speed transition between small-block and large-block bars.
 * Called from bench_all after bench_memory. Text mode, all adapters. */
void bench_cache_waterfall_visual(const opts_t *o);

#endif
