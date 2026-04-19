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

#endif
