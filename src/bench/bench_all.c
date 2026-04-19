#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/crumb.h"
#include "../core/report.h"

/*
 * PC-XT relative-rating reference constants.
 *
 * Sources (derived to match CheckIt 3.0's ratios against published PC-XT
 * measurements; see docs/plans/v0.4-historical-benchmarks.md §2.3):
 *
 *   PC_XT_DHRYSTONES         = 344 — CheckIt 3.0 computes 33,609 / 97.70
 *                                    ≈ 344 Dhrystones/sec as its PC-XT
 *                                    integer reference. Aligns with BYTE
 *                                    1988 round-up (~346) within rounding.
 *
 *   PC_XT_K_WHETSTONES_X10   = 66  — CheckIt computes 11,419.9 / 1730.29
 *                                    ≈ 6.6 K-Whetstones/sec for a PC-XT
 *                                    with 8087. Stored × 10 so integer
 *                                    math avoids the 6.6 fraction.
 *
 *   PC_XT_MEM_KBPS           = 140 — IBM PC Technical Reference Manual
 *                                    timing at 1T wait state, 4.77 MHz.
 *
 * Changing these constants changes the scale of every _xt_factor emitted.
 * Do not update without also updating docs/plans/v0.4-historical-benchmarks.md
 * and, if the change is material, docs/consistency-rules.md.
 */
#define PC_XT_DHRYSTONES         344UL
#define PC_XT_K_WHETSTONES_X10   66UL
#define PC_XT_MEM_KBPS           140UL

/* NOTE: `name` MUST be a string literal — the "bench." name concatenation
 * relies on C89 adjacent-literal fusion. Passing a `const char *` here
 * would compile to pointer-tack-on and silently produce garbage. */
#define WRAP_BENCH(name, call) do {                                     \
    if (!crumb_skiplist_has("bench." name)) {                           \
        crumb_enter("bench." name);                                     \
        call;                                                            \
        crumb_exit();                                                   \
    }                                                                    \
} while (0)

/* Static-lifetime buffers for the formatted ratio strings. report_add_str
 * stores the pointer verbatim, so these must outlive the call. */
static char xt_cpu_val[16];
static char xt_fpu_val[16];
static char xt_mem_val[16];

static const result_t *find_bench_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Format an x100-scaled ratio (e.g. 9770) as a decimal with 2 places
 * (e.g. "97.70") into `out`. `out` must have room for ~10 chars. */
static void format_x100(char *out, unsigned long value_x100)
{
    sprintf(out, "%lu.%02lu", value_x100 / 100UL, value_x100 % 100UL);
}

static void emit_xt_ratios(result_table_t *t)
{
    const result_t *r_dhry  = find_bench_key(t, "bench.cpu.dhrystones");
    const result_t *r_kwhet = find_bench_key(t, "bench.fpu.k_whetstones");
    const result_t *r_mem   = find_bench_key(t, "bench.memory.copy_kbps");

    /* CPU ratio — emit only when Dhrystone produced a real number. */
    if (r_dhry && r_dhry->type == V_U32 && r_dhry->v.u > 0UL) {
        unsigned long factor_x100 = (r_dhry->v.u * 100UL) / PC_XT_DHRYSTONES;
        format_x100(xt_cpu_val, factor_x100);
        report_add_str(t, "bench.cpu_xt_factor", xt_cpu_val,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    /* FPU ratio — only when Whetstone produced a real number. A run on a
     * no-FPU machine will have whetstone_status=skipped_no_fpu with no
     * k_whetstones row, so find_bench_key returns NULL and we emit
     * nothing (rather than a misleading 0.00× PC-XT). */
    if (r_kwhet && r_kwhet->type == V_U32 && r_kwhet->v.u > 0UL) {
        /* factor = k_whet / (PC_XT_K_WHETSTONES_X10 / 10)
         *        = k_whet * 10 / PC_XT_K_WHETSTONES_X10
         * factor_x100 = k_whet * 1000 / PC_XT_K_WHETSTONES_X10 */
        unsigned long factor_x100;
        if (r_kwhet->v.u > 4294967UL) {
            /* 32-bit overflow guard — unlikely in practice. */
            factor_x100 = (r_kwhet->v.u / PC_XT_K_WHETSTONES_X10) * 1000UL;
        } else {
            factor_x100 = (r_kwhet->v.u * 1000UL) / PC_XT_K_WHETSTONES_X10;
        }
        format_x100(xt_fpu_val, factor_x100);
        /* CONF_LOW mirrors bench.fpu.k_whetstones — the fpu_xt_factor is
         * directly derived from it, so it inherits the same "not CheckIt-
         * comparable" caveat. See docs/plans/checkit-comparison.md. */
        report_add_str(t, "bench.fpu_xt_factor", xt_fpu_val,
                       CONF_LOW, VERDICT_UNKNOWN);
    }

    /* Memory ratio */
    if (r_mem && r_mem->type == V_U32 && r_mem->v.u > 0UL) {
        unsigned long factor_x100 = (r_mem->v.u * 100UL) / PC_XT_MEM_KBPS;
        format_x100(xt_mem_val, factor_x100);
        report_add_str(t, "bench.mem_xt_factor", xt_mem_val,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }
}

void bench_all(result_table_t *t, const opts_t *o)
{
    puts("[benchmark] running...");
    WRAP_BENCH("cpu",        bench_cpu(t, o));
    WRAP_BENCH("memory",     bench_memory(t, o));
    WRAP_BENCH("fpu",        bench_fpu(t, o));
    WRAP_BENCH("cache",      bench_cache(t, o));
    WRAP_BENCH("video",      bench_video(t, o));
    WRAP_BENCH("dhrystone",  bench_dhrystone(t, o));
    WRAP_BENCH("whetstone",  bench_whetstone(t, o));

    /* PC-XT relative-rating math runs after all individual benches so it
     * sees populated rows. Each _xt_factor emit is gated on its input
     * being present — a run with /ONLY:DET (no bench) produces no ratios
     * and is silent rather than incorrect. Consumed by the Whetstone-
     * implied consistency rule via detect_fpu cross-check (Rule 10). */
    emit_xt_ratios(t);
}
