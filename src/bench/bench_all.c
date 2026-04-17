#include <stdio.h>
#include "bench.h"

void bench_all(result_table_t *t, const opts_t *o)
{
    puts("[benchmark] running...");
    bench_cpu(t, o);
    bench_memory(t, o);
    bench_fpu(t, o);
    /* bench_cache / bench_video land as Phase 3 continues. */
}
