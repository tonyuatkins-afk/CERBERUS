#ifndef CERBERUS_BENCH_H
#define CERBERUS_BENCH_H

#include "../cerberus.h"

void bench_all(result_table_t *t, const opts_t *o);

/* Per-subsystem benchmark entry points. Each times a fixed workload
 * via the timing module and reports iters/sec + us/iter into the
 * [bench.<subsys>] INI section. */
void bench_cpu(result_table_t *t);
void bench_memory(result_table_t *t);
void bench_fpu(result_table_t *t);

#endif
