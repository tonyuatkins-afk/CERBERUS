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
    printf("infer_family: all five signals modern -> MODERN (80387+):\n");
    r.infinity_affine    = 1;
    r.pseudo_nan_traps   = 1;
    r.has_fprem1         = 1;
    r.has_fsin           = 1;
    r.fptan_pushes_one   = 1;    /* M2.2 axis */
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MODERN,
          "5/5 modern -> FP_FAMILY_MODERN");
}

static void test_family_all_legacy(void)
{
    fp_probe_result_t r;
    printf("infer_family: all five signals legacy -> LEGACY (8087/80287):\n");
    r.infinity_affine    = 0;
    r.pseudo_nan_traps   = 0;
    r.has_fprem1         = 0;
    r.has_fsin           = 0;
    r.fptan_pushes_one   = 0;    /* M2.2 axis */
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_LEGACY,
          "5/5 legacy -> FP_FAMILY_LEGACY");
}

static void test_family_mixed(void)
{
    fp_probe_result_t r;
    printf("infer_family: mixed signals -> MIXED (anomaly):\n");

    /* Partial modern: 4 modern + 1 legacy on FPTAN axis */
    r.infinity_affine    = 1;
    r.pseudo_nan_traps   = 1;
    r.has_fprem1         = 1;
    r.has_fsin           = 1;
    r.fptan_pushes_one   = 0;          /* legacy */
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MIXED,
          "4 modern + 1 legacy (FPTAN) -> MIXED");

    /* Partial legacy: 4 legacy + 1 modern */
    r.infinity_affine    = 0;
    r.pseudo_nan_traps   = 0;
    r.has_fprem1         = 0;
    r.has_fsin           = 0;
    r.fptan_pushes_one   = 1;          /* modern */
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MIXED,
          "4 legacy + 1 modern (FPTAN) -> MIXED");

    /* BEK-V409-shaped: 4 modern + 1 legacy on has_fsin (pre-M2 observation) */
    r.infinity_affine    = 1;
    r.pseudo_nan_traps   = 1;
    r.has_fprem1         = 1;
    r.has_fsin           = 1;
    r.fptan_pushes_one   = 0;
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MIXED,
          "BEK-V409-shaped 4/1 split -> MIXED");

    /* Half/half */
    r.infinity_affine    = 1;
    r.pseudo_nan_traps   = 0;
    r.has_fprem1         = 1;
    r.has_fsin           = 0;
    r.fptan_pushes_one   = 1;
    CHECK(fpu_fp_infer_family(&r) == FP_FAMILY_MIXED,
          "3-modern/2-legacy split -> MIXED");
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

    /* M2.2: exhaustively walk all 32 combinations of the 5 signals
     * (affine, traps, fprem1, fsin, fptan_pushes_one) to confirm the
     * inference table has exactly two "pure" outcomes and all 30
     * others are MIXED. */
    int modern_count = 0, legacy_count = 0, mixed_count = 0;

    printf("infer_family: exhaustive 32-combo sweep:\n");
    for (i = 0; i < 32U; i++) {
        r.infinity_affine    = (i & 0x01) ? 1 : 0;
        r.pseudo_nan_traps   = (i & 0x02) ? 1 : 0;
        r.has_fprem1         = (i & 0x04) ? 1 : 0;
        r.has_fsin           = (i & 0x08) ? 1 : 0;
        r.fptan_pushes_one   = (i & 0x10) ? 1 : 0;
        switch (fpu_fp_infer_family(&r)) {
            case FP_FAMILY_MODERN: modern_count++; break;
            case FP_FAMILY_LEGACY: legacy_count++; break;
            case FP_FAMILY_MIXED:  mixed_count++;  break;
            default: break;
        }
    }
    CHECK(modern_count == 1,  "exactly 1 of 32 combos -> MODERN");
    CHECK(legacy_count == 1,  "exactly 1 of 32 combos -> LEGACY");
    CHECK(mixed_count  == 30, "exactly 30 of 32 combos -> MIXED");
}

/* M2.3 rounding-control cross-check inference */

static void test_rounding_ieee_conformant(void)
{
    fp_rounding_result_t r;
    printf("rounding_is_conformant: canonical IEEE-754 table -> yes:\n");
    r.nearest_pos = 2; r.nearest_neg = -2;
    r.down_pos    = 1; r.down_neg    = -2;
    r.up_pos      = 2; r.up_neg      = -1;
    r.trunc_pos   = 1; r.trunc_neg   = -1;
    CHECK(fpu_fp_rounding_is_conformant(&r) == 1,
          "IEEE canonical table -> 1 (conformant)");
}

