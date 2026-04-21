/*
 * Whetstone — synthetic floating-point benchmark, Curnow/Wichmann 1976.
 *
 * v0.5.0 — dispatches to x87 assembly kernel (bench_whet_fpu.asm) on
 * FPU-equipped systems. Replaces the prior Watcom-compiled C kernel,
 * which suffered a ~15x slowdown vs the published reference envelope
 * because the `volatile` DCE-suppression pattern forced every inner
 * arithmetic through memory instead of x87 registers.
 *
 * Standard output is K-Whetstones/second, where a K-Whetstone is 1,000
 * Whetstone operations. Published reference envelope for a 486 DX-2 at
 * 66 MHz: 1,500-3,000 K-Whet. Systems without an FPU skip the run
 * entirely and emit whetstone_status=skipped_no_fpu.
 *
 * Rule 10 (whetstone_fpu_consistency) reads fpu.detected and the
 * whetstone_status / k_whetstones rows to flag any disagreement between
 * the FPU-detection head and the Whetstone benchmark's own FPU-presence
 * inference.
 *
 * Issue #4 note: the asm kernel's numeric output still needs real-
 * hardware validation on BEK-V409. Until that lands, treat reported
 * K-Whet as same-machine regression signal, not as a cross-tool
 * comparison anchor. See docs/plans/checkit-comparison.md for the
 * calibration plan.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"
#include "../core/journey.h"

/* ------------------------------------------------------------------- */
/* Asm-kernel interface                                                 */
/* ------------------------------------------------------------------- */

/* Data globals defined in bench_whet_fpu.asm. We read them after the
 * kernel returns to build the DCE-barrier checksum — Watcom can't elide
 * the asm kernel's memory writes because these symbols have external
 * linkage and the asm routine is opaque to the optimizer. */
extern unsigned long whet_fpu_units;
extern double        whet_fpu_E1[4];
extern int           whet_fpu_J, whet_fpu_K, whet_fpu_L;
extern double        whet_fpu_X1, whet_fpu_X2, whet_fpu_X3, whet_fpu_X4;
extern double        whet_fpu_X,  whet_fpu_Y,  whet_fpu_Z;

/* Asm kernel entry. Reads whet_fpu_units, runs that many Whetstone
 * units, leaves accumulator state in the globals above. */
extern void whet_fpu_run_units(void);

#pragma aux whet_fpu_run_units "whet_fpu_run_units_" \
    modify exact [ax bx cx dx si di];

/* ------------------------------------------------------------------- */
/* FPU-absence check                                                    */
/* ------------------------------------------------------------------- */

