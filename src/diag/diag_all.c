#include <stdio.h>
#include "diag.h"

void diag_all(result_table_t *t, const opts_t *o)
{
    (void)o;
    puts("[diagnose] running...");
    diag_cpu(t);
    diag_mem(t);
    diag_fpu(t);
    /* Further subsystems (video RAM, cache coherence, DMA) land as
     * Phase 2 continues; each adds a diag_<subsys>() call here. */
}