static void test_rounding_nearest_broken(void)
{
    fp_rounding_result_t r;
    printf("rounding_is_conformant: RC=nearest misbehaves -> no:\n");
    r.nearest_pos = 1;   /* wrong — should be 2 */
    r.nearest_neg = -2;
    r.down_pos    = 1; r.down_neg    = -2;
    r.up_pos      = 2; r.up_neg      = -1;
    r.trunc_pos   = 1; r.trunc_neg   = -1;
    CHECK(fpu_fp_rounding_is_conformant(&r) == 0,
          "RC=nearest misbehaves -> 0");
}

static void test_rounding_all_zero(void)
{
    fp_rounding_result_t r;
    /* Zero-init: most commonly indicates the asm probe never ran.
     * Must not be conformant. */
    memset(&r, 0, sizeof(r));
    printf("rounding_is_conformant: zero-init -> no:\n");
    CHECK(fpu_fp_rounding_is_conformant(&r) == 0, "zero-init -> 0");
}

static void test_rounding_null_input(void)
{
    printf("rounding_is_conformant: NULL input -> no:\n");
    CHECK(fpu_fp_rounding_is_conformant((fp_rounding_result_t *)0) == 0,
          "NULL input -> 0");
}

static void test_rounding_modes_all_uniquely_broken(void)
{
    fp_rounding_result_t r;
    printf("rounding_is_conformant: each mode tested individually:\n");
    /* Default: canonical */
    r.nearest_pos = 2; r.nearest_neg = -2;
    r.down_pos    = 1; r.down_neg    = -2;
    r.up_pos      = 2; r.up_neg      = -1;
    r.trunc_pos   = 1; r.trunc_neg   = -1;

    /* Mutate each of the 8 values individually and confirm all mutations
     * cause non-conformant. Covers the rule-per-mode logic. */
    {
        fp_rounding_result_t m;
        int mutations_detected = 0;
        int axis;
        for (axis = 0; axis < 8; axis++) {
            m = r;
            switch (axis) {
                case 0: m.nearest_pos += 1; break;
                case 1: m.nearest_neg += 1; break;
                case 2: m.down_pos    += 1; break;
                case 3: m.down_neg    += 1; break;
                case 4: m.up_pos      += 1; break;
                case 5: m.up_neg      += 1; break;
                case 6: m.trunc_pos   += 1; break;
                case 7: m.trunc_neg   += 1; break;
            }
            if (!fpu_fp_rounding_is_conformant(&m)) mutations_detected++;
        }
        CHECK(mutations_detected == 8,
              "all 8 single-axis mutations detected as non-conformant");
    }
}

/* M2.4 precision-control cross-check inference */

static void test_precision_three_distinct(void)
{
    unsigned char s[10] = {0x56,0x55,0x55,0x55,0x55,0x55,0x55,0xAA,0xFD,0x3F};
    unsigned char d[10] = {0x55,0x55,0x55,0x55,0x55,0x55,0x55,0xAA,0xFD,0x3F};
    unsigned char e[10] = {0x55,0x55,0x55,0x55,0x55,0x55,0x55,0xAA,0xFE,0x3F};
    printf("precision_modes_distinct: three distinct buffers -> yes:\n");
    CHECK(fpu_fp_precision_modes_distinct(s, d, e) == 1,
          "three distinct -> 1 (distinct)");
}

static void test_precision_two_identical(void)
{
    unsigned char x[10] = {0,0,0,0,0,0,0,0xAA,0xFD,0x3F};
    unsigned char y[10] = {0,0,0,0,0,0,0,0xAA,0xFD,0x3F};   /* dup of x */
    unsigned char z[10] = {0,0,0,0,0,0,0,0xAA,0xFE,0x3F};
    printf("precision_modes_distinct: two identical -> no:\n");
    CHECK(fpu_fp_precision_modes_distinct(x, y, z) == 0,
          "first two identical -> 0 (NOT distinct)");
    CHECK(fpu_fp_precision_modes_distinct(x, z, x) == 0,
          "first+third identical -> 0");
    CHECK(fpu_fp_precision_modes_distinct(z, y, y) == 0,
          "second+third identical -> 0");
}

static void test_precision_null_inputs(void)
{
    unsigned char v[10] = {0,0,0,0,0,0,0,0xAA,0xFD,0x3F};
    printf("precision_modes_distinct: NULL input -> no:\n");
    CHECK(fpu_fp_precision_modes_distinct((unsigned char *)0, v, v) == 0,
          "first NULL -> 0");
    CHECK(fpu_fp_precision_modes_distinct(v, (unsigned char *)0, v) == 0,
          "second NULL -> 0");
    CHECK(fpu_fp_precision_modes_distinct(v, v, (unsigned char *)0) == 0,
          "third NULL -> 0");
}

/* M2.6 exception-flag roundtrip inference */

