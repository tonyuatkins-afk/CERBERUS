/*
 * Host-side test for Hercules variant classification (0.8.1 M3.3).
 *
 * Exercises only the pure classifier display_classify_hercules_id() and
 * the variant-token mapper display_hercules_variant_token(). The live
 * 3BAh probe is hardware-only and cannot be exercised on a host build.
 *
 * The classifier contract: given the 3 ID bits (bits 6:4 of 3BAh,
 * shifted into 2:0), return the documented Hercules variant. Bit
 * patterns not in the documented table bucket as UNKNOWN so we never
 * claim a specific model when the hardware reports something the
 * detection code hasn't catalogued.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include <stdio.h>
#include <string.h>
#include "../../src/core/display.h"

/* The classifier + token mapper are pure helpers with no DOS linkage.
 * Include the source directly to avoid a separate object compile. */
#include "../../src/core/display_hercules_ids.c"

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

static void test_documented_ids(void)
{
    printf("\n[test_documented_ids]\n");
    CHECK(display_classify_hercules_id(0x00) == HERCULES_VARIANT_HGC,
          "id=0x00 -> HGC (original 1982)");
    CHECK(display_classify_hercules_id(0x01) == HERCULES_VARIANT_HGCPLUS,
          "id=0x01 -> HGC+ (1986 softfont)");
    CHECK(display_classify_hercules_id(0x05) == HERCULES_VARIANT_INCOLOR,
          "id=0x05 -> InColor (1987 16-color)");
}

static void test_undocumented_ids_bucket_as_unknown(void)
{
    printf("\n[test_undocumented_ids_bucket_as_unknown]\n");
    CHECK(display_classify_hercules_id(0x02) == HERCULES_VARIANT_UNKNOWN,
          "id=0x02 -> UNKNOWN (reserved bit pattern)");
    CHECK(display_classify_hercules_id(0x03) == HERCULES_VARIANT_UNKNOWN,
          "id=0x03 -> UNKNOWN");
    CHECK(display_classify_hercules_id(0x04) == HERCULES_VARIANT_UNKNOWN,
          "id=0x04 -> UNKNOWN");
    CHECK(display_classify_hercules_id(0x06) == HERCULES_VARIANT_UNKNOWN,
          "id=0x06 -> UNKNOWN");
    CHECK(display_classify_hercules_id(0x07) == HERCULES_VARIANT_UNKNOWN,
          "id=0x07 -> UNKNOWN");
}

static void test_high_bits_ignored(void)
{
    /* The classifier takes the low 3 bits and ignores the high 5. The
     * live probe already does the >>4 + &0x07 shift; the classifier
     * contract is that any caller passing garbage high bits still gets
     * a correct variant for the low 3. */
    printf("\n[test_high_bits_ignored]\n");
    CHECK(display_classify_hercules_id(0xF8) == HERCULES_VARIANT_HGC,
          "0xF8 (high bits set, low 3 = 0) -> HGC");
    CHECK(display_classify_hercules_id(0xF9) == HERCULES_VARIANT_HGCPLUS,
          "0xF9 (high bits set, low 3 = 1) -> HGC+");
    CHECK(display_classify_hercules_id(0xFD) == HERCULES_VARIANT_INCOLOR,
          "0xFD (high bits set, low 3 = 5) -> InColor");
}

static void test_token_mapping(void)
{
    printf("\n[test_token_mapping]\n");
    CHECK(strcmp(display_hercules_variant_token(HERCULES_VARIANT_HGC), "hgc") == 0,
          "HGC -> 'hgc'");
    CHECK(strcmp(display_hercules_variant_token(HERCULES_VARIANT_HGCPLUS), "hgcplus") == 0,
          "HGC+ -> 'hgcplus'");
    CHECK(strcmp(display_hercules_variant_token(HERCULES_VARIANT_INCOLOR), "incolor") == 0,
          "InColor -> 'incolor'");
    CHECK(strcmp(display_hercules_variant_token(HERCULES_VARIANT_UNKNOWN), "unknown") == 0,
          "UNKNOWN -> 'unknown'");
    CHECK(strcmp(display_hercules_variant_token(HERCULES_VARIANT_NA), "na") == 0,
          "NA -> 'na'");
}

int main(void)
{
    test_documented_ids();
    test_undocumented_ids_bucket_as_unknown();
    test_high_bits_ignored();
    test_token_mapping();

    printf("\n=== %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
