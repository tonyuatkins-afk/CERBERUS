#include "timing.h"

static int emulator_flag = 0;

us_t timing_ticks_to_us(unsigned long ticks)
{
    return (us_t)((ticks * 838UL + 500UL) / 1000UL);
}

void timing_init(void)
{
    /* Phase 0 stub — full PIT C2 sanity probe lands in Task 0.2 */
    emulator_flag = 0;
}

void timing_start(void)
{
    /* Phase 0 stub — PIT C2 gate-based start in Task 0.2 */
}

us_t timing_stop(void)
{
    /* Phase 0 stub — returns 0 until Task 0.2 wires the ASM */
    return 0UL;
}

void timing_wait_us(us_t microseconds)
{
    /* Phase 0 stub — real impl uses PIT Channel 0 in Task 0.2 */
    (void)microseconds;
}

int timing_emulator_hint(void)
{
    return emulator_flag;
}
