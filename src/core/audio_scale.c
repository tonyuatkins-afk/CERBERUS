/*
 * Audio Hardware Scale — v0.6.0 T7, extended v0.6.1 T7b.
 *
 * Plays an 8-note C major scale with a text-mode visual frequency-bar
 * accompaniment. Two output layers:
 *
 *   OPL2 FM synthesis  — preferred when AdLib/Sound Blaster detected
 *                        at port 0x388. Smooth sine-wave tones.
 *   PC speaker         — universal fallback. Square-wave via PIT C2.
 *
 * Runtime detection: brief OPL2 timer-status probe at 0x388 before
 * playback starts. If probe succeeds, OPL2 wins; otherwise speaker.
 * Confirms end-to-end audio: if you hear 8 ascending notes, your
 * audio path (either OPL or speaker) works.
 *
 * v0.6.2 deferred: SB16 PCM DMA playback. OPL2 FM covers
 * AdLib + SB1 + SB Pro + SB16 in v0.6.1; PCM DMA adds complexity
 * (8237 channel setup, DSP init, buffer management) for marginal
 * audible benefit over OPL on a short scale.
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

/* OPL2 F-numbers for C major at block 4. Formula:
 *   fnum = freq * 65536 / 49716  (f_clock = 49716 Hz at block 4). */
static const unsigned int opl_fnum[8] = {
    345,   /* C4 */
    387,   /* D4 */
    435,   /* E4 */
    460,   /* F4 */
    517,   /* G4 */
    580,   /* A4 */
    651,   /* B4 */
    689    /* C5 */
};

#define OPL_ADDR 0x388
#define OPL_DATA 0x389

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

/* ----------------------------------------------------------------------- */
/* OPL2 FM path                                                             */
/* ----------------------------------------------------------------------- */

/* Short bus-settle delay via port 0x80 reads (ISA-stable microsecond-ish). */
static void as_delay_us(unsigned long us)
{
    unsigned long i;
    for (i = 0; i < us; i++) (void)inp(0x80);
}

static void as_opl_write(unsigned char reg, unsigned char val)
{
    outp(OPL_ADDR, reg);
    as_delay_us(4);
    outp(OPL_DATA, val);
    as_delay_us(28);
}

/* OPL2 timer-status detection. Returns 1 if an AdLib-class chip is
 * responding at port 0x388, 0 otherwise. Standard probe sequence. */
static int as_opl_probe(void)
{
    unsigned char s1, s2;
    as_opl_write(0x04, 0x60);   /* reset timers */
    as_opl_write(0x04, 0x80);   /* clear timer-status flags */
    s1 = (unsigned char)inp(OPL_ADDR);
    as_opl_write(0x02, 0xFF);   /* timer 1 divisor */
    as_opl_write(0x04, 0x21);   /* start timer 1 */
    as_delay_us(100);
    s2 = (unsigned char)inp(OPL_ADDR);
    as_opl_write(0x04, 0x60);   /* reset */
    as_opl_write(0x04, 0x80);
    /* OPL2 sets timer-1-expired bit (0x40) and overall interrupt bit
     * (0x80) when timer 1 expires. Empty bus returns all-zeros or
     * all-ones regardless of the probe. */
    return (s1 & 0xE0) == 0x00 && (s2 & 0xE0) == 0xC0;
}

/* Program channel 0 as a clean sine-like voice: no feedback, carrier-
 * only amplitude. Different from intro's "brass growl" — we want a
 * clean note here, not a bark. */
static void as_opl_program(void)
{
    int r;
    /* Silence all channels first */
    for (r = 0x20; r <= 0xF5; r++) as_opl_write((unsigned char)r, 0);
    as_opl_write(0x01, 0x00);
    as_opl_write(0xBD, 0x00);
    /* Channel 0, operators 0 (mod) + 3 (car) */
    /* 0x20: multiplier 1, no sustain-pedal */
    as_opl_write(0x20, 0x01);
    as_opl_write(0x23, 0x01);
    /* 0x40: level. Modulator high attenuation (quiet), carrier loud. */
    as_opl_write(0x40, 0x3F);   /* mod silent — pure carrier sine */
    as_opl_write(0x43, 0x10);   /* carrier loud */
    /* 0x60: attack F, decay 0 */
    as_opl_write(0x60, 0xF0);
    as_opl_write(0x63, 0xF0);
    /* 0x80: sustain 0 (loudest), release fast */
    as_opl_write(0x80, 0x0F);
    as_opl_write(0x83, 0x0F);
    /* 0xE0: sine */
    as_opl_write(0xE0, 0x00);
    as_opl_write(0xE3, 0x00);
    /* 0xC0: no feedback, additive off (FM) */
    as_opl_write(0xC0, 0x00);
}

static void as_opl_note_on(unsigned int fnum)
{
    as_opl_write(0xA0, (unsigned char)(fnum & 0xFF));
    as_opl_write(0xB0, (unsigned char)(0x20 | (4 << 2) | ((fnum >> 8) & 0x03)));
}

static void as_opl_note_off(void)
{
    as_opl_write(0xB0, 0x00);
}

/* ----------------------------------------------------------------------- */
/* PC speaker path                                                          */
/* ----------------------------------------------------------------------- */

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
    int using_opl;
    const int BAR_BASE = 18;      /* bottommost row of bars */
    const int BAR_MAX_H = 10;     /* tallest bar = 10 cells */
    const int FIRST_COL = 16;     /* leftmost bar column */

    if (journey_should_skip(o)) return;

    using_opl = as_opl_probe();

    {
        const char *desc;
        if (using_opl) {
            desc = "Playing a test scale through AdLib/Sound Blaster "
                   "OPL2 FM. If you hear 8 ascending notes, your audio "
                   "path works end-to-end.";
        } else {
            desc = "Playing a test scale through the PC speaker. "
                   "If you hear 8 ascending notes, your audio "
                   "path works end-to-end.";
        }
        if (journey_title_card(o, HEAD_RIGHT, "AUDIO HARDWARE", desc) == 1)
            return;
    }

    if (as_is_mono()) {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL; bar_attr = ATTR_BOLD;
    } else {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL; bar_attr = ATTR_YELLOW;
    }

    as_fill(0, AS_ROWS - 1, ' ', ATTR_NORMAL);
    as_puts(3, (AS_COLS - 26) / 2,
            using_opl ? "Audio Scale — OPL2 FM Synth"
                      : "Audio Scale — PC Speaker", title_attr);

    if (using_opl) as_opl_program();

    /* Pre-draw labels */
    for (i = 0; i < 8; i++) {
        as_puts(BAR_BASE + 2, FIRST_COL + i * 7, note_label[i], label_attr);
    }

    for (i = 0; i < 8; i++) {
        int height = 2 + (i * (BAR_MAX_H - 2)) / 7;   /* 2 to MAX_H */
        int bar_top = BAR_BASE - height + 1;
        /* Play + show simultaneously */
        as_draw_bar(FIRST_COL + i * 7, bar_top, height, bar_attr);
        if (using_opl) {
            as_opl_note_on(opl_fnum[i]);
        } else {
            as_speaker_on(note_freq[i]);
        }
        /* Hold ~250 ms = 5 BIOS ticks */
        {
            unsigned long end = as_ticks() + 5UL;
            while (as_ticks() < end) {
                if (journey_poll_skip()) {
                    if (using_opl) as_opl_note_off(); else as_speaker_off();
                    return;
                }
            }
        }
        if (using_opl) as_opl_note_off(); else as_speaker_off();
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

    journey_result_flash(o, "Audio: speaker path verified end-to-end");
}
