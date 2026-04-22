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
} fp_probe_result_t;

fp_family_t fpu_fp_infer_family(const fp_probe_result_t *r);
const char *fpu_fp_family_name(fp_family_t f);

/* v0.6.0 visual journey hooks. Called after the corresponding
 * diagnostic/benchmark completes and the verdict has been emitted.
 * Each honors journey_should_skip() on entry and no-ops on /NOUI,
 * /QUICK, or after the skip-all latch. */
void diag_bit_parade(const opts_t *o);
void diag_lissajous(const result_table_t *t, const opts_t *o);
void diag_latency_map(const opts_t *o);

#endif
