#ifndef CERBERUS_DIAG_H
#define CERBERUS_DIAG_H

#include "../cerberus.h"

void diag_all(result_table_t *t, const opts_t *o);

/* Per-subsystem diagnostic entry points. Each reads existing detect
 * results from the table and attaches verdicts to those entries (via
 * report_set_verdict) while optionally adding diagnose.<subsys>.<test>
 * rows for fine-grained pass/fail reporting. */
void diag_cpu(result_table_t *t);
void diag_mem(result_table_t *t);
void diag_fpu(result_table_t *t);
void diag_video(result_table_t *t);

/* v0.3 completion — see docs/plans/v0.3-diagnose-completion.md */
void diag_cache(result_table_t *t);
void diag_dma(result_table_t *t);

/* v0.7.1 — FPU behavioral fingerprinting. Runs four x87 probes to
 * distinguish 8087/80287 from 80387+ via observable behavior (infinity
 * mode, pseudo-NaN handling, FPREM1 / FSIN presence). Skipped when
 * fpu.detected=none. */
void diag_fpu_fingerprint(result_table_t *t);

/* Pure-math kernels exposed for host-testing. */
verdict_t diag_cache_classify_ratio_x100(unsigned long ratio_x100,
                                         const char **out_msg_prefix);
verdict_t diag_dma_summary_verdict(int ch_pass, int ch_fail, int ch_skip);

/* v0.7.1: FPU fingerprint inference (pure, host-testable). */
typedef enum {
    FP_FAMILY_UNKNOWN = 0,
    FP_FAMILY_LEGACY  = 1,    /* 8087/80287 behavioral signature */
    FP_FAMILY_MODERN  = 2,    /* 80387+ behavioral signature */
    FP_FAMILY_MIXED   = 3     /* inconsistent probes — anomaly */
} fp_family_t;

typedef struct {
    int infinity_affine;      /* 1 = affine (modern), 0 = projective */
    int pseudo_nan_traps;     /* 1 = IE raised on FLD (modern), 0 = accepted */
    int has_fprem1;           /* 1 = D9 F5 executed (modern), 0 = #UD */
    int has_fsin;             /* 1 = D9 FE executed (modern), 0 = #UD */
    int fptan_pushes_one;     /* M2.2 axis (research gap I):
                               * 1 = FPTAN leaves ST(0)=1.0 (modern 387+),
                               * 0 = FPTAN leaves a cos-ish denom (8087/287). */
} fp_probe_result_t;

fp_family_t fpu_fp_infer_family(const fp_probe_result_t *r);
const char *fpu_fp_family_name(fp_family_t f);

/* M2.3 FPU rounding-control cross-check (research gap J).
 * Results of FISTP(1.5) and FISTP(-1.5) under each of the 4 RC modes.
 * Expected IEEE-754-conformant values per RC mode (see comments in
 * diag_fpu_fingerprint_a.asm): the 4 (pos, neg) pairs are distinct and
 * fully characterize correct RC behavior on the tested FPU. */
typedef struct {
    int nearest_pos, nearest_neg;    /* RC=00: expect ( 2, -2) */
    int down_pos,    down_neg;       /* RC=01: expect ( 1, -2) */
    int up_pos,      up_neg;         /* RC=10: expect ( 2, -1) */
    int trunc_pos,   trunc_neg;      /* RC=11: expect ( 1, -1) */
} fp_rounding_result_t;

/* Returns 1 if all 4 modes produced their expected (pos, neg) pair,
 * 0 if any mode misbehaved. Pure, host-testable. */
int fpu_fp_rounding_is_conformant(const fp_rounding_result_t *r);

/* M2.4 FPU precision-control cross-check (research gap K).
 * Given three 10-byte extended results of 1.0/3.0 under PC=single (00),
 * PC=double (10), PC=extended (11), returns 1 if all three results are
 * distinct (which proves the PC field actually changed precision), or
 * 0 if any two are identical (PC not honored, or probe failed). */
int fpu_fp_precision_modes_distinct(const unsigned char *single_10,
                                     const unsigned char *double_10,
                                     const unsigned char *extended_10);

/* M2.6 FPU exception-flag roundtrip (research gap M).
 * Given six FSTSW values captured after triggering each of the six
 * x87 exceptions in order (IE, DE, ZE, OE, UE, PE), returns the count
 * of exceptions that actually raised their expected bit. A healthy
 * FPU should score 6/6. Returns -1 on NULL input. Pure, host-testable. */
int fpu_fp_exceptions_count_raised(const unsigned int *sw_6);

/* Returns the exception bit mask the FPU actually raised across the
 * six probes (OR of the relevant bit from each SW). Useful for
 * emitting a compact "which 6 bits fired" token. Returns 0 on NULL. */
unsigned int fpu_fp_exceptions_bitmap(const unsigned int *sw_6);

/* v0.6.0 visual journey hooks. Called after the corresponding
 * diagnostic/benchmark completes and the verdict has been emitted.
 * Each honors journey_should_skip() on entry and no-ops on /NOUI,
 * /QUICK, or after the skip-all latch. */
void diag_bit_parade(const opts_t *o);
void diag_lissajous(const result_table_t *t, const opts_t *o);
void diag_latency_map(const opts_t *o);

#endif
