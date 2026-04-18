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
    (void)o;
    puts("[diagnose] running...");
    WRAP_DIAG("cpu",   diag_cpu(t));
    WRAP_DIAG("mem",   diag_mem(t));
    WRAP_DIAG("fpu",   diag_fpu(t));
    WRAP_DIAG("video", diag_video(t));
    /*
     * Deferred from Phase 2 — needs real-hardware iteration:
     *
     *   Cache coherence  Requires 486+ INVD/WBINVD or explicit cache
     *                    line flush to get meaningful results. Blind
     *                    implementation risks false positives from
     *                    emulator cache synthesis. Re-evaluate once
     *                    Task 1.10 real-hardware gate surfaces actual
     *                    failure modes.
     *
     *   DMA liveness     The plan is explicit: "diagnostics MUST NOT
     *                    damage hardware. No DMA that overwrites DOS
     *                    kernel memory." A safe harness needs careful
     *                    address-space negotiation via INT 21h — scope
     *                    grows fast. Deferred until benchmark module
     *                    establishes DMA handling in Phase 3.
     */
}
