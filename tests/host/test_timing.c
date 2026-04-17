/*
 * Host-side unit test for the timing module's pure-math helpers.
 * Compiled with -DCERBERUS_HOST_TEST so the hardware-specific code is
 * excluded and we don't drag in <conio.h>.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif
#include "../../src/core/timing.c"

#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

#define EXPECT_EQ_UL(actual, expected, label)                                 \
    do {                                                                      \
        unsigned long a_ = (unsigned long)(actual);                           \
        unsigned long e_ = (unsigned long)(expected);                         \
        if (a_ == e_) {                                                       \
            printf("  OK   %s: %lu\n", (label), a_);                          \
        } else {                                                              \
            printf("  FAIL %s: got %lu expected %lu\n", (label), a_, e_);     \
            failures++;                                                       \
        }                                                                     \
    } while (0)

static void test_ticks_to_us(void)
{
    printf("timing_ticks_to_us corner cases:\n");
    /* Formula: (ticks * 838 + 500) / 1000
     * These expected values are computed by the formula directly, not by
     * the "true" 838.095 ns value — the 0.01% systematic error is accepted
     * per the plan's methodology note. */
    EXPECT_EQ_UL(timing_ticks_to_us(0UL),         0UL,       "0 ticks");
    EXPECT_EQ_UL(timing_ticks_to_us(1UL),         1UL,       "1 tick");         /* (838+500)/1000 */
    EXPECT_EQ_UL(timing_ticks_to_us(100UL),       84UL,      "100 ticks");      /* (83800+500)/1000 = 84 */
    EXPECT_EQ_UL(timing_ticks_to_us(1000UL),      838UL,     "1000 ticks");     /* (838000+500)/1000 */
    EXPECT_EQ_UL(timing_ticks_to_us(65535UL),     54918UL,   "65535 ticks");    /* (54918330+500)/1000 = 54918 */
    EXPECT_EQ_UL(timing_ticks_to_us(1000000UL),   838000UL,  "1,000,000 ticks");
}

static void test_elapsed_ticks(void)
{
    printf("timing_elapsed_ticks (rollover math) — BOTH branches:\n");

    /* Normal case — counter went DOWN (start > stop) */
    EXPECT_EQ_UL(timing_elapsed_ticks(0xFFFF, 0x8000), 0x7FFFUL, "start=FFFF stop=8000 (no wrap)");
    EXPECT_EQ_UL(timing_elapsed_ticks(0xFFFF, 0xFFFE), 0x0001UL, "start=FFFF stop=FFFE (1 tick)");
    EXPECT_EQ_UL(timing_elapsed_ticks(0x8000, 0x0000), 0x8000UL, "start=8000 stop=0000 (halfway)");

    /* Wrap case — counter went down past zero and reloaded (stop > start) */
    EXPECT_EQ_UL(timing_elapsed_ticks(0x1000, 0xE000), 0x3000UL, "start=1000 stop=E000 (wrapped)");
    EXPECT_EQ_UL(timing_elapsed_ticks(0x0001, 0x0002), 0xFFFFUL, "start=0001 stop=0002 (max wrap)");
    EXPECT_EQ_UL(timing_elapsed_ticks(0x0000, 0xFFFF), 0x0001UL, "start=0000 stop=FFFF (min wrap)");

    /* Zero-elapsed edge case */
    EXPECT_EQ_UL(timing_elapsed_ticks(0x1234, 0x1234), 0UL,      "start=stop (zero elapsed)");
}

static void test_bios_ticks_to_us(void)
{
    printf("timing_bios_ticks_to_us (54925 us/tick):\n");
    EXPECT_EQ_UL(timing_bios_ticks_to_us(0UL),      0UL,        "0 bios ticks");
    EXPECT_EQ_UL(timing_bios_ticks_to_us(1UL),      54925UL,    "1 bios tick");
    EXPECT_EQ_UL(timing_bios_ticks_to_us(4UL),      219700UL,   "4 bios ticks (target interval)");
    EXPECT_EQ_UL(timing_bios_ticks_to_us(18UL),     988650UL,   "18 bios ticks (~1 sec)");
    /* Sanity — stay under the 32-bit overflow boundary. */
    EXPECT_EQ_UL(timing_bios_ticks_to_us(1000UL),   54925000UL, "1000 bios ticks (~55 sec)");
}

int main(void)
{
    printf("=== CERBERUS host unit test: timing ===\n");
    test_ticks_to_us();
    test_elapsed_ticks();
    test_bios_ticks_to_us();
    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
