/*
 * Whetstone — synthetic floating-point benchmark, Curnow/Wichmann 1976.
 *
 * Ported from the public-domain C rewrite by Harold Curnow (derived from
 * the original Fortran) via the widely-mirrored Painter/Weicker cleanup.
 * Public domain.
 *
 * Standard output is K-Whetstones/second, where a K-Whetstone is 1,000
 * Whetstone operations. The reference defines the operation-weighting
 * such that a 1 MHz PDP-11 equivalent hits ~0.5 KWIPS; a 486 DX-2 at
 * 66 MHz produces ~11,000 KWIPS (matching the CheckIt 3.0 reference on
 * the BEK-V409 bench box: 11,419.9 K-Whetstones).
 *
 * Port adaptations:
 *   - No stdio output. Emit INI rows instead of printing.
 *   - FPU-presence gate: if detect_fpu reported "fpu.detected=none",
 *     skip the run entirely and emit whetstone_status=skipped_no_fpu.
 *     Running the x87 instructions on a system without an FPU (and
 *     without an FPU-emulator TSR like EM87) would trigger INT 7 which
 *     may or may not be handled gracefully.
 *   - Auto-calibration: a short warmup estimates scale; the main run
 *     targets ~5 seconds on the detected CPU. Fallback to a fixed loop
 *     count if warmup returns 0.
 *   - BIOS-tick-timed via timing_start_long / timing_stop_long. Same
 *     reasoning as bench_dhrystone: a 5-second target is ~91 PIT C2
 *     wraps, and the C2-based timing_start/timing_stop helpers can only
 *     resolve one wrap. The BIOS-tick pair (~55 ms resolution) is the
 *     right primitive for multi-wrap intervals; the ~1% discretization
 *     at 5 seconds is well under the ±5% match-target budget.
 *   - Reference constants and module weights preserved. Iteration count
 *     per module from the reference (N1..N11).
 *
 * Rule 10 (whetstone_fpu_consistency) reads fpu.detected and the
 * whetstone_status / k_whetstones rows to flag any disagreement between
 * the FPU-detection head and the Whetstone benchmark's own FPU-presence
 * inference.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"

/* ------------------------------------------------------------------- */
/* FPU-absence check via result-table lookup                            */
/* ------------------------------------------------------------------- */