static void test_exceptions_all_six_raised(void)
{
    /* Each SW has the corresponding expected bit set (plus noise). */
    unsigned int sw[6];
    sw[0] = 0x0001U | 0x4000U;  /* IE set + other noise */
    sw[1] = 0x0002U;            /* DE */
    sw[2] = 0x0004U | 0x0001U;  /* ZE + IE noise (irrelevant for ZE check) */
    sw[3] = 0x0008U;            /* OE */
    sw[4] = 0x0010U;            /* UE */
    sw[5] = 0x0020U | 0x4000U;  /* PE + noise */
    printf("exceptions_count_raised: all 6 fire their bit -> 6:\n");
    CHECK(fpu_fp_exceptions_count_raised(sw) == 6, "6/6 raised -> 6");

    printf("exceptions_bitmap: all 6 -> 0x003F:\n");
    CHECK(fpu_fp_exceptions_bitmap(sw) == 0x003FU, "bitmap = 0x3F");
}

static void test_exceptions_three_raised(void)
{
    /* Only IE, ZE, PE raise their expected bits. DE, OE, UE silent. */
    unsigned int sw[6];
    sw[0] = 0x0001U;      /* IE fires */
    sw[1] = 0x0000U;      /* DE silent */
    sw[2] = 0x0004U;      /* ZE fires */
    sw[3] = 0x0000U;      /* OE silent */
    sw[4] = 0x0000U;      /* UE silent */
    sw[5] = 0x0020U;      /* PE fires */
    printf("exceptions_count_raised: 3 of 6 -> 3:\n");
    CHECK(fpu_fp_exceptions_count_raised(sw) == 3, "3/6 raised -> 3");

    printf("exceptions_bitmap: IE+ZE+PE -> 0x25:\n");
    CHECK(fpu_fp_exceptions_bitmap(sw) == 0x25U, "bitmap = 0x25");
}

static void test_exceptions_zero_raised(void)
{
    unsigned int sw[6] = {0, 0, 0, 0, 0, 0};
    printf("exceptions_count_raised: none fire -> 0:\n");
    CHECK(fpu_fp_exceptions_count_raised(sw) == 0, "0/6 -> 0");
    CHECK(fpu_fp_exceptions_bitmap(sw) == 0U, "bitmap = 0");
}

static void test_exceptions_bitmap_construction(void)
{
    unsigned int sw[6];
    /* Wrong bit set in some slots: DE position has IE bit set, etc.
     * The inference should only count the EXPECTED bit per position. */
    sw[0] = 0x0001U;      /* position 0: IE expected = 0x01, got 0x01 -> counted */
    sw[1] = 0x0001U;      /* position 1: DE expected = 0x02, got 0x01 -> NOT counted */
    sw[2] = 0x0004U;      /* position 2: ZE = 0x04, got 0x04 -> counted */
    sw[3] = 0x0010U;      /* position 3: OE = 0x08, got 0x10 (UE bit) -> NOT counted */
    sw[4] = 0x0010U;      /* position 4: UE = 0x10, got 0x10 -> counted */
    sw[5] = 0x0001U;      /* position 5: PE = 0x20, got 0x01 -> NOT counted */
    printf("exceptions: position-dependent bit checking:\n");
    CHECK(fpu_fp_exceptions_count_raised(sw) == 3,
          "position-matched bits counted, mismatches ignored -> 3");
    CHECK(fpu_fp_exceptions_bitmap(sw) == 0x15U,
          "bitmap = IE+ZE+UE = 0x15");
}

static void test_exceptions_null_inputs(void)
{
    printf("exceptions: NULL inputs:\n");
    CHECK(fpu_fp_exceptions_count_raised((unsigned int *)0) == -1,
          "NULL -> -1");
    CHECK(fpu_fp_exceptions_bitmap((unsigned int *)0) == 0U,
          "NULL bitmap -> 0");
}

int main(void)
{
    printf("=== CERBERUS host unit test: diag_fpu_fingerprint ===\n");
    test_family_all_modern();
    test_family_all_legacy();
    test_family_mixed();
    test_family_name_mapping();
    test_signal_coverage();
    /* M2.3 rounding-control tests */
    test_rounding_ieee_conformant();
    test_rounding_nearest_broken();
    test_rounding_all_zero();
    test_rounding_null_input();
    test_rounding_modes_all_uniquely_broken();
    /* M2.4 precision-control tests */
    test_precision_three_distinct();
    test_precision_two_identical();
    test_precision_null_inputs();
    /* M2.6 exception-roundtrip tests */
    test_exceptions_all_six_raised();
    test_exceptions_three_raised();
    test_exceptions_zero_raised();
    test_exceptions_bitmap_construction();
    test_exceptions_null_inputs();
    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
