/*
 * FPU behavioral fingerprinting — Phase 2 v0.7.1 addition.
 *
 * Four probes distinguish 8087/80287 from 80387+ by observable behavior,
 * orthogonal to the presence-detection in detect/fpu.c:
 *
 *   1. Infinity comparison mode (projective vs affine)
 *   2. Pseudo-NaN handling (silently accepted vs #IE raised)
 *   3. FPREM1 opcode presence
 *   4. FSIN opcode presence
 *
 * The probes that could fault (3 and 4) are bracketed by an INT 6
 * handler install/restore — same pattern cpu.c uses for its pushfd
 * test, but with its own CS-local fault flag to keep phase ownership
 * clean.
 *
 * Skipped entirely when detect_fpu already reported fpu.detected=none.
 * The C inference helpers (fpu_fp_infer_family, fpu_fp_family_name)
 * are pure and exposed in diag.h for host-side testing.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include "diag.h"
#include "../core/report.h"

/* --- NASM probe externs ------------------------------------------- */

extern int  fpu_fp_int6_fired(void);
extern void fpu_fp_int6_clear(void);
extern void __far fpu_fp_int6_handler(void);
extern unsigned int fpu_fp_compare_inf(const unsigned char __far *plus_inf,
                                       const unsigned char __far *minus_inf);
extern unsigned int fpu_fp_load_pseudo_nan(const unsigned char __far *ptr);
extern void fpu_fp_try_fprem1(void);
extern void fpu_fp_try_fsin(void);
extern unsigned int fpu_fp_try_fptan(void);   /* M2.2: research gap I */
extern void fpu_fp_probe_rounding(unsigned int mode,
                                   int __far *out_pair);   /* M2.3: gap J */
extern void fpu_fp_probe_precision(unsigned int pc_mode,
                                    unsigned char __far *out_10);  /* M2.4: gap K */
extern void fpu_fp_probe_exceptions(unsigned int __far *out_6);    /* M2.6: gap M */

#pragma aux fpu_fp_int6_fired    "fpu_fp_int6_fired_"    modify exact [ax];
#pragma aux fpu_fp_int6_clear    "fpu_fp_int6_clear_"    modify exact [ax];
#pragma aux fpu_fp_int6_handler  "fpu_fp_int6_handler_";
#pragma aux fpu_fp_compare_inf   "fpu_fp_compare_inf_" \
    parm caller [] \
    modify exact [ax es di];
#pragma aux fpu_fp_load_pseudo_nan "fpu_fp_load_pseudo_nan_" \
    parm caller [] \
    modify exact [ax es di];
#pragma aux fpu_fp_try_fprem1    "fpu_fp_try_fprem1_";
#pragma aux fpu_fp_try_fsin      "fpu_fp_try_fsin_";
#pragma aux fpu_fp_try_fptan     "fpu_fp_try_fptan_"     modify exact [ax];
#pragma aux fpu_fp_probe_rounding "fpu_fp_probe_rounding_" \
    parm caller [] \
    modify exact [ax bx cx es di];
#pragma aux fpu_fp_probe_precision "fpu_fp_probe_precision_" \
    parm caller [] \
    modify exact [ax bx cx es di];
#pragma aux fpu_fp_probe_exceptions "fpu_fp_probe_exceptions_" \
    parm caller [] \
    modify exact [ax es di];

/* --- Pure inference helpers (host-testable) ----------------------- */

fp_family_t fpu_fp_infer_family(const fp_probe_result_t *r)
{
    /*
     * Inference logic (M2.2: 5 axes, was 4 pre-M2):
     *   All 5 probes "modern"                        -> MODERN (80387+)
     *     (affine / traps / executes / executes / pushes-1.0)
     *   All 5 probes "legacy"                        -> LEGACY (8087 or 80287)
     *     (projective / accepts / traps / traps / doesn't-push-1.0)
     *   Anything else                                -> MIXED (anomaly)
     *
     * The probe signals are independent, so a mixed result signals a
     * bug in our probe asm, an exotic emulator that only half-implements
     * x87, or a hardware anomaly worth flagging to the user.
     *
     * FPTAN pushes-1.0 is the fifth axis (research gap I). On
     * BEK-V409 (486 integrated) and 386+387 captures the other four
     * axes all landed MIXED because the 486 integrated FPU and 387
     * share most modern behaviors but one axis diverged; adding the
     * FPTAN axis should push all five toward MODERN on clean 387+.
     */
    int modern_score = 0;
    int legacy_score = 0;

    if (r->infinity_affine)    modern_score++; else legacy_score++;
    if (r->pseudo_nan_traps)   modern_score++; else legacy_score++;
    if (r->has_fprem1)         modern_score++; else legacy_score++;
    if (r->has_fsin)           modern_score++; else legacy_score++;
    if (r->fptan_pushes_one)   modern_score++; else legacy_score++;

    if (modern_score == 5) return FP_FAMILY_MODERN;
    if (legacy_score == 5) return FP_FAMILY_LEGACY;
    return FP_FAMILY_MIXED;
}

