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
 * tests so the rate arithmetic can be exercised without PIT hardware. */
unsigned long bench_cache_kb_per_sec(unsigned long bytes,
                                     unsigned long elapsed_us);

#endif
