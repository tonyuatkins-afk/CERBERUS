/*
 * Audio Hardware Scale — v0.6.0 T7.
 *
 * Plays an 8-note C major scale through the PC speaker (PIT Channel 2
 * square wave) with a text-mode visual frequency-bar accompaniment.
 * Universal hardware: every PC has a speaker, so this always fires
 * (unless /NOUI, /QUICK, or skip-all).
 *
 * Scope note: v0.6.0 ships the PC-speaker path only. OPL2 FM synthesis
 * and SB16 PCM DMA playback were in Tony's original brief as preferred
 * paths on AdLib/SB-equipped machines; those land in a v0.6.1 follow-up.
 * For now the PC speaker is both the primary and the fallback — clean
 * one-layer implementation.
 *
 * Each note: PIT C2 programmed for the note's frequency, speaker gate
 * opened for ~250 ms, then silenced. Visual: a vertical bar rises as
 * each note plays, height proportional to (index / 8).
 *
 * Confirms end-to-end audio: if you hear the ascending scale, your
 * speaker and PIT C2 both work. Silence on PC-speaker hardware means
 * something in the port 61h path is broken.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include "audio_scale.h"
#include "display.h"
#include "journey.h"

#define AS_COLS 80
#define AS_ROWS 25

/* PIT Channel 2 inputs at 1.193182 MHz. Divisor = 1193180 / freq. */
static const unsigned int note_freq[8] = {
    262,   /* C4 */
    294,   /* D4 */
    330,   /* E4 */
    349,   /* F4 */
    392,   /* G4 */
    440,   /* A4 */
    494,   /* B4 */
    523    /* C5 */
};

static const char *const note_label[8] = {
    "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"
};

static unsigned char __far *as_vram(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

static int as_is_mono(void)
{
    adapter_t a = display_adapter();
    return (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
            a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO);
}

static void as_putc(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v = as_vram();
    unsigned int off = (unsigned int)((row * AS_COLS + col) * 2);
    v[off] = ch; v[off + 1] = attr;
}

static void as_puts(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < AS_COLS) { as_putc(row, col++, (unsigned char)*s++, attr); }
}

static void as_fill(int r0, int r1, unsigned char ch, unsigned char attr)
{
    int r, c;
    for (r = r0; r <= r1; r++)
        for (c = 0; c < AS_COLS; c++) as_putc(r, c, ch, attr);
}

static unsigned long as_ticks(void)
{
    unsigned int __far *low  = (unsigned int __far *)MK_FP(0x0040, 0x006C);
    unsigned int __far *high = (unsigned int __far *)MK_FP(0x0040, 0x006E);
    unsigned int h1, h2, l;
    do { h1 = *high; l = *low; h2 = *high; } while (h1 != h2);
    return ((unsigned long)h1 << 16) | l;
}

/* Program PIT C2 for the given frequency and open the speaker gate. */
static void as_speaker_on(unsigned int hz)
{
    unsigned long divisor;
    unsigned char port61;
    if (hz == 0) return;
    divisor = 1193180UL / (unsigned long)hz;
    /* Mode 3 (square wave), channel 2, access both bytes LSB then MSB. */
    outp(0x43, 0xB6);
    outp(0x42, (int)(divisor & 0xFF));
    outp(0x42, (int)((divisor >> 8) & 0xFF));
    port61 = (unsigned char)inp(0x61);
    outp(0x61, (int)(port61 | 0x03));   /* gate PIT C2 + speaker data */
}

static void as_speaker_off(void)
{
    unsigned char port61 = (unsigned char)inp(0x61);
    outp(0x61, (int)(port61 & 0xFC));   /* clear both bits */
}

/* Render a vertical frequency bar at col centered in an 8-column
 * layout. bar_top is the topmost row of the bar (lower value = taller
 * bar); height is the number of block cells. */
static void as_draw_bar(int col, int bar_top, int height, unsigned char attr)
{
    int r;
    for (r = 0; r < height; r++) {
        as_putc(bar_top + r, col,     0xDB, attr);
        as_putc(bar_top + r, col + 1, 0xDB, attr);
    }
}

void audio_scale_visual(const opts_t *o)
{
    unsigned char label_attr, bar_attr, title_attr;
    int i;
    const int BAR_BASE = 18;      /* bottommost row of bars */
    const int BAR_MAX_H = 10;     /* tallest bar = 10 cells */
    const int FIRST_COL = 16;     /* leftmost bar column */

    if (journey_should_skip(o)) return;

    if (journey_title_card(o, HEAD_RIGHT,
                           "AUDIO HARDWARE",
                           "Playing a test scale through the PC speaker. "
                           "If you hear 8 ascending notes, your audio "
                           "path works end-to-end.") == 1) return;

    if (as_is_mono()) {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL; bar_attr = ATTR_BOLD;
    } else {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL; bar_attr = ATTR_YELLOW;
    }

    as_fill(0, AS_ROWS - 1, ' ', ATTR_NORMAL);
    as_puts(3, (AS_COLS - 26) / 2,
            "Audio Scale — PC Speaker", title_attr);

    /* Pre-draw labels */
    for (i = 0; i < 8; i++) {
        as_puts(BAR_BASE + 2, FIRST_COL + i * 7, note_label[i], label_attr);
    }

    for (i = 0; i < 8; i++) {
        int height = 2 + (i * (BAR_MAX_H - 2)) / 7;   /* 2 to MAX_H */
        int bar_top = BAR_BASE - height + 1;
        /* Play + show simultaneously */
        as_draw_bar(FIRST_COL + i * 7, bar_top, height, bar_attr);
        as_speaker_on(note_freq[i]);
        /* Hold ~250 ms = 5 BIOS ticks */
        {
            unsigned long end = as_ticks() + 5UL;
            while (as_ticks() < end) {
                if (journey_poll_skip()) {
                    as_speaker_off();
                    return;
                }
            }
        }
        as_speaker_off();
    }

    /* Leave the final state on screen briefly, then any-key to continue */
    {
        unsigned long end = as_ticks() + 18UL;   /* ~1 s */
        while (as_ticks() < end) {
            if (kbhit()) {
                union REGS r;
                r.h.ah = 0x00;
                int86(0x16, &r, &r);
                break;
            }
        }
    }
}
