#include <stdio.h>
#include "bench.h"
#include "../core/crumb.h"

#define WRAP_BENCH(name, call) do {                                     \
    if (!crumb_skiplist_has("bench." name)) {                           \
        crumb_enter("bench." name);                                     \
        call;                                                            \
        crumb_exit();                                                   \
    }                                                                    \
} while (0)

void bench_all(result_table_t *t, const opts_t *o)
{
    puts("[benchmark] running...");
    WRAP_BENCH("cpu",    bench_cpu(t, o));
    WRAP_BENCH("memory", bench_memory(t, o));
    WRAP_BENCH("fpu",    bench_fpu(t, o));
    /* bench_cache / bench_video land as Phase 3 continues. */
}
