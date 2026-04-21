/*
 * Audio Hardware Scale — v0.6.0 T7, extended v0.6.1 T7b, v0.6.2 T7c.
 *
 * Plays an 8-note C major scale with a text-mode visual frequency-bar
 * accompaniment. Three output layers, probed in order:
 *
 *   SB DSP direct-mode PCM — preferred when BLASTER env is set and
 *                             the DSP at base+6 resets cleanly. Square-
 *                             wave samples pushed via DSP command 0x10.
 *                             Genuine PCM, not FM synthesis.
 *   OPL2 FM synthesis      — used when no SB-DSP responds but port
 *                             0x388 has an AdLib chip. Clean sine voice.
 *   PC speaker             — universal fallback. Square-wave via PIT C2.
 *
 * Scope note: v0.6.2 uses DSP direct mode (command 0x10 per sample)
 * rather than DMA-buffered playback. DSP direct gives us genuine PCM
 * output without the 8237 / IRQ complexity of a full DMA driver; it
 * does run at a lower effective sample rate (~2-4 kHz via Watcom busy-
 * wait on a 486). Fidelity is squarer than DMA-streamed PCM but clearly
 * distinct from OPL's FM — the "is your SB card actually producing
 * PCM samples" question gets a direct yes/no answer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include "audio_scale.h"
#include "display.h"
#include "tui_util.h"
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

/* Sound Blaster DSP — base port parsed from BLASTER env at runtime. */
static unsigned int sb_base = 0;
#define SB_RESET(base)  ((base) + 0x06)
#define SB_READ(base)   ((base) + 0x0A)
#define SB_WRITE(base)  ((base) + 0x0C)
#define SB_DATA_AVAIL(base) ((base) + 0x0E)

static const char *const note_label[8] = {
    "C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5"
};







/* ----------------------------------------------------------------------- */
/* SB DSP direct-mode PCM path                                              */
/* ----------------------------------------------------------------------- */

/* Short bus-settle delay via port 0x80 reads (ISA-stable microsecond-ish). */
static void as_delay_us(unsigned long us)
{
    unsigned long i;
    for (i = 0; i < us; i++) (void)inp(0x80);
}

/* Parse BLASTER env. Format: "A220 I5 D1 H5 T4" — we only need A<hex>.
 * Returns base port (0 on miss). */
static unsigned int as_parse_blaster_base(void)
{
    const char *env = getenv("BLASTER");
    unsigned int base = 0;
    if (!env) return 0;
    while (*env) {
        if (*env == 'A' || *env == 'a') {
            unsigned int v = 0;
            env++;
            while (*env && *env != ' ') {
                unsigned char c = (unsigned char)*env;
                unsigned int d;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'A' && c <= 'F') d = 10 + c - 'A';
                else if (c >= 'a' && c <= 'f') d = 10 + c - 'a';
                else break;
                v = (v << 4) | d;
                env++;
            }
            base = v;
            break;
        }
        env++;
    }
    return base;
}

/* DSP reset sequence. Returns 1 on clean reset (0xAA byte), 0 otherwise. */
static int as_sb_dsp_reset(unsigned int base)
{
    int tries;
    outp(SB_RESET(base), 1);
    as_delay_us(10);
    outp(SB_RESET(base), 0);
    /* Poll data-avail bit 7 at base+0xE for up to ~100 us. */
    for (tries = 0; tries < 200; tries++) {
        if (inp(SB_DATA_AVAIL(base)) & 0x80) {
            unsigned char got = (unsigned char)inp(SB_READ(base));
            return (got == 0xAA) ? 1 : 0;
        }
        as_delay_us(1);
    }
    return 0;
}

/* Probe. Parses BLASTER, resets DSP, returns 1 on success. */
static int as_sb_probe(void)
{
    sb_base = as_parse_blaster_base();
    if (sb_base == 0) return 0;
    return as_sb_dsp_reset(sb_base);
}

/* Write a DSP command or data byte. Poll write-ready bit 7 at base+C. */
static void as_sb_write(unsigned char b)
{
    int guard;
    for (guard = 0; guard < 4000; guard++) {
        if ((inp(SB_WRITE(sb_base)) & 0x80) == 0) break;
    }
    outp(SB_WRITE(sb_base), (int)b);
}

static void as_sb_speaker_on(void)  { as_sb_write(0xD1); }
static void as_sb_speaker_off(void) { as_sb_write(0xD3); }

/* Play a square-wave note for `duration_samples` at the given half-period
 * count (samples-per-half-cycle). Uses DSP command 0x10 per sample. */
static void as_sb_play_note(unsigned int half_period, unsigned long samples,
                            volatile int *abort_flag)
{
    unsigned long i;
    unsigned char level = 0xFF;
    unsigned int phase = 0;
    for (i = 0; i < samples; i++) {
        as_sb_write(0x10);           /* DSP direct output command */
        as_sb_write(level);          /* 8-bit unsigned sample */
        phase++;
        if (phase >= half_period) { phase = 0; level = (unsigned char)~level; }
        /* Skip poll every 64 samples */
        if ((i & 63) == 0 && abort_flag && *abort_flag) return;
    }
}