static const result_t *find_key_local(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Returns 1 if detect_fpu reported an FPU present, 0 otherwise. If the
 * key is missing entirely (detect head skipped), we treat as absent so
 * we don't execute x87 on an unknown-capability machine. */
static int fpu_looks_present(const result_table_t *t)
{
    const result_t *r = find_key_local(t, "fpu.detected");
    if (!r) return 0;
    if (!r->v.s) return 0;
    if (strcmp(r->v.s, "none") == 0) return 0;
    return 1;
}

/* ------------------------------------------------------------------- */
/* Whetstone reference workload                                         */
/* ------------------------------------------------------------------- */

/* `volatile` on the accumulator statics is load-bearing for DCE
 * suppression — Watcom -ox will otherwise eliminate arithmetic whose
 * result is only observed via other volatile-less globals. The
 * whetstones_checksum emit at the end of bench_whetstone() is the
 * belt-and-braces observer; between the two, every loop iteration's
 * work must be preserved. Removing either is a v7-class regression
 * (30× overreport). */
static volatile double T, T1, T2;
static volatile double E1[4];
static volatile int    J, K, L;

/* Reference module iteration counts. Per Curnow 1976, these distribute
 * 1,000,000 Whetstone "operations" across the 11 modules according to
 * the weighting scheme that makes a 1-MIPS PDP-11 class machine produce
 * ~1 KWIPS in a nominal 1-second run. */
#define N1  0
#define N2  12
#define N3  14
#define N4  345
#define N6  210
#define N7  32
#define N8  899
#define N9  616
#define N11 93

/* V_U32 emits below pass NULL display per the b6c179b / 6c3a023 fix
 * pattern — format_result_value in report.c handles "%lu" formatting
 * from r->v.u at INI-write time. Avoids the static-buffer-dangling
 * corruption class (v7 capture showed "fpu.whetstone_elapsed_us=(�"
 * garbage). Do not re-add dedicated display buffers; the NULL path
 * is correct and bug-free. */

/* ------------------------------------------------------------------- */
/* Module helper procedures (Curnow reference)                          */
/* ------------------------------------------------------------------- */

static void PA(double *E)
{
    int J_Loc;
    J_Loc = 0;
    do {
        E[0] = (E[0] + E[1] + E[2] - E[3]) * T;
        E[1] = (E[0] + E[1] - E[2] + E[3]) * T;
        E[2] = (E[0] - E[1] + E[2] + E[3]) * T;
        E[3] = (-E[0] + E[1] + E[2] + E[3]) / T2;
        J_Loc++;
    } while (J_Loc < 6);
}

static void P3(double XX, double YY, double *ZZ)
{
    double X_Loc = XX;
    double Y_Loc = YY;
    X_Loc = T * (X_Loc + Y_Loc);
    Y_Loc = T * (X_Loc + Y_Loc);
    *ZZ = (X_Loc + Y_Loc) / T2;
}

static void P0(void)
{
    E1[J] = E1[K];
    E1[K] = E1[L];
    E1[L] = E1[J];
}

/* ------------------------------------------------------------------- */
/* One Whetstone unit = 1000 iterations of the 11 modules.              */
/* run_whetstone_units(n) runs n such units; K-Whetstones = n / seconds.*/
/* ------------------------------------------------------------------- */

static void run_whetstone_units(unsigned long units)
{
    /* volatile forces every accumulator read/write to memory regardless
     * of register allocation pressure. Without it Watcom -ox hoists X1-X4
     * / X / Y / Z into registers and DCEs intermediate values that aren't
     * observed by the return-to-memory path. */
    volatile double X1, X2, X3, X4, X, Y, Z;
    int    I_mod, N_mod;
    unsigned long unit_idx;

    for (unit_idx = 0; unit_idx < units; unit_idx++) {
        T  = 0.499975;
        T1 = 0.50025;
        T2 = 2.0;

        /* Module 1: simple identifiers */
        X1 =  1.0;
        X2 = -1.0;
        X3 = -1.0;
        X4 = -1.0;
        for (I_mod = 1; I_mod <= N1; I_mod++) {
            X1 = (X1 + X2 + X3 - X4) * T;
            X2 = (X1 + X2 - X3 + X4) * T;
            X3 = (X1 - X2 + X3 + X4) * T;
            X4 = (-X1 + X2 + X3 + X4) * T;
        }

        /* Module 2: array elements */
        E1[0] =  1.0;
        E1[1] = -1.0;
        E1[2] = -1.0;
        E1[3] = -1.0;
        for (I_mod = 1; I_mod <= N2; I_mod++) {
            E1[0] = ( E1[0] + E1[1] + E1[2] - E1[3]) * T;
            E1[1] = ( E1[0] + E1[1] - E1[2] + E1[3]) * T;
            E1[2] = ( E1[0] - E1[1] + E1[2] + E1[3]) * T;
            E1[3] = (-E1[0] + E1[1] + E1[2] + E1[3]) * T;
        }

        /* Module 3: array as parameter */
        for (I_mod = 1; I_mod <= N3; I_mod++) {
            PA(E1);
        }

        /* Module 4: conditional jumps */
        J = 1;
        for (I_mod = 1; I_mod <= N4; I_mod++) {
            if (J == 1) J = 2; else J = 3;
            if (J > 2)  J = 0; else J = 1;
            if (J < 1)  J = 1; else J = 0;
        }

        /* Module 5: omitted — no work per reference */

        /* Module 6: integer arithmetic */
        J = 1;
        K = 2;
        L = 3;
        for (I_mod = 1; I_mod <= N6; I_mod++) {
            J = J * (K - J) * (L - K);
            K = L * K - (L - J) * K;
            L = (L - K) * (K + J);
            E1[L - 2] = (double)(J + K + L);
            E1[K - 2] = (double)(J * K * L);
        }

        /* Module 7: trig functions */
        X = 0.5;
        Y = 0.5;
        for (I_mod = 1; I_mod <= N7; I_mod++) {
            X = T * atan(T2 * sin(X) * cos(X) / (cos(X + Y) + cos(X - Y) - 1.0));
            Y = T * atan(T2 * sin(Y) * cos(Y) / (cos(X + Y) + cos(X - Y) - 1.0));
        }

        /* Module 8: procedure calls */
        X = 1.0;
        Y = 1.0;
        Z = 1.0;
        for (I_mod = 1; I_mod <= N8; I_mod++) {
            P3(X, Y, &Z);
        }

        /* Module 9: array references */
        J = 1;
        K = 2;
        L = 3;
        E1[0] = 1.0;
        E1[1] = 2.0;
        E1[2] = 3.0;
        for (I_mod = 1; I_mod <= N9; I_mod++) {
            P0();
        }

        /* Module 10: integer arithmetic in a loop */
        J = 2;
        K = 3;
        for (I_mod = 1; I_mod <= 7; I_mod++) {
            J = J + K;
            K = J + K;
            J = K - J;
            K = K - J - J;
        }

        /* Module 11: standard functions */
        X = 0.75;
        for (I_mod = 1; I_mod <= N11; I_mod++) {
            X = sqrt(exp(log(X) / T1));
        }

        /* volatile accumulators + the post-loop checksum in
         * bench_whetstone() form the DCE barrier now. These sink-assignments
         * are redundant but kept as in-loop resistance to any future
         * optimizer that gets more aggressive about volatile locals.
         * Cheap; keep. */
        N_mod = J;
        (void)N_mod;
        (void)Z;
        (void)X4;
        (void)X3;
        (void)X2;
    }
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

    /* Warmup to calibrate iteration count. */
    timing_start_long();
    run_whetstone_units(W_WARMUP_UNITS);
    warmup_us = timing_stop_long();

    if (warmup_us == 0) {
        units = 500UL;
    } else if (warmup_us >= W_TARGET_MAIN_US / 2UL) {
        /* Slow-hardware short-circuit (S1 round-2 fix — same pattern as
         * bench_dhrystone). A warmup that already consumed half the
         * target runtime means the main run would blow past target; the
         * cleanest semantics are to treat the warmup AS the main run.
         * Skip the second pass by reusing warmup_us as main_us. */
        main_us        = warmup_us;
        units          = W_WARMUP_UNITS;
        warmup_is_main = 1;
    } else {
        units = (W_WARMUP_UNITS * W_TARGET_MAIN_US) / (unsigned long)warmup_us;
        if (units < W_MIN_UNITS) units = W_MIN_UNITS;
        if (units > W_MAX_UNITS) units = W_MAX_UNITS;
    }

    if (!warmup_is_main) {
        /* Real run */
        timing_start_long();
        run_whetstone_units(units);
        main_us = timing_stop_long();
    }

    /* Anti-DCE observer — consume final FPU accumulator state via
     * report_add_u32 (external linkage via report.c, opaque to Watcom
     * -ox). Paired with volatile on T/T1/T2/E1/J/K/L and the volatile
     * locals inside run_whetstone_units, the checksum read back here
     * forces every loop iteration's work to be preserved. v7 produced
     * k_whetstones=344,256 (30× CheckIt's 11,419.9) without these
     * barriers — fully DCE'd workload. Do not remove; not for
     * consistency-engine use, purely an optimization barrier. */
    {
        unsigned long checksum = 0UL;
        checksum ^= (unsigned long)(long)E1[0];
        checksum ^= (unsigned long)(long)E1[1];
        checksum ^= (unsigned long)(long)E1[2];
        checksum ^= (unsigned long)(long)E1[3];
        checksum ^= (unsigned long)(unsigned int)J;
        checksum ^= (unsigned long)(unsigned int)K;
        checksum ^= (unsigned long)(unsigned int)L;
        report_add_u32(t, "bench.fpu.whetstones_checksum",
                       checksum, (const char *)0,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    report_add_u32(t, "bench.fpu.whetstone_elapsed_us",
                   (unsigned long)main_us, (const char *)0,
                   CONF_HIGH, VERDICT_UNKNOWN);

    if (main_us > 0) {
        /* units * 1,000,000 / main_us — watch for 32-bit overflow.
         * If units * 1e6 might overflow (units > ~4294), use the
         * rearranged form. Typical units on a 486 run ~500-2000. */
        if (units < 4294UL) {
            k_whet_per_sec = (units * 1000000UL) / (unsigned long)main_us;
        } else {
            unsigned long us_per_unit_x1000 =
                ((unsigned long)main_us * 1000UL) / units;
            if (us_per_unit_x1000 > 0) {
                k_whet_per_sec = 1000000000UL / us_per_unit_x1000;
            } else {
                k_whet_per_sec = 0;
            }
        }
        report_add_u32(t, "bench.fpu.k_whetstones",
                       k_whet_per_sec, (const char *)0,
                       CONF_HIGH, VERDICT_UNKNOWN);
        report_add_str(t, "bench.fpu.whetstone_status", "ok",
                       CONF_HIGH, VERDICT_UNKNOWN);
    } else {
        report_add_str(t, "bench.fpu.whetstone_status",
                       "inconclusive_elapsed_zero",
                       CONF_LOW, VERDICT_WARN);
    }
}