const char *fpu_fp_family_name(fp_family_t f)
{
    switch (f) {
        case FP_FAMILY_LEGACY: return "8087_or_80287";
        case FP_FAMILY_MODERN: return "80387_or_newer";
        case FP_FAMILY_MIXED:  return "mixed";
        default:               return "unknown";
    }
}

/* M2.3 rounding-control cross-check inference. Returns 1 if all four
 * RC modes produced the expected (pos, neg) pair from rounding 1.5 and
 * -1.5. See diag.h fp_rounding_result_t for the expected table. */
int fpu_fp_rounding_is_conformant(const fp_rounding_result_t *r)
{
    if (!r) return 0;
    if (r->nearest_pos != 2  || r->nearest_neg != -2) return 0;
    if (r->down_pos    != 1  || r->down_neg    != -2) return 0;
    if (r->up_pos      != 2  || r->up_neg      != -1) return 0;
    if (r->trunc_pos   != 1  || r->trunc_neg   != -1) return 0;
    return 1;
}

/* M2.4 precision-control cross-check inference. Given three 10-byte
 * tword results of 1.0/3.0 computed under PC=single, =double, =extended,
 * returns 1 if all three buffers differ from each other (proving PC
 * actually changed precision), 0 if any pair is bytewise identical. */
int fpu_fp_precision_modes_distinct(const unsigned char *single_10,
                                     const unsigned char *double_10,
                                     const unsigned char *extended_10)
{
    if (!single_10 || !double_10 || !extended_10) return 0;
    if (memcmp(single_10, double_10, 10) == 0) return 0;
    if (memcmp(single_10, extended_10, 10) == 0) return 0;
    if (memcmp(double_10, extended_10, 10) == 0) return 0;
    return 1;
}

/* M2.6 exception-flag roundtrip inference. Each sw_6[i] is FSTSW
 * captured after triggering exception i. Expected bits, indexed by
 * position: IE=0x01, DE=0x02, ZE=0x04, OE=0x08, UE=0x10, PE=0x20.
 * Counts how many of the six fired their expected bit. */
int fpu_fp_exceptions_count_raised(const unsigned int *sw_6)
{
    static const unsigned int expected_bits[6] = {
        0x0001U, 0x0002U, 0x0004U, 0x0008U, 0x0010U, 0x0020U
    };
    unsigned int i;
    int raised = 0;
    if (!sw_6) return -1;
    for (i = 0U; i < 6U; i++) {
        if (sw_6[i] & expected_bits[i]) raised++;
    }
    return raised;
}

unsigned int fpu_fp_exceptions_bitmap(const unsigned int *sw_6)
{
    static const unsigned int expected_bits[6] = {
        0x0001U, 0x0002U, 0x0004U, 0x0008U, 0x0010U, 0x0020U
    };
    unsigned int i;
    unsigned int out = 0U;
    if (!sw_6) return 0U;
    for (i = 0U; i < 6U; i++) {
        if (sw_6[i] & expected_bits[i]) out |= expected_bits[i];
    }
    return out;
}

/* --- Host-test stop point: runtime probes excluded under CERBERUS_HOST_TEST */
#ifndef CERBERUS_HOST_TEST

/* --- 80-bit extended-precision encodings (DGROUP) ----------------- */

/* IEEE-754 80-bit extended format (Intel x87 native):
 *   byte 0..7  : 64-bit significand (includes explicit integer bit at bit 63)
 *   byte 8..9  : 16-bit exponent + sign (sign in bit 15, exp in bits 14..0)
 *
 * +infinity:  sign=0 exp=0x7FFF int_bit=1 frac=0
 * -infinity:  sign=1 exp=0x7FFF int_bit=1 frac=0
 * pseudo-NaN: sign=0 exp=0x7FFF int_bit=0 frac=0  (invalid on 80387+, accepted pre-387)
 */
static const unsigned char plus_inf_encoding[10] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xFF, 0x7F
};
static const unsigned char minus_inf_encoding[10] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0xFF, 0xFF
};
static const unsigned char pseudo_nan_encoding[10] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0x7F
};

/* --- INT 6 vector install / restore ------------------------------- */

static void (__interrupt __far *saved_int6)(void);

static void install_int6(void)
{
    saved_int6 = _dos_getvect(6);
    _dos_setvect(6, (void (__interrupt __far *)(void))fpu_fp_int6_handler);
}

