#include <stdio.h>
#include "diag.h"
#include "../core/crumb.h"

/* NOTE: `name` MUST be a string literal — the "diag." name concatenation
 * relies on C89 adjacent-literal fusion. Passing a `const char *` here
 * would compile to pointer-tack-on and silently produce garbage. */
#define WRAP_DIAG(name, call) do {                                      \
    if (!crumb_skiplist_has("diag." name)) {                            \
        crumb_enter("diag." name);                                      \
        call;                                                            \
        crumb_exit();                                                   \
    }                                                                    \
} while (0)

void diag_all(result_table_t *t, const opts_t *o)
{
    puts("[diagnose] running...");
    WRAP_DIAG("cpu",   diag_cpu(t));
    /* v0.6.0 T2: visual journey hook — bit parade after CPU ALU diag. */
    diag_bit_parade(o);
    WRAP_DIAG("mem",   diag_mem(t));
    WRAP_DIAG("fpu",   diag_fpu(t));
    WRAP_DIAG("video", diag_video(t));
    WRAP_DIAG("cache", diag_cache(t));
    WRAP_DIAG("dma",   diag_dma(t));
    /*
     * v0.3 completion landed diag_cache (stride-ratio timing probe) and
     * diag_dma (8237 count-register readback on channels 1/2/3/5/6/7 —
     * channels 0 + 4 safety-skipped as refresh/cascade).
     *
     * See docs/plans/v0.3-diagnose-completion.md for the verdict tables,
     * safety rationale, and real-hardware verification gate. Real-iron
     * acceptance is user-run on BEK-V409 after host-side green.
     */
}
