/*
 * Host-side unit test for bench_cache_char's pure inference helpers:
 *   bench_cc_infer_l1_kb         (footprint drop -> L1 size)
 *   bench_cc_infer_line_bytes    (stride drop -> line size)
 *   bench_cc_infer_write_policy  (read vs write ratio -> wb/wt/unknown)
 *
 * The stride/write/read kernels themselves exercise FAR memory and PIT
 * timing — neither fits the host-test sandbox. Those are validated on
 * hardware / DOSBox Staging via the Batch B gate capture.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif
#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/bench/bench_cache_char.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { printf("  OK   %s\n", (msg)); }                           \
        else      { printf("  FAIL %s\n", (msg)); failures++; }               \
    } while (0)

#define EXPECT_EQ_U(actual, expected, label)                                  \
    do {                                                                      \
        unsigned int a_ = (unsigned int)(actual);                             \
        unsigned int e_ = (unsigned int)(expected);                           \
        if (a_ == e_) { printf("  OK   %s: %u\n", (label), a_); }             \
        else { printf("  FAIL %s: got %u expected %u\n", (label), a_, e_);    \
               failures++; }                                                  \
    } while (0)

/* ======================================================================= */
/* L1 size inference                                                        */
/* ======================================================================= */

static void test_l1_drop_at_8kb(void)
{
    /* Classic 486 8 KB L1: 2/4/8 KB cached, 16/32 KB miss. */
    unsigned long kbps[5] = { 32000, 31000, 31500, 15000, 14500 };
    printf("infer_l1_kb: drop 8->16KB -> L1=8:\n");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(kbps, 5), 8U, "l1=8KB");
}

static void test_l1_real_iron_bek_v409(void)
{
    /* Real-iron capture from BEK-V409 (486 DX2-66, 2026-04-21 v0.7.1):
     * size sweep 2/4/8/16/32 KB = 1960/1960/1880/1512/1511 KB/s.
     * Drop at 16 KB = 22% below baseline 1960. Should detect L1=8 KB. */
    unsigned long kbps[5] = { 1960, 1960, 1880, 1512, 1511 };
    printf("infer_l1_kb: BEK-V409 real-iron 486 DX2 -> L1=8:\n");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(kbps, 5), 8U, "BEK-V409 L1=8KB");
}

static void test_l1_small_drop_still_detected(void)
{
    /* 16% drop at 8->16 KB should trigger (threshold is 15%). */
    unsigned long kbps[5] = { 2000, 2000, 2000, 1680, 1680 };
    printf("infer_l1_kb: 16%% drop triggers detection:\n");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(kbps, 5), 8U, "16pct drop -> L1=8");
}

static void test_l1_tiny_drop_does_not_trigger(void)
{
    /* 10% drop at 8->16 KB should NOT trigger (threshold is 15%). */
    unsigned long kbps[5] = { 2000, 2000, 2000, 1800, 1790 };
    printf("infer_l1_kb: 10%% drop does NOT trigger (below threshold):\n");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(kbps, 5), 0U, "10pct drop -> inconclusive");
}

static void test_l1_drop_at_4kb(void)
{
    unsigned long kbps[5] = { 40000, 39000, 8000, 7800, 7500 };
    printf("infer_l1_kb: drop 4->8KB -> L1=4:\n");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(kbps, 5), 4U, "l1=4KB");
}

static void test_l1_drop_at_16kb(void)
{
    /* Pentium-class might have 16 KB L1 or larger. */
    unsigned long kbps[5] = { 80000, 79000, 78000, 77000, 20000 };
    printf("infer_l1_kb: drop 16->32KB -> L1=16:\n");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(kbps, 5), 16U, "l1=16KB");
}

static void test_l1_no_drop(void)
{
    /* All hits — L1 must be >= 32 KB or something else. Report 0. */
    unsigned long kbps[5] = { 50000, 49500, 49000, 48500, 48000 };
    printf("infer_l1_kb: no drop -> 0 (inconclusive):\n");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(kbps, 5), 0U, "no drop -> 0");
}