static void restore_int6(void)
{
    _dos_setvect(6, saved_int6);
}

/* --- Result-table lookup ------------------------------------------ */

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* --- Probe orchestration ------------------------------------------ */

/* Per-mode compact display buffers for the rounding cross-check row.
 * Lifetime: file-scope static so report_add_str's stored pointer
 * survives the report_write_ini pass. Format "<pos>,<neg>" e.g. "2,-2". */
static char rnd_display_nearest[12];
static char rnd_display_down[12];
static char rnd_display_up[12];
static char rnd_display_trunc[12];

void diag_fpu_fingerprint(result_table_t *t)
{
    const result_t *fpu_entry;
    const char *fpu_val;
    fp_probe_result_t probes;
    fp_family_t family;
    unsigned int sw;
    fp_rounding_result_t rnd_results;   /* M2.3 */
    int rounding_ok;
    /* M2.4 precision-control probe outputs: 10-byte tword per mode */
    unsigned char prc_single[10];
    unsigned char prc_double[10];
    unsigned char prc_extended[10];
    int precision_ok;
    /* M2.6 exception-flag roundtrip: 6 status-word captures */
    unsigned int exc_sw[6];
    int exc_raised;

    /* Skip if no FPU — nothing to fingerprint. detect_fpu owns the
     * fpu.detected key; we only run when it says something is there. */
    fpu_entry = find_key(t, "fpu.detected");
    if (!fpu_entry || fpu_entry->type != V_STR) return;
    fpu_val = fpu_entry->v.s;
    if (strcmp(fpu_val, "none") == 0) {
        report_add_str(t, "diagnose.fpu.fingerprint_status",
                       "skipped (no FPU detected)",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return;
    }

    /* Probe 1: Infinity mode — safe on all x87s, no trap risk. */
    sw = fpu_fp_compare_inf((const unsigned char __far *)plus_inf_encoding,
                            (const unsigned char __far *)minus_inf_encoding);
    /* C3 is bit 14 of the status word. Set when operands compared equal
     * (projective) or unordered. We expect affine (-inf < +inf) to give
     * C3=0 C0=1; projective (+inf == -inf) to give C3=1 C0=0. */
    probes.infinity_affine = ((sw & 0x4000U) == 0U) ? 1 : 0;

    /* Probe 2: Pseudo-NaN. 80387+ raise IE (bit 0 of SW). */
    sw = fpu_fp_load_pseudo_nan((const unsigned char __far *)pseudo_nan_encoding);
    probes.pseudo_nan_traps = ((sw & 0x0001U) != 0U) ? 1 : 0;

    /* Probes 3 and 4: opcode fault-catch. Install the fingerprinting
     * module's INT 6 handler, attempt each opcode, observe fault flag. */
    install_int6();

    fpu_fp_int6_clear();
    fpu_fp_try_fprem1();
    probes.has_fprem1 = fpu_fp_int6_fired() ? 0 : 1;

    fpu_fp_int6_clear();
    fpu_fp_try_fsin();
    probes.has_fsin = fpu_fp_int6_fired() ? 0 : 1;

    restore_int6();

    /* Probe 5 (M2.2, research gap I): FPTAN pushes 1.0 on 387+.
     * No fault expected on either generation; FPTAN is valid back to
     * the 8087. No INT 6 handler needed. We decode the status word's
     * C3 bit (0x4000): set when ST(0) compared equal to 1.0 after
     * FPTAN. */
    sw = fpu_fp_try_fptan();
    probes.fptan_pushes_one = ((sw & 0x4000U) != 0U) ? 1 : 0;

    /* Infer family from the five signals. */
    family = fpu_fp_infer_family(&probes);

    /* Probe 6 (M2.3, research gap J): rounding-control cross-check.
     * Run FISTP(1.5) and FISTP(-1.5) under each of RC=00/01/10/11;
     * the 4 (pos, neg) pairs are distinct and fully characterize
     * correct RC behavior. Results are INI-emitted below regardless
     * of pass/warn outcome so post-run readers have the forensic
     * trail (CACHECHK Phase 3 pattern). */
    {
        int pair[2];
        fpu_fp_probe_rounding(0U, (int __far *)pair);
        rnd_results.nearest_pos = pair[0];
        rnd_results.nearest_neg = pair[1];
        fpu_fp_probe_rounding(1U, (int __far *)pair);
        rnd_results.down_pos    = pair[0];
        rnd_results.down_neg    = pair[1];
        fpu_fp_probe_rounding(2U, (int __far *)pair);
        rnd_results.up_pos      = pair[0];
        rnd_results.up_neg      = pair[1];
        fpu_fp_probe_rounding(3U, (int __far *)pair);
        rnd_results.trunc_pos   = pair[0];
        rnd_results.trunc_neg   = pair[1];
    }
    rounding_ok = fpu_fp_rounding_is_conformant(&rnd_results);

    /* Probe 7 (M2.4, research gap K): precision-control cross-check.
     * Run 1.0/3.0 under PC=single/double/extended; each should produce
     * a bytewise-distinct 10-byte result. If any two modes produce
     * identical results, PC isn't being honored or the FPU doesn't
     * implement distinct precision rounding. */
    fpu_fp_probe_precision(0U, (unsigned char __far *)prc_single);
    fpu_fp_probe_precision(2U, (unsigned char __far *)prc_double);
    fpu_fp_probe_precision(3U, (unsigned char __far *)prc_extended);
    precision_ok = fpu_fp_precision_modes_distinct(prc_single,
                                                    prc_double,
                                                    prc_extended);

    /* Probe 8 (M2.6, research gap M): exception-flag roundtrip.
     * Triggers each of the 6 x87 exceptions in turn and captures
     * FSTSW. A healthy FPU raises all 6 expected bits. Partial
     * raising (3/6 say) signals a real hardware or emulation gap. */
    fpu_fp_probe_exceptions((unsigned int __far *)exc_sw);
    exc_raised = fpu_fp_exceptions_count_raised(exc_sw);

    /* Emit individual probe results. Each is CONF_HIGH because the
     * behavior is deterministic on real hardware; MIXED cases drop the
     * family key to CONF_LOW to flag the anomaly. */
    report_add_str(t, "diagnose.fpu.infinity_mode",
                   probes.infinity_affine ? "affine" : "projective",
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.pseudo_nan",
                   probes.pseudo_nan_traps ? "traps" : "accepts",
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.has_fprem1",
                   probes.has_fprem1 ? "yes" : "no",
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.has_fsin",
                   probes.has_fsin ? "yes" : "no",
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.fptan_pushes_one",
                   probes.fptan_pushes_one ? "yes" : "no",
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.family_behavioral",
                   fpu_fp_family_name(family),
                   (family == FP_FAMILY_MIXED) ? CONF_LOW : CONF_HIGH,
                   VERDICT_UNKNOWN);

    /* M2.3 emit: per-mode rounding results (compact form, see display
     * buffer lifetime note near their declaration) + overall verdict. */
    sprintf(rnd_display_nearest, "%d,%d",
            rnd_results.nearest_pos, rnd_results.nearest_neg);
    sprintf(rnd_display_down, "%d,%d",
            rnd_results.down_pos, rnd_results.down_neg);
    sprintf(rnd_display_up, "%d,%d",
            rnd_results.up_pos, rnd_results.up_neg);
    sprintf(rnd_display_trunc, "%d,%d",
            rnd_results.trunc_pos, rnd_results.trunc_neg);
    report_add_str(t, "diagnose.fpu.rounding_nearest",
                   rnd_display_nearest, CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.rounding_down",
                   rnd_display_down, CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.rounding_up",
                   rnd_display_up, CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.rounding_truncate",
                   rnd_display_trunc, CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.rounding_modes_ok",
                   rounding_ok ? "yes" : "no",
                   CONF_HIGH,
                   rounding_ok ? VERDICT_PASS : VERDICT_WARN);

    /* M2.4 emit: precision-modes-ok summary only. Per-mode 10-byte
     * result buffers are kept in stack locals (small DGROUP footprint)
     * and not emitted to INI; the summary is the inference result of
     * whether the three results differ. If this row reports "no" on
     * real iron, that's the signal to add the detailed per-mode
     * forensic emit in a follow-up pass. */
    report_add_str(t, "diagnose.fpu.precision_modes_ok",
                   precision_ok ? "yes" : "no",
                   CONF_HIGH,
                   precision_ok ? VERDICT_PASS : VERDICT_WARN);

    /* M2.6 emit: count of exceptions that fired their expected bit.
     * A healthy FPU: 6/6. Emit as a compact "raised_N_of_6" token.
     * Per-exception detail is not emitted separately to preserve
     * DGROUP; if real-iron captures show anything less than 6/6 we
     * can expand the emit in a follow-up. */
    {
        static char exc_raised_display[16];
        sprintf(exc_raised_display, "%d_of_6", exc_raised);
        report_add_str(t, "diagnose.fpu.exceptions_raised",
                       exc_raised_display, CONF_HIGH,
                       (exc_raised == 6) ? VERDICT_PASS : VERDICT_WARN);
    }

    report_add_str(t, "diagnose.fpu.fingerprint_status", "ok",
                   CONF_HIGH, VERDICT_UNKNOWN);
}

#endif  /* CERBERUS_HOST_TEST */