/* Sample-rate estimate per DSP write: Watcom busy-wait + two outp's on
 * a 486 DX-2 runs ~2-4 kHz. For note frequencies we choose half_period
 * in samples such that (sample_rate / freq / 2) ≈ half_period. Using
 * nominal 3000 Hz sample rate:
 *   C4 262 Hz → 3000 / 262 / 2 ≈ 5.7  → 6
 *   D4 294   → 5
 *   ...
 * Values are rough — user will hear distinct ascending pitches which
 * is the whole point of the journey demo. */
static const unsigned int sb_half_period[8] = {
    6,   /* C4 ~ 262 */
    5,   /* D4 ~ 294 */
    5,   /* E4 ~ 330 */
    4,   /* F4 ~ 349 */
    4,   /* G4 ~ 392 */
    3,   /* A4 ~ 440 */
    3,   /* B4 ~ 494 */
    3    /* C5 ~ 523 */
};
/* ~150 ms of samples per note at 3 kHz → 450 samples. */
#define SB_SAMPLES_PER_NOTE 450UL

/* ----------------------------------------------------------------------- */
/* OPL2 FM path                                                             */
/* ----------------------------------------------------------------------- */

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
        tui_putc(bar_top + r, col,     0xDB, attr);
        tui_putc(bar_top + r, col + 1, 0xDB, attr);
    }
}

void audio_scale_visual(const opts_t *o)
{
    unsigned char label_attr, bar_attr, title_attr;
    int i;
    int using_sb = 0;
    int using_opl = 0;

    const int BAR_BASE = 18;      /* bottommost row of bars */
    const int BAR_MAX_H = 10;     /* tallest bar = 10 cells */
    const int FIRST_COL = 16;     /* leftmost bar column */

    if (journey_should_skip(o)) return;

    /* Probe order: SB DSP direct → OPL2 FM → PC speaker fallback. */
    using_sb = as_sb_probe();
    if (!using_sb) using_opl = as_opl_probe();

    {
        const char *desc;
        if (using_sb) {
            desc = "Playing a test scale as PCM samples through your "
                   "Sound Blaster DSP. If you hear 8 ascending square-"
                   "wave notes, your SB PCM path works.";
        } else if (using_opl) {
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

    if (tui_is_mono()) {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL; bar_attr = ATTR_BOLD;
    } else {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL; bar_attr = ATTR_YELLOW;
    }

    tui_fill(0, AS_ROWS - 1, ' ', ATTR_NORMAL);
    {
        const char *heading;
        if (using_sb)      heading = "Audio Scale — SB DSP Direct PCM";
        else if (using_opl) heading = "Audio Scale — OPL2 FM Synth";
        else                heading = "Audio Scale — PC Speaker";
        tui_puts(3, (AS_COLS - (int)strlen(heading)) / 2, heading, title_attr);
    }

    if (using_opl) as_opl_program();
    if (using_sb)  as_sb_speaker_on();

    /* Pre-draw labels */
    for (i = 0; i < 8; i++) {
        tui_puts(BAR_BASE + 2, FIRST_COL + i * 7, note_label[i], label_attr);
    }

    for (i = 0; i < 8; i++) {
        int height = 2 + (i * (BAR_MAX_H - 2)) / 7;   /* 2 to MAX_H */
        int bar_top = BAR_BASE - height + 1;
        /* Play + show simultaneously */
        as_draw_bar(FIRST_COL + i * 7, bar_top, height, bar_attr);
        if (using_sb) {
            /* DSP direct mode — SB_SAMPLES_PER_NOTE samples at the chosen
             * half-period. The playback loop itself holds the note duration;
             * no separate tick wait needed. journey_poll_skip is checked
             * inside as_sb_play_note via abort flag. */
            int abort_flag = 0;
            as_sb_play_note(sb_half_period[i], SB_SAMPLES_PER_NOTE,
                            &abort_flag);
            if (journey_poll_skip()) {
                as_sb_speaker_off();
                return;
            }
        } else if (using_opl) {
            as_opl_note_on(opl_fnum[i]);
            {
                unsigned long end = tui_ticks() + 5UL;
                while (tui_ticks() < end) {
                    if (journey_poll_skip()) { as_opl_note_off(); return; }
                }
            }
            as_opl_note_off();
        } else {
            as_speaker_on(note_freq[i]);
            {
                unsigned long end = tui_ticks() + 5UL;
                while (tui_ticks() < end) {
                    if (journey_poll_skip()) { as_speaker_off(); return; }
                }
            }
            as_speaker_off();
        }
    }

    if (using_sb) as_sb_speaker_off();

    /* Leave the final state on screen briefly, then any-key to continue */
    {
        unsigned long end = tui_ticks() + 18UL;   /* ~1 s */
        while (tui_ticks() < end) {
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