static void test_l1_degenerate(void)
{
    unsigned long empty[5] = { 0, 0, 0, 0, 0 };
    printf("infer_l1_kb: degenerate inputs:\n");
    EXPECT_EQ_U(bench_cc_infer_l1_kb((const unsigned long *)0, 5), 0U,
                "NULL input -> 0");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(empty, 1), 0U, "n<2 -> 0");
    EXPECT_EQ_U(bench_cc_infer_l1_kb(empty, 5), 0U, "all-zero -> 0");
}

/* ======================================================================= */
/* Line size inference                                                      */
/* ======================================================================= */

static void test_line_real_iron_bek_v409(void)
{
    /* Real-iron capture from BEK-V409 (486 DX2-66, 2026-04-21 v0.7.1):
     * strides 4/8/16/32/64 = 1826/1707/1502/1509/1518 KB/s.
     * Strides 16, 32, 64 plateau within 1% of each other after the
     * drop from 1707 to 1502. Should detect line=16 B (the first
     * stride where the plateau starts). */
    unsigned long kbps[5] = { 1826, 1707, 1502, 1509, 1518 };
    printf("infer_line_bytes: BEK-V409 real-iron 486 DX2 -> line=16:\n");
    EXPECT_EQ_U(bench_cc_infer_line_bytes(kbps, 5), 16U, "BEK-V409 line=16B");
}

static void test_line_classic_486_shape(void)
{
    /* Same shape as BEK but with a clearer drop-then-plateau signature. */
    unsigned long kbps[5] = { 30000, 25000, 10000, 10200, 10100 };
    printf("infer_line_bytes: classic 486 shape -> line=16:\n");
    EXPECT_EQ_U(bench_cc_infer_line_bytes(kbps, 5), 16U, "line=16B");
}

/*
 * v0.8.0 M2.1: the stride sweep extended from 5 to 6 points by adding
 * stride=128. With the new final point, line=32 (Pentium) and line=64
 * (Pentium Pro+) become detectable via the 2-step plateau guard. The
 * existing 5-point tests still work (the inference function caps
 * n_strides at 6 internally, so passing 5 is backward-compatible) and
 * validate legacy line=16 (486) coverage. The M2.1 additions below
 * validate the new line=32 and line=64 coverage.
 */

static void test_line_pentium_shape_line_32(void)
{
    /* Synthetic Pentium shape: line=32 means strides 32, 64, 128 plateau
     * at the same rate (all miss once per line). Strides < 32 are still
     * stride-sensitive because one line carries multiple accesses. */
    unsigned long kbps[6] = { 50000, 42000, 30000, 12000, 12100, 12050 };
    printf("infer_line_bytes: Pentium line=32 -> plateau at stride 32-64-128:\n");
    EXPECT_EQ_U(bench_cc_infer_line_bytes(kbps, 6), 32U, "Pentium line=32B");
}

static void test_line_pentium_pro_shape_line_64(void)
{
    /* Pentium Pro / PII / PIII: line=64 requires strides 64 and 128
     * to plateau. With only 2 plateau points the 2-step guard wants 3
     * consecutive values; the guard begins at stride[3]=32, requires
     * ratio(32,64) and ratio(64,128) both within 8%. On a hypothetical
     * PPro the stride-32 read already mostly hits the same line as
     * stride-64 because 32 is half a line (two accesses per line, not
     * four as with stride 16). So the "plateau" can effectively start
     * at stride=32 with PPro line=64. Test data reflects that. */
    unsigned long kbps[6] = { 80000, 65000, 45000, 18000, 17500, 17400 };
    printf("infer_line_bytes: PPro line=64-ish plateau -> detected=32:\n");
    /* Documented expected: this produces line_bytes=32 because the
     * plateau starts at stride=32. A fully-rigorous line=64 detection
     * would need a stride=256 point. This test pins the current
     * behavior for M2 regression coverage. */
    EXPECT_EQ_U(bench_cc_infer_line_bytes(kbps, 6), 32U, "PPro plateau at 32+");
}

