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

/* --- Pure inference helpers (host-testable) ----------------------- */

fp_family_t fpu_fp_infer_family(const fp_probe_result_t *r)
{
    /*
     * Inference logic:
     *   All 4 probes "modern" (affine / traps / executes / executes) -> MODERN (80387+)
     *   All 4 probes "legacy" (projective / accepts / traps / traps) -> LEGACY (8087 or 80287)
     *   Anything else                                                  -> MIXED (anomaly)
     *
     * The probe signals are independent, so a mixed result signals a
     * bug in our probe asm, an exotic emulator that only half-implements
     * x87, or a hardware anomaly worth flagging to the user.
     */
    int modern_score = 0;
    int legacy_score = 0;

    if (r->infinity_affine)    modern_score++; else legacy_score++;
    if (r->pseudo_nan_traps)   modern_score++; else legacy_score++;
    if (r->has_fprem1)         modern_score++; else legacy_score++;
    if (r->has_fsin)           modern_score++; else legacy_score++;

    if (modern_score == 4) return FP_FAMILY_MODERN;
    if (legacy_score == 4) return FP_FAMILY_LEGACY;
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

void diag_fpu_fingerprint(result_table_t *t)
{
    const result_t *fpu_entry;
    const char *fpu_val;
    fp_probe_result_t probes;
    fp_family_t family;
    unsigned int sw;

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

    /* Infer family from the four signals. */
    family = fpu_fp_infer_family(&probes);

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
    report_add_str(t, "diagnose.fpu.family_behavioral",
                   fpu_fp_family_name(family),
                   (family == FP_FAMILY_MIXED) ? CONF_LOW : CONF_HIGH,
                   VERDICT_UNKNOWN);
    report_add_str(t, "diagnose.fpu.fingerprint_status", "ok",
                   CONF_HIGH, VERDICT_UNKNOWN);
}

#endif  /* CERBERUS_HOST_TEST */
