#include <stdio.h>
#include "bench.h"

void bench_all(result_table_t *t, const opts_t *o)
{
    (void)o;
    puts("[benchmark] running...");
    bench_cpu(t);
    /* bench_fpu / bench_memory / bench_cache / bench_video land as
     * Phase 3 continues. */
}
