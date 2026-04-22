/*
 * Host-side unit test for diag_fpu_fingerprint's pure inference helpers.
 * The probe asm lives in diag_fpu_fingerprint_a.asm and can only run on
 * DOS (and only when an FPU is actually present); this test covers the
 * table-driven family inference via fpu_fp_infer_family and the
 * fpu_fp_family_name string mapping.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif
#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/diag/diag_fpu_fingerprint.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { printf("  OK   %s\n", (msg)); }                           \
        else      { printf("  FAIL %s\n", (msg)); failures++; }               \
    } while (0)

static void test_family_all_modern(void)
{
    fp_probe_result_t r;
    printf("infer_family: all four signals modern -> MODERN (80387+):\n");
    r.infinity_affine  = 1;
    r.pseudo_nan_traps = 1;
    r.has_fprem1       = 1;
    r.has_fsin         = 1;
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MODERN,
          "4/4 modern -> FP_FAMILY_MODERN");
}

static void test_family_all_legacy(void)
{
    fp_probe_result_t r;
    printf("infer_family: all four signals legacy -> LEGACY (8087/80287):\n");
    r.infinity_affine  = 0;
    r.pseudo_nan_traps = 0;
    r.has_fprem1       = 0;
    r.has_fsin         = 0;
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_LEGACY,
          "4/4 legacy -> FP_FAMILY_LEGACY");
}

static void test_family_mixed(void)
{
    fp_probe_result_t r;
    printf("infer_family: mixed signals -> MIXED (anomaly):\n");

    /* Partial modern: 3 modern + 1 legacy */
    r.infinity_affine  = 1;
    r.pseudo_nan_traps = 1;
    r.has_fprem1       = 1;
    r.has_fsin         = 0;          /* legacy */
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MIXED,
          "3 modern + 1 legacy -> MIXED");

    /* Partial legacy: 3 legacy + 1 modern */
    r.infinity_affine  = 0;
    r.pseudo_nan_traps = 0;
    r.has_fprem1       = 1;          /* modern */
    r.has_fsin         = 0;
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MIXED,
          "3 legacy + 1 modern -> MIXED");

    /* Half/half */
    r.infinity_affine  = 1;
    r.pseudo_nan_traps = 0;
    r.has_fprem1       = 1;
    r.has_fsin         = 0;
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MIXED,
          "2/2 split -> MIXED");

    /* Opposite half/half */
    r.infinity_affine  = 0;
    r.pseudo_nan_traps = 1;
    r.has_fprem1       = 0;
    r.has_fsin         = 1;
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MIXED,
          "opposite 2/2 split -> MIXED");
}

static void test_family_name_mapping(void)
{
    printf("fpu_fp_family_name (enum -> INI token):\n");
    CHECK(strcmp(fpu_fp_family_name(FP_FAMILY_LEGACY),  "8087_or_80287")   == 0,
          "FP_FAMILY_LEGACY -> \"8087_or_80287\"");
    CHECK(strcmp(fpu_fp_family_name(FP_FAMILY_MODERN),  "80387_or_newer")  == 0,
          "FP_FAMILY_MODERN -> \"80387_or_newer\"");
    CHECK(strcmp(fpu_fp_family_name(FP_FAMILY_MIXED),   "mixed")           == 0,
          "FP_FAMILY_MIXED -> \"mixed\"");
    CHECK(strcmp(fpu_fp_family_name(FP_FAMILY_UNKNOWN), "unknown")         == 0,
          "FP_FAMILY_UNKNOWN -> \"unknown\"");
    /* Defensive: out-of-range enum values get a stable token. */
    CHECK(strcmp(fpu_fp_family_name((fp_family_t)99), "unknown") == 0,
          "out-of-range -> \"unknown\"");
}

static void test_signal_coverage(void)
{
    fp_probe_result_t r;
    unsigned int i;

    /* Exhaustively walk all 16 combinations of (affine, traps, fprem1, fsin)
     * to confirm the inference table has exactly two "pure" outcomes and
     * all 14 others are MIXED. */
    int modern_count = 0, legacy_count = 0, mixed_count = 0;

    printf("infer_family: exhaustive 16-combo sweep:\n");
    for (i = 0; i < 16U; i++) {
        r.infinity_affine  = (i & 0x1) ? 1 : 0;
        r.pseudo_nan_traps = (i & 0x2) ? 1 : 0;
        r.has_fprem1       = (i & 0x4) ? 1 : 0;
        r.has_fsin         = (i & 0x8) ? 1 : 0;
        switch (fpu_fp_infer_family(&r)) {
            case FP_FAMILY_MODERN: modern_count++; break;
            case FP_FAMILY_LEGACY: legacy_count++; break;
            case FP_FAMILY_MIXED:  mixed_count++;  break;
            default: break;
        }
    }
    CHECK(modern_count == 1, "exactly 1 of 16 combos -> MODERN");
    CHECK(legacy_count == 1, "exactly 1 of 16 combos -> LEGACY");
    CHECK(mixed_count  == 14, "exactly 14 of 16 combos -> MIXED");
}

int main(void)
{
    printf("=== CERBERUS host unit test: diag_fpu_fingerprint ===\n");
    test_family_all_modern();
    test_family_all_legacy();
    test_family_mixed();
    test_family_name_mapping();
    test_signal_coverage();
    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
