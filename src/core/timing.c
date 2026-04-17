/*
 * CERBERUS timing subsystem
 *
 * PIT Channel 2 gate-based measurement. Channel 2 is used because its gate
 * is software-controlled (via port 61h bit 0) without affecting Channel 0,
 * the BIOS system tick.
 *
 * Resolution: ~838ns per PIT tick (1.193182 MHz).
 *
 * CRITICAL: PIT counts DOWN in Mode 2. The "normal" case is stop_count <
 * start_count (counter decreased). Wraparound is stop_count > start_count
 * (counter passed zero and reloaded). Getting this backwards silently
 * corrupts every measurement on slow real hardware where the wrap case is
 * actually exercised (DOSBox-X rarely exercises wrap because emulation is
 * too fast — this makes the bug invisible in emulation).
 *
 * The math primitives below are guarded against CERBERUS_HOST_TEST so the
 * host unit tests can exercise them without dragging in <conio.h>.
 */

#include "timing.h"

/* ----------------------------------------------------------------------- */
/* Pure portable math — host-testable                                      */
/* ----------------------------------------------------------------------- */

us_t timing_ticks_to_us(unsigned long ticks)
{
    /*
     * Target: ticks * (1000000 / 1193182) microseconds.
     * Approximation used: ticks * 838 / 1000, +500 for round-to-nearest.
     * Systematic error ~0.01% — acceptable for the ±10% benchmark match goal.
     * MUST use unsigned long throughout — ticks * 838 overflows 16-bit int
     * at ticks >= 78 (feedback_dos_16bit_int.md).
     */
    return (us_t)((ticks * 838UL + 500UL) / 1000UL);
}

unsigned long timing_elapsed_ticks(unsigned int start_count,
                                   unsigned int stop_count)
{
    /*
     * PIT Channel 2 counts DOWN in Mode 2, reloading from 0xFFFF at zero.
     * If the counter didn't wrap during measurement, start > stop and
     * elapsed = start - stop.
     * If the counter wrapped once, stop > start and we need to add 65536.
     * Equal values are the zero-elapsed edge case.
     */
    if (stop_count <= start_count) {
        return (unsigned long)start_count - (unsigned long)stop_count;
    } else {
        return 65536UL + (unsigned long)start_count - (unsigned long)stop_count;
    }
}

/* ----------------------------------------------------------------------- */
/* Hardware layer — excluded from host tests                                */
/* ----------------------------------------------------------------------- */

#ifndef CERBERUS_HOST_TEST

#include <conio.h>

/* PIT 8254 register addresses */
#define PIT_CH0_DATA      0x40
#define PIT_CH2_DATA      0x42
#define PIT_CTRL          0x43
#define SPEAKER_CTRL      0x61

/* Control bytes (command register format: SC1 SC0 RW1 RW0 M2 M1 M0 BCD) */
#define CTRL_C0_LATCH     0x00  /* 00 00 0000 : channel 0 counter latch */
#define CTRL_C2_LATCH     0x80  /* 10 00 0000 : channel 2 counter latch */
#define CTRL_C2_MODE2     0xB4  /* 10 11 010 0 : C2, LSB/MSB access, mode 2, binary */

/* Port 61h bit map:
 *   bit 0 = PIT Channel 2 gate enable
 *   bit 1 = speaker data enable (we always want this 0)
 */
#define SPK_GATE          0x01
#define SPK_DATA          0x02
#define SPK_MASK          0x03

static int           emulator_flag         = 0;
static unsigned int  measurement_start_ct  = 0xFFFF;

/* --- Raw PIT I/O primitives --- */

static unsigned int pit_read_c2(void)
{
    unsigned int lo, hi;
    outp(PIT_CTRL, CTRL_C2_LATCH);
    lo = (unsigned int)(inp(PIT_CH2_DATA) & 0xFF);
    hi = (unsigned int)(inp(PIT_CH2_DATA) & 0xFF);
    return (hi << 8) | lo;
}

static unsigned int pit_read_c0(void)
{
    unsigned int lo, hi;
    outp(PIT_CTRL, CTRL_C0_LATCH);
    lo = (unsigned int)(inp(PIT_CH0_DATA) & 0xFF);
    hi = (unsigned int)(inp(PIT_CH0_DATA) & 0xFF);
    return (hi << 8) | lo;
}

static void c2_gate_off(void)
{
    outp(SPEAKER_CTRL, inp(SPEAKER_CTRL) & (unsigned char)~SPK_MASK);
}

static void c2_gate_on(void)
{
    unsigned char v = (unsigned char)(inp(SPEAKER_CTRL) & ~SPK_MASK);
    outp(SPEAKER_CTRL, v | SPK_GATE);
}

static void c2_program_mode2_max(void)
{
    outp(PIT_CTRL, CTRL_C2_MODE2);
    outp(PIT_CH2_DATA, 0xFF);   /* LSB */
    outp(PIT_CH2_DATA, 0xFF);   /* MSB */
}

/* --- Public API --- */

void timing_init(void)
{
    /*
     * XT-clone sanity probe: on some XT clones Channel 2 is wired oddly
     * to the speaker such that the gate bit doesn't actually enable the
     * counter. Verify that the counter advances between two reads with a
     * small delay; if it doesn't, flag timing as emulator/broken so all
     * downstream confidence is capped.
     */
    unsigned int c1, c2;
    volatile unsigned int i;

    emulator_flag = 0;

    c2_gate_off();
    c2_program_mode2_max();
    c2_gate_on();

    c1 = pit_read_c2();
    for (i = 0; i < 500; i++) { /* small delay loop */ }
    c2 = pit_read_c2();

    /* If counter didn't move at all, PIT C2 gate isn't working on this box */
    if (c1 == c2) {
        emulator_flag = 1;
    }

    c2_gate_off();
}

void timing_start(void)
{
    c2_gate_off();
    c2_program_mode2_max();
    measurement_start_ct = 0xFFFF;
    c2_gate_on();
}

us_t timing_stop(void)
{
    unsigned int stop_ct = pit_read_c2();
    unsigned long ticks;

    c2_gate_off();
    ticks = timing_elapsed_ticks(measurement_start_ct, stop_ct);
    return timing_ticks_to_us(ticks);
}

void timing_wait_us(us_t microseconds)
{
    /*
     * Uses Channel 0 (the BIOS system tick — already running, DO NOT
     * reprogram). Safe to call while Channel 2 is in use for measurement.
     * Cap target at ~0xF000 ticks (~55ms) to keep the single-wrap math
     * valid; longer waits can be chained by the caller if needed.
     */
    unsigned long target_ticks;
    unsigned int start, now;
    unsigned long elapsed;

    if (microseconds == 0) return;

    /* target_ticks = microseconds * 1193 / 1000, rounded */
    target_ticks = (microseconds * 1193UL + 500UL) / 1000UL;
    if (target_ticks >= 0xF000UL) target_ticks = 0xF000UL;
    if (target_ticks == 0) target_ticks = 1;

    start = pit_read_c0();
    do {
        now = pit_read_c0();
        elapsed = timing_elapsed_ticks(start, now);
    } while (elapsed < target_ticks);
}

int timing_emulator_hint(void)
{
    return emulator_flag;
}

#endif  /* CERBERUS_HOST_TEST */