static void test_line_no_plateau(void)
{
    /* Monotonic drop through the whole sweep: inconclusive (no plateau
     * formed, so we cannot call a line size). */
    unsigned long kbps[6] = { 20000, 16000, 12000, 8000, 4000, 2000 };
    printf("infer_line_bytes: no plateau -> 0:\n");
    EXPECT_EQ_U(bench_cc_infer_line_bytes(kbps, 6), 0U, "no plateau -> 0");
}

/* ======================================================================= */
/* Write policy inference                                                   */
/* ======================================================================= */

static void test_wp_wb(void)
{
    printf("infer_write_policy: write-back (writes ~= reads):\n");
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 9500),  "wb") == 0,
          "read=10000 write=9500 -> wb (95%%)");
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 8000),  "wb") == 0,
          "read=10000 write=8000 -> wb (80%%)");
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 7500),  "wb") == 0,
          "read=10000 write=7500 -> wb (75%%, threshold)");
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 10000), "wb") == 0,
          "read=write -> wb");
}

static void test_wp_wt(void)
{
    printf("infer_write_policy: write-through (write << read):\n");
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 5000), "wt") == 0,
          "read=10000 write=5000 -> wt (50%%)");
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 1500), "wt") == 0,
          "read=10000 write=1500 -> wt (15%%)");
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 5999), "wt") == 0,
          "read=10000 write=5999 -> wt (just under 60%%)");
}

static void test_wp_wb_store_buffer(void)
{
    printf("infer_write_policy: write>read (write-back + store buffer):\n");
    /* BEK-V409 real-iron: wp_read=1960, wp_write=2759. 1.41x ratio.
     * Classic write-back with store buffer — writes queue while the
     * CPU continues; reads must complete. Now classified as wb. */
    CHECK(strcmp(bench_cc_infer_write_policy(1960, 2759), "wb") == 0,
          "BEK-V409: write>read -> wb (store buffer signature)");
    CHECK(strcmp(bench_cc_infer_write_policy(1000, 5000), "wb") == 0,
          "5x write speed -> wb");
}

static void test_wp_unknown(void)
{
    printf("infer_write_policy: ambiguous/degenerate:\n");
    /* 60-75% range is ambiguous: declared unknown. */
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 6500), "unknown") == 0,
          "read=10000 write=6500 -> unknown (65%%, ambiguous)");
    CHECK(strcmp(bench_cc_infer_write_policy(10000, 7000), "unknown") == 0,
          "read=10000 write=7000 -> unknown (70%%)");
    /* Zero inputs */
    CHECK(strcmp(bench_cc_infer_write_policy(0, 5000), "unknown") == 0,
          "read=0 -> unknown");
    CHECK(strcmp(bench_cc_infer_write_policy(5000, 0), "unknown") == 0,
          "write=0 -> unknown");
}

int main(void)
{
    printf("=== CERBERUS host unit test: bench_cache_char ===\n");
    test_l1_drop_at_8kb();
    test_l1_drop_at_4kb();
    test_l1_drop_at_16kb();
    test_l1_real_iron_bek_v409();
    test_l1_small_drop_still_detected();
    test_l1_tiny_drop_does_not_trigger();
    test_l1_no_drop();
    test_l1_degenerate();
    test_line_real_iron_bek_v409();
    test_line_classic_486_shape();
    test_line_pentium_shape_line_32();
    test_line_pentium_pro_shape_line_64();
    test_line_no_plateau();
    test_wp_wb();
    test_wp_wt();
    test_wp_wb_store_buffer();
    test_wp_unknown();
    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
