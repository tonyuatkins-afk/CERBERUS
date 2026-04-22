/*
 * PIT Metronome visual — v0.6.0 T6.
 *
 * Fires after timing_self_check. A dot bounces across a text-mode row
 * once per PIT Channel-0 tick (18.2 Hz, BIOS INT 8 update of the count
 * at 0040:006C). Every tick the PC speaker clicks briefly — port 61h
 * bits 0+1 toggled up for ~1 ms, then off. A steady rhythm both
 * visually and audibly confirms the timer is ticking at the expected
 * rate; a TSR stealing ticks produces visible stutter and audible gaps.
 *
 * Text mode on every adapter. No FPU needed, no sound hardware needed
 * beyond the universal PC speaker.
 *
 * Duration: ~75 ticks (~4 seconds) — long enough to establish rhythm
 * or expose stutter.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include "timing.h"
#include "../core/display.h"
#include "tui_util.h"
#include "../core/journey.h"

#define MT_COLS 80
#define MT_ROWS 25







/* PC speaker via port 61h. Enable the speaker (bit 1 = speaker data,
 * bit 0 = gate to PIT C2). With PIT C2 programmed to a square wave,
 * asserting both bits produces a tone. For a brief click we just
 * toggle the speaker data bit on, wait ~1 ms, toggle off. No PIT C2
 * programming needed — the existing PIT C2 value (whatever OS left
 * there) will chop the level into a tick-shaped transient. */
static void mt_click(void)
{
    unsigned char port61 = (unsigned char)inp(0x61);
    outp(0x61, (int)(port61 | 0x02));   /* speaker on */
    {
        /* Short on-time, portable: read port 0x80 ~200 times for
         * roughly 200 us on ISA. Enough for a perceptible click. */
        int i;
        for (i = 0; i < 200; i++) (void)inp(0x80);
    }
    outp(0x61, (int)port61);            /* restore original state */
}

void timing_metronome_visual(const opts_t *o)
{
    unsigned long start;
    unsigned long last_tick;
    unsigned long elapsed;
    unsigned char dot_attr, bg_attr, title_attr;
    int dot_col;
    int direction = 1;
    int tick_count = 0;
    const int DURATION_TICKS = 75;    /* ~4 s */
    const int ROW_BOUNCE = 12;
    const int LEFT_EDGE = 4;
    const int RIGHT_EDGE = MT_COLS - 5;

    if (journey_should_skip(o)) return;

    if (journey_title_card(o, HEAD_CENTER,
                           "SYSTEM TIMER",
                           "Your PC's heartbeat. The Programmable "
                           "Interval Timer should tick at a steady "
                           "18.2 Hz. Listen for stutters.") == 1) return;

    if (tui_is_mono()) {
        dot_attr = ATTR_BOLD; bg_attr = ATTR_NORMAL; title_attr = ATTR_BOLD;
    } else {
        dot_attr = ATTR_YELLOW; bg_attr = ATTR_NORMAL; title_attr = ATTR_BOLD;
    }

    tui_fill(0, MT_ROWS - 1, ' ', ATTR_NORMAL);
    tui_puts(6, (MT_COLS - 27) / 2, "PIT Metronome: 18.2 Hz beat", title_attr);

    /* Track: draw horizontal line markers at LEFT_EDGE and RIGHT_EDGE */
    tui_putc(ROW_BOUNCE, LEFT_EDGE - 2,  '|', bg_attr);
    tui_putc(ROW_BOUNCE, RIGHT_EDGE + 2, '|', bg_attr);

    dot_col = LEFT_EDGE;
    start = tui_ticks();
    last_tick = start;
    /* Draw initial dot */
    tui_putc(ROW_BOUNCE, dot_col, 0x07, dot_attr);   /* CP437 bullet */
    mt_click();

    while (tick_count < DURATION_TICKS) {
        unsigned long now = tui_ticks();
        if (now != last_tick) {
            /* One or more ticks elapsed. Advance the dot per elapsed tick. */
            unsigned long n = now - last_tick;
            last_tick = now;
            while (n > 0) {
                /* Erase current dot position */
                tui_putc(ROW_BOUNCE, dot_col, '-', bg_attr);
                dot_col += direction;
                if (dot_col >= RIGHT_EDGE) { dot_col = RIGHT_EDGE; direction = -1; }
                if (dot_col <= LEFT_EDGE)  { dot_col = LEFT_EDGE;  direction =  1; }
                tui_putc(ROW_BOUNCE, dot_col, 0x07, dot_attr);
                mt_click();
                tick_count++;
                n--;
                if (tick_count >= DURATION_TICKS) break;
            }
        }
        /* Skip check */
        {
            int s = journey_poll_skip();
            if (s) break;
        }
    }
    elapsed = tui_ticks() - start;
    (void)elapsed;

    journey_result_flash(o, "Timer: PIT ticking at 18.2 Hz");
}