static const result_t *find_key_local(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Returns 1 if detect_fpu reported an FPU present, 0 otherwise. Missing
 * key treated as absent so we don't execute x87 on unknown-capability
 * hardware. Guards against value-type reinterpretation per the S5
 * round-2 pattern. */
static int fpu_looks_present(const result_table_t *t)
{
    const result_t *r = find_key_local(t, "fpu.detected");
    if (!r) return 0;
    if (r->type != V_STR || !r->v.s) return 0;
    if (strcmp(r->v.s, "none") == 0) return 0;
    return 1;
}

/* ------------------------------------------------------------------- */
/* CERBERUS entry point                                                 */
/* ------------------------------------------------------------------- */

#define W_WARMUP_UNITS   10UL
#define W_MIN_UNITS      10UL
#define W_MAX_UNITS      200000UL
#define W_TARGET_MAIN_US 5000000UL    /* 5 seconds */

void bench_whetstone(result_table_t *t, const opts_t *o)
{
    us_t warmup_us, main_us;
    unsigned long units;
    unsigned long k_whet_per_sec;
    int warmup_is_main;
    (void)o;

    warmup_is_main = 0;

    if (!fpu_looks_present(t)) {
        report_add_str(t, "bench.fpu.whetstone_status", "skipped_no_fpu",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }

    /* v0.6.0 T8: title card for the FPU Benchmark section. Covers both
     * the timed Whetstone measurement (below) and the Mandelbrot visual
     * that fires at the end. */
    if (journey_title_card(o, HEAD_CENTER,
                           "FPU BENCHMARK",
                           "Measuring floating-point throughput with "
                           "Whetstone. Then your FPU renders a "
                           "Mandelbrot fractal in real time.") == 1) {
        /* skip-all latched; still emit the measurement + row, skip
         * visuals only. Mandelbrot will no-op. */
    }

    /* Warmup */
    whet_fpu_units = W_WARMUP_UNITS;
    timing_start_long();
    whet_fpu_run_units();
    warmup_us = timing_stop_long();

    if (warmup_us == 0) {
        units = 500UL;
    } else if (warmup_us >= W_TARGET_MAIN_US / 2UL) {
        /* Slow-hardware short-circuit: treat the warmup as the main run. */
        main_us        = warmup_us;
        units          = W_WARMUP_UNITS;
        warmup_is_main = 1;
    } else {
        units = (W_WARMUP_UNITS * W_TARGET_MAIN_US) / (unsigned long)warmup_us;
        if (units < W_MIN_UNITS) units = W_MIN_UNITS;
        if (units > W_MAX_UNITS) units = W_MAX_UNITS;
    }

    if (!warmup_is_main) {
        whet_fpu_units = units;
        timing_start_long();
        whet_fpu_run_units();
        main_us = timing_stop_long();
    }

    /* DCE-barrier checksum over the asm kernel's final state. */
    {
        unsigned long checksum = 0UL;
        checksum ^= (unsigned long)(long)whet_fpu_E1[0];
        checksum ^= (unsigned long)(long)whet_fpu_E1[1];
        checksum ^= (unsigned long)(long)whet_fpu_E1[2];
        checksum ^= (unsigned long)(long)whet_fpu_E1[3];
        checksum ^= (unsigned long)(unsigned int)whet_fpu_J;
        checksum ^= (unsigned long)(unsigned int)whet_fpu_K;
        checksum ^= (unsigned long)(unsigned int)whet_fpu_L;
        checksum ^= (unsigned long)(long)whet_fpu_X1;
        checksum ^= (unsigned long)(long)whet_fpu_X2;
        checksum ^= (unsigned long)(long)whet_fpu_X3;
        checksum ^= (unsigned long)(long)whet_fpu_X4;
        checksum ^= (unsigned long)(long)whet_fpu_X;
        checksum ^= (unsigned long)(long)whet_fpu_Y;
        checksum ^= (unsigned long)(long)whet_fpu_Z;
        report_add_u32(t, "bench.fpu.whetstones_checksum",
                       checksum, (const char *)0,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    if (main_us > 0) {
        report_add_u32(t, "bench.fpu.whetstone_elapsed_us",
                       (unsigned long)main_us, (const char *)0,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    if (main_us > 0) {
        /* k_whet_per_sec = units * 1,000,000 / main_us. Rescale to ms
         * so numerator stays in 32 bits. See v0.4 bench_dhrystone S1
         * fix for the overflow analysis. */
        if (main_us >= 1000UL) {
            unsigned long ms = (unsigned long)main_us / 1000UL;
            k_whet_per_sec = (units * 1000UL) / ms;
            if (k_whet_per_sec == 0UL) {
                report_add_str(t, "bench.fpu.whetstone_status",
                               "inconclusive_sub_kwhet",
                               CONF_LOW, VERDICT_WARN);
            } else {
                /* v0.5.0 asm kernel emits at CONF_HIGH. Per-tool
                 * comparability and absolute accuracy still need
                 * real-hardware validation on BEK-V409 (issue #4);
                 * meanwhile the number is reproducible across cold
                 * boots and usable as same-machine regression signal. */
                report_add_u32(t, "bench.fpu.k_whetstones",
                               k_whet_per_sec, (const char *)0,
                               CONF_HIGH, VERDICT_UNKNOWN);
                report_add_str(t, "bench.fpu.whetstone_status", "ok",
                               CONF_HIGH, VERDICT_UNKNOWN);
            }
        } else {
            report_add_str(t, "bench.fpu.whetstone_status",
                           "inconclusive_sub_ms",
                           CONF_LOW, VERDICT_WARN);
        }
    } else {
        report_add_str(t, "bench.fpu.whetstone_status",
                       "inconclusive_elapsed_zero",
                       CONF_LOW, VERDICT_WARN);
    }

    /* Visual coda: Mandelbrot in VGA mode 13h. Not timed, not
     * measured — a post-run proof-of-life for the FPU. Gates on
     * VGA-capable adapter and /NOUI inside the demo; FPU presence
     * is implied by reaching this point (the early-return above). */
    bench_mandelbrot_demo(o);
}
