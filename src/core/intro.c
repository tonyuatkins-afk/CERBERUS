/*
 * CERBERUS boot splash.
 *
 * Renders a full-screen CP437 three-headed-dog emblem with a
 * block-letter CERBERUS wordmark above and a bound-in-chains
 * motif below, framed by a double-line gate. Animation plays
 * once per invocation; keypress or ~1500 ms timeout dismisses.
 *
 * Iconography follows classical sources: three dog heads
 * (Pseudo-Apollodorus, Virgil), serpent tail (Pseudo-
 * Apollodorus), red glowing eyes (Dante), bound in chains
 * (Hercules 12th labor), framed at the threshold of a gate
 * (Roman and Etruscan art). The three eyes illuminate in
 * sequence to echo the tool's three analysis heads.
 *
 * Adapter ladder:
 *   VGA color:    DAC fade in, attribute animation, DAC fade out.
 *   EGA color:    attribute animation only; palette not remapped
 *                 because we share the EGA color space with the
 *                 later detect-pane output.
 *   CGA:          same as EGA color path; CGA text-mode attribute
 *                 byte drives the 16 color slots directly.
 *   MCGA:         VGA DAC path (MCGA has a DAC).
 *   VGA mono /    intensity-bit pulse; block-shading silhouette
 *   EGA mono /    stays legible; no color cycling attempted.
 *   MDA /
 *   Hercules:
 *
 * Timing uses the BIOS tick counter at 0040:006C (updates at
 * ~18.2 Hz, 54.9 ms per tick). All waits are keypress-abortable.
 *
 * Skipped entirely under /NOUI and /NOINTRO. Emulator skip is
 * intentional: synthetic capture runs under DOSBox-X should not
 * burn ~1.5 s on visual flourish. Detection of the emulator
 * state is done by the caller (main.c passes an already-populated
 * opts_t that reflects the runtime environment via the later
 * detect_env path; for the intro we rely only on /NOINTRO and
 * /NOUI because detect_env runs AFTER the splash in the current
 * startup order).
 */

#include <dos.h>
#include <i86.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include "intro.h"
#include "display.h"
#include "head_art.h"
#include "../cerberus.h"

#define COLS 80
#define ROWS 25

/* VGA DAC ports */
#define DAC_READ_INDEX  0x3C7
#define DAC_WRITE_INDEX 0x3C8
#define DAC_DATA        0x3C9

/* Attribute cheat-sheet (CGA-text-mode 16-color scheme):
 *  0x00 black           0x08 dark gray
 *  0x01 blue            0x09 bright blue
 *  0x02 green           0x0A bright green
 *  0x03 cyan            0x0B bright cyan
 *  0x04 red             0x0C bright red
 *  0x05 magenta         0x0D bright magenta
 *  0x06 brown           0x0E yellow
 *  0x07 light gray      0x0F white
 * Attribute byte: (bg << 4) | fg, bg high nibble bits 4-6 (bit 7 is
 * usually the blink bit; we keep it 0).
 */

#define A_BLACK         0x00
#define A_DIM_GRAY      0x08
#define A_GRAY          0x07
#define A_WHITE         0x0F
#define A_DARK_RED      0x04
#define A_RED           0x0C
#define A_YELLOW        0x0E
#define A_BROWN         0x06
#define A_CYAN          0x03
#define A_BRIGHT_CYAN   0x0B
#define A_BLUE          0x01
#define A_BRIGHT_BLUE   0x09

/* ----------------------------------------------------------------------- */
/* VRAM primitives                                                          */
/* ----------------------------------------------------------------------- */

static unsigned char __far *vram_seg(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

static int is_mono(void)
{
    adapter_t a = display_adapter();
    return (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
            a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO);
}

static int has_dac(void)
{
    /* DAC palette is a VGA/MCGA feature. EGA has palette registers
     * but mapped through the attribute controller, reached via a
     * different port; for this intro we only drive the DAC on
     * hardware that has one. */
    adapter_t a = display_adapter();
    return (a == ADAPTER_VGA_COLOR || a == ADAPTER_MCGA);
}

static void put_cell(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v;
    unsigned int off;
    if (row < 0 || row >= ROWS || col < 0 || col >= COLS) return;
    v = vram_seg();
    off = (unsigned int)((row * COLS + col) * 2);
    v[off]     = ch;
    v[off + 1] = attr;
}

static void put_str(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < COLS) {
        put_cell(row, col++, (unsigned char)*s++, attr);
    }
}

static void fill_screen(unsigned char ch, unsigned char attr)
{
    int r, c;
    for (r = 0; r < ROWS; r++)
        for (c = 0; c < COLS; c++)
            put_cell(r, c, ch, attr);
}

/* Repaint every cell's attribute byte, leaving the character data
 * alone. Cheap way to flash the whole frame a different color. */
static void recolor_all(unsigned char attr)
{
    unsigned char __far *v = vram_seg();
    unsigned int off;
    for (off = 1; off < (unsigned int)(ROWS * COLS * 2); off += 2) {
        v[off] = attr;
    }
}

/* ----------------------------------------------------------------------- */
/* Timing and input                                                         */
/* ----------------------------------------------------------------------- */

static unsigned long read_ticks(void)
{
    /* Atomic read. A single 32-bit deref compiles to two 16-bit loads
     * on 8088 — INT 8 can fire between them and increment the high
     * word, corrupting the returned value mid-read. Retry until the
     * high word matches across two reads. Same pattern as timing.c
     * and tui_util.c. v0.7.0-rc2 quality-gate fix. */
    unsigned int __far *hi = (unsigned int __far *)MK_FP(0x0040, 0x006E);
    unsigned int __far *lo = (unsigned int __far *)MK_FP(0x0040, 0x006C);
    unsigned int h1, h2, l;
    do {
        h1 = *hi;
        l  = *lo;
        h2 = *hi;
    } while (h1 != h2);
    return ((unsigned long)h1 << 16) | l;
}

/* Wait up to n ticks (n * ~54.9 ms), early-return 1 if a key was
 * pressed during the wait. Does NOT drain the buffer; caller decides
 * whether to consume the key. */
static int wait_ticks_or_key(unsigned int n)
{
    unsigned long start = read_ticks();
    while (read_ticks() - start < (unsigned long)n) {
        if (kbhit()) return 1;
    }
    return 0;
}

static void drain_keys(void)
{
    while (kbhit()) (void)getch();
}

/* ----------------------------------------------------------------------- */
/* VGA DAC palette fade                                                     */
/* ----------------------------------------------------------------------- */

/* Save the first 16 DAC entries (text-mode 16-color block). Each
 * entry is three 6-bit components (0..63). */
static void dac_save16(unsigned char pal[16][3])
{
    int i, j;
    for (i = 0; i < 16; i++) {
        outp(DAC_READ_INDEX, (unsigned char)i);
        for (j = 0; j < 3; j++) {
            pal[i][j] = (unsigned char)(inp(DAC_DATA) & 0x3F);
        }
    }
}

static void dac_write16(const unsigned char pal[16][3])
{
    int i, j;
    outp(DAC_WRITE_INDEX, 0);
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 3; j++) {
            outp(DAC_DATA, pal[i][j]);
        }
    }
}

/* Scale entry (r,g,b) by num/denom into (out_r, out_g, out_b). */
static void scale_rgb(unsigned char r, unsigned char g, unsigned char b,
                      unsigned int num, unsigned int denom,
                      unsigned char out[3])
{
    out[0] = (unsigned char)((unsigned int)r * num / denom);
    out[1] = (unsigned char)((unsigned int)g * num / denom);
    out[2] = (unsigned char)((unsigned int)b * num / denom);
}

/* Fade the 16 color palette from 0 to 100% of saved over `steps`
 * linear interpolations, waiting ~1 tick between steps (~55 ms).
 * Early-return 1 if a key was pressed during the fade. */
static int dac_fade_in(const unsigned char saved[16][3], int steps)
{
    unsigned char working[16][3];
    int s, i;
    for (s = 1; s <= steps; s++) {
        for (i = 0; i < 16; i++) {
            scale_rgb(saved[i][0], saved[i][1], saved[i][2],
                      (unsigned int)s, (unsigned int)steps,
                      working[i]);
        }
        dac_write16((const unsigned char (*)[3])working);
        if (wait_ticks_or_key(1)) return 1;
    }
    return 0;
}

static int dac_fade_out(const unsigned char saved[16][3], int steps)
{
    unsigned char working[16][3];
    int s, i;
    for (s = steps - 1; s >= 0; s--) {
        for (i = 0; i < 16; i++) {
            scale_rgb(saved[i][0], saved[i][1], saved[i][2],
                      (unsigned int)s, (unsigned int)steps,
                      working[i]);
        }
        dac_write16((const unsigned char (*)[3])working);
        if (wait_ticks_or_key(1)) return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------- */
/* Art data: CERBERUS block-letter wordmark                                 */
/* ----------------------------------------------------------------------- */

/* Each letter is 5 rows tall, 5 cols wide. Strings use '#' as the
 * "filled" marker; render will translate to the block character of
 * choice. Letters separated by one column of blank. */
static const char *const letter_C[5] = {
    "#####",
    "#    ",
    "#    ",
    "#    ",
    "#####"
};
static const char *const letter_E[5] = {
    "#####",
    "#    ",
    "###  ",
    "#    ",
    "#####"
};
static const char *const letter_R[5] = {
    "#### ",
    "#   #",
    "#### ",
    "#  # ",
    "#   #"
};
static const char *const letter_B[5] = {
    "#### ",
    "#   #",
    "#### ",
    "#   #",
    "#### "
};
static const char *const letter_U[5] = {
    "#   #",
    "#   #",
    "#   #",
    "#   #",
    "#####"
};
static const char *const letter_S[5] = {
    "#####",
    "#    ",
    "#####",
    "    #",
    "#####"
};

static const char *const *const wordmark[8] = {
    letter_C, letter_E, letter_R, letter_B, letter_E, letter_R, letter_U, letter_S
};

/* CP437 glyphs used for the dog. Hex codes call out non-ASCII. */
#define G_BLOCK_FULL  0xDB  /* full block */
#define G_BLOCK_DK    0xB2  /* dark shade */
#define G_BLOCK_MD    0xB1  /* medium shade */
#define G_BLOCK_LT    0xB0  /* light shade */
#define G_HALF_UP     0xDF  /* upper half block */
#define G_HALF_DN     0xDC  /* lower half block */
#define G_HALF_LT     0xDD  /* left half block */
#define G_HALF_RT     0xDE  /* right half block */
#define G_EYE_OPEN    0x09  /* hollow bullet */
#define G_EYE_GLOW    0x07  /* bullet */
#define G_BOX_TL      0xC9  /* double-line top-left */
#define G_BOX_TR      0xBB  /* double-line top-right */
#define G_BOX_BL      0xC8  /* double-line bottom-left */
#define G_BOX_BR      0xBC  /* double-line bottom-right */
#define G_BOX_H       0xCD  /* double horizontal */
#define G_BOX_V       0xBA  /* double vertical */
#define G_CHAIN       0xF0  /* identical-to (chain link stand-in) */
#define G_SER_UP      0x16  /* inverted triangle-ish scale */
#define G_SER_DN      0x17  /* scale */
#define G_APPROX      0xF7  /* approximately (serpent body curl) */
#define G_FANG        0x1F  /* down-pointing triangle (canine fang) */
#define G_SPINE       0x1E  /* up-pointing triangle (snake-mane spine) */

/* ----------------------------------------------------------------------- */
/* Wordmark renderer                                                        */
/* ----------------------------------------------------------------------- */

static int wordmark_width(void)
{
    /* 8 letters at 5 cols each, 7 gaps of 1 col = 47 cols */
    return 8 * 5 + 7;
}

static void draw_wordmark(int top_row, unsigned char fill_glyph,
                          unsigned char attr)
{
    int left = (COLS - wordmark_width()) / 2;
    int li, r, c;
    for (li = 0; li < 8; li++) {
        int lx = left + li * 6;  /* 5 cols + 1 gap */
        const char *const *rows = wordmark[li];
        for (r = 0; r < 5; r++) {
            const char *row = rows[r];
            for (c = 0; c < 5; c++) {
                if (row[c] == '#') {
                    put_cell(top_row + r, lx + c, fill_glyph, attr);
                }
            }
        }
    }
}

/* ----------------------------------------------------------------------- */
/* Three-headed dog emblem                                                  */
/* ----------------------------------------------------------------------- */

/* The dog occupies rows 10-16 (inclusive), centered horizontally.
 * Three heads each 9 cols wide, spaced 3 cols apart, total width
 * 9+3+9+3+9 = 33 cols. Left edge at (80-33)/2 = 23, heads at
 * columns 23-31, 35-43, 47-55. */

#define DOG_TOP     10
#define DOG_LEFT_H1 23
#define DOG_LEFT_H2 35
#define DOG_LEFT_H3 47

/* Eye positions (row, col) per head. v0.6.0 T0c: heads are now
 * directional — left head (H1) has 1 eye, center head (H2) has 2,
 * right head (H3) has 1 — so four eye cells total. eye_head_map
 * lets apply_eye_phase translate each cell back to its head index
 * for the attr cascade. */
typedef struct {
    int row;
    int col;
} cell_t;

#define EYE_CELL_COUNT 4

static const cell_t eye_cells[EYE_CELL_COUNT] = {
    { DOG_TOP + 2, DOG_LEFT_H1 + 6 },   /* H1 LEFT — single eye right side */
    { DOG_TOP + 2, DOG_LEFT_H2 + 3 },   /* H2 CENTER — left eye */
    { DOG_TOP + 2, DOG_LEFT_H2 + 5 },   /* H2 CENTER — right eye */
    { DOG_TOP + 2, DOG_LEFT_H3 + 2 }    /* H3 RIGHT — single eye left side */
};

static const int eye_head_map[EYE_CELL_COUNT] = { 0, 1, 1, 2 };

/* Draw one of the three directional heads at (DOG_TOP, left) using
 * the shared head_art tables. variant picks LEFT/CENTER/RIGHT. The
 * body glyph on the v0.5.0 path was a parameter; here it's folded
 * into the art data. eye_attr drives the initial eye color (later
 * cycled by the animation); fang_attr stays constant (bright). */
static void draw_head(int left, head_dir_t variant,
                      unsigned char body_attr, unsigned char eye_attr,
                      unsigned char fang_attr)
{
    int r, c;
    for (r = 0; r < HEAD_ROWS; r++) {
        for (c = 0; c < HEAD_COLS; c++) {
            head_cell_t cell = head_art[variant][r][c];
            unsigned char attr;
            switch (cell.kind) {
                case HEAD_CELL_EYE:  attr = eye_attr;  break;
                case HEAD_CELL_FANG: attr = fang_attr; break;
                default:             attr = body_attr; break;
            }
            put_cell(DOG_TOP + r, left + c, cell.glyph, attr);
        }
    }
}

/* Serpent-mane spines above each head. Pseudo-Apollodorus is explicit
 * about snakes running along Cerberus's back; the closest text-mode read
 * is a row of small up-pointing triangles above each skull silhouette. */
static void draw_spines(unsigned char attr)
{
    static const int left_col[3] = {
        DOG_LEFT_H1, DOG_LEFT_H2, DOG_LEFT_H3
    };
    int head;
    for (head = 0; head < 3; head++) {
        int lx = left_col[head];
        /* Alternate spine heights via a simple zigzag so it reads as a
         * writhing mane rather than a row of tally marks. */
        put_cell(DOG_TOP - 1, lx + 1, G_SPINE, attr);
        put_cell(DOG_TOP - 1, lx + 3, G_SPINE, attr);
        put_cell(DOG_TOP - 1, lx + 5, G_SPINE, attr);
        put_cell(DOG_TOP - 1, lx + 7, G_SPINE, attr);
    }
}

/* v0.6.0 T0b: draw_body_and_chain() removed. The static title's chain
 * bar + body + serpent-tail composition was eating the visual space
 * between heads and tagline and reading as a progress widget. Chain
 * rattle + shatter still run in the animation (rattle_chain writes to
 * row 16 directly). */

/* ----------------------------------------------------------------------- */
/* Full static frame (pre-animation)                                        */
/* ----------------------------------------------------------------------- */

static void draw_gate_border(unsigned char attr)
{
    int i;
    /* Top border on row 1 */
    put_cell(1, 1,  G_BOX_TL, attr);
    for (i = 2; i < COLS - 1; i++) put_cell(1, i, G_BOX_H, attr);
    put_cell(1, COLS - 2, G_BOX_TR, attr);

    /* Side borders */
    for (i = 2; i < ROWS - 2; i++) {
        put_cell(i, 1,        G_BOX_V, attr);
        put_cell(i, COLS - 2, G_BOX_V, attr);
    }

    /* Bottom border on row ROWS-2 */
    put_cell(ROWS - 2, 1,        G_BOX_BL, attr);
    for (i = 2; i < COLS - 1; i++) put_cell(ROWS - 2, i, G_BOX_H, attr);
    put_cell(ROWS - 2, COLS - 2, G_BOX_BR, attr);
}

/* Center a string on a row. */
static int center_col(const char *s)
{
    int len = (int)strlen(s);
    return (COLS - len) / 2;
}

static void draw_centered(int row, const char *s, unsigned char attr)
{
    put_str(row, center_col(s), s, attr);
}

/* Render the full static frame. Eyes start at eye_attr (black or
 * off) so the later animation can pulse them in. */
static void draw_frame(unsigned char wordmark_attr,
                       unsigned char body_attr,
                       unsigned char chain_attr,
                       unsigned char serpent_attr,
                       unsigned char text_attr,
                       unsigned char gate_attr,
                       unsigned char initial_eye_attr)
{
    fill_screen(' ', A_BLACK);
    draw_gate_border(gate_attr);

    /* Wordmark, rows 3..7 */
    draw_wordmark(3, G_BLOCK_FULL, wordmark_attr);

    /* Three heads at DOG_TOP (rows 10-13) */
    /* Serpent-mane spines above each head (row 9) */
    draw_spines(serpent_attr);

    {
        /* v0.6.0 T0c: directional heads — left-facing, center (dominant,
         * forward), right-facing. Fangs stay bright white/accent even at
         * rest; eyes begin dim (initial_eye_attr) and pulse through the
         * animation. */
        unsigned char fang_attr = (unsigned char)(is_mono() ? A_WHITE : A_WHITE);
        draw_head(DOG_LEFT_H1, HEAD_LEFT,   body_attr, initial_eye_attr, fang_attr);
        draw_head(DOG_LEFT_H2, HEAD_CENTER, body_attr, initial_eye_attr, fang_attr);
        draw_head(DOG_LEFT_H3, HEAD_RIGHT,  body_attr, initial_eye_attr, fang_attr);
    }

    /* v0.6.0 T0b: clear rows 15-18 of the old chain/body/serpent-tail
     * area on both initial draw and post-shatter redraw. The animation's
     * chain rattle + shatter still writes directly to row 16 and runs
     * its course; when draw_frame is called a second time after the
     * shatter, we want those stale remnants gone.
     *
     * Row 14 (DOG_TOP+4) is the shared-neck row added below — clear 15
     * and down so we don't erase the neck. */
    {
        int clr;
        int clr_row;
        for (clr_row = DOG_TOP + 5; clr_row <= DOG_TOP + 8; clr_row++) {
            for (clr = DOG_LEFT_H1; clr <= DOG_LEFT_H3 + 8 + 5; clr++) {
                put_cell(clr_row, clr, ' ', body_attr);
            }
        }
        (void)chain_attr;
        (void)serpent_attr;
    }

    /* v0.6.0 T0c: shared-neck row at DOG_TOP+4. A single row of
     * upper-half blocks spans the three heads' combined width, selling
     * "one creature with three heads" rather than "three separate dogs."
     * Rendered AFTER the chain-area clear so it survives both the
     * initial draw and the post-shatter redraw. */
    {
        int neck_row = DOG_TOP + 4;
        int head_lefts[3];
        int h;
        head_lefts[0] = DOG_LEFT_H1;
        head_lefts[1] = DOG_LEFT_H2;
        head_lefts[2] = DOG_LEFT_H3;
        for (h = 0; h < 3; h++) {
            int c;
            for (c = 0; c < HEAD_COLS; c++) {
                put_cell(neck_row, head_lefts[h] + c, 0xDF, body_attr);
            }
        }
    }

    /* Subtitle below the emblem, row 19. Shifted up one row from the
     * original layout to free row 22 for the hellfire ember flicker. */
    draw_centered(19, "Tough Times Demand Tough Tests", text_attr);

    /* Version line row 20 */
    draw_centered(20, "CERBERUS " CERBERUS_VERSION
                      "  .  (c) 2026 Tony Atkins  .  MIT",
                  text_attr);

    /* Press-any-key hint row 21, dim */
    draw_centered(21, "press any key to continue",
                  (unsigned char)(is_mono() ? A_GRAY : A_DIM_GRAY));
    /* Row 22 intentionally left blank; hellfire_tick paints it during
     * the sustain phase. */
}

/* ----------------------------------------------------------------------- */
/* Eye-pulse animation                                                      */
/* ----------------------------------------------------------------------- */

static unsigned char eye_attr_for(int head_index, int phase)
{
    /* head_index: 0 (left), 1 (center), 2 (right).
     * phase: 0..N. Each head lights 2 phases after the previous.
     * On mono hardware, substitute intensity bit ladder. */
    int heat;  /* 0 = off, 1 = dim, 2 = bright, 3 = hot */
    int mine_starts_at = head_index * 2;
    if (phase < mine_starts_at)     heat = 0;
    else if (phase < mine_starts_at + 1) heat = 1;
    else if (phase < mine_starts_at + 2) heat = 2;
    else                                   heat = 3;

    if (is_mono()) {
        /* dim = 0x07 (normal), bright = 0x0F (intense), hot = 0x8F+ reserve
         * (blink on would invert; simulate hotter via blink bit on color adapters
         * but skip on mono so we don't blink). Keep simple. */
        switch (heat) {
            case 0: return A_DIM_GRAY;
            case 1: return A_GRAY;
            case 2: return A_WHITE;
            default: return A_WHITE;  /* no hotter state on mono */
        }
    } else {
        switch (heat) {
            case 0: return (unsigned char)((A_DARK_RED) & 0x0F);  /* dim red */
            case 1: return A_DARK_RED;
            case 2: return A_RED;
            default: return A_YELLOW;
        }
    }
}

static void apply_eye_phase(int phase)
{
    int i;
    unsigned char attrs[3];
    attrs[0] = eye_attr_for(0, phase);
    attrs[1] = eye_attr_for(1, phase);
    attrs[2] = eye_attr_for(2, phase);
    /* v0.6.0 T0c: four eyes total (left head: 1, center: 2, right: 1).
     * eye_head_map[i] selects the head-index attr for each eye cell.
     * Cascade timing (phase budget in eye_attr_for) unchanged; visible
     * eye count drops from 6 to 4 but the tempo and heating progression
     * of each head still happens on the same phase schedule. */
    for (i = 0; i < EYE_CELL_COUNT; i++) {
        put_cell(eye_cells[i].row, eye_cells[i].col,
                 G_EYE_GLOW, attrs[eye_head_map[i]]);
    }
}

/* ----------------------------------------------------------------------- */
/* OPL2 sound. Three escalating barks for the eye cascade plus a sustained  */
/* A-minor chord while the heads hold their hot gaze.                       */
/*                                                                          */
/* Writes are unconditional; on boxes with no OPL2 the port at 0x388 sinks  */
/* the data with no audible or state-visible effect. The OPL2 state we      */
/* leave behind is overwritten by detect_audio when it runs later.          */
/* ----------------------------------------------------------------------- */

#define OPL_ADDR 0x388
#define OPL_DATA 0x389

/* inp(0x80) reads the diagnostic port, which on ISA produces a ~1 us bus
 * cycle independent of CPU speed. Use as the portable short-delay knob.
 * Takes unsigned long because the heartbeat and flash functions want
 * values >65535 (~65 ms) which do not fit a 16-bit unsigned int. */
static void short_delay(unsigned long us)
{
    unsigned long i;
    for (i = 0; i < us; i++) (void)inp(0x80);
}

static void opl_write(unsigned char reg, unsigned char val)
{
    outp(OPL_ADDR, reg);
    short_delay(4);         /* ~4 us, covers OPL2 address-wait spec of 3.3 us */
    outp(OPL_DATA, val);
    short_delay(28);        /* ~28 us, covers OPL2 data-wait spec of 23 us */
}

/* OPL2 operator offsets per channel 0..8. Modulator first, carrier second. */
static const unsigned char opl_mod_op[9] = { 0, 1, 2, 8, 9,10,16,17,18 };
static const unsigned char opl_car_op[9] = { 3, 4, 5,11,12,13,19,20,21 };

/* Program channel ch as a brass-ish voice with fast attack, short sustain,
 * mild feedback for grit. Suitable for barking and minor-chord sustain. */
static void opl_program_brass(int ch)
{
    unsigned char mop = opl_mod_op[ch];
    unsigned char cop = opl_car_op[ch];

    /* 0x20: bit 5 sustain-enable, bits 0..3 multiplier (1) */
    opl_write((unsigned char)(0x20 + mop), 0x21);
    opl_write((unsigned char)(0x20 + cop), 0x21);

    /* 0x40: total level (attenuation, 0=loudest).
     * Modulator moderate (0x10); carrier loud (0x05). */
    opl_write((unsigned char)(0x40 + mop), 0x10);
    opl_write((unsigned char)(0x40 + cop), 0x05);

    /* 0x60: attack nibble (F=fastest), decay nibble (low digits = slow). */
    opl_write((unsigned char)(0x60 + mop), 0xF2);
    opl_write((unsigned char)(0x60 + cop), 0xF2);

    /* 0x80: sustain level nibble (0=loudest sustain), release nibble. */
    opl_write((unsigned char)(0x80 + mop), 0x17);
    opl_write((unsigned char)(0x80 + cop), 0x17);

    /* 0xE0: waveform; 0=sine, perfectly fine for a growl on top of feedback. */
    opl_write((unsigned char)(0xE0 + mop), 0x00);
    opl_write((unsigned char)(0xE0 + cop), 0x00);

    /* 0xC0: per-channel feedback (high nibble 0..7) and connection (bit 0:
     * 0=FM, 1=additive). Strong feedback plus FM produces the grit. */
    opl_write((unsigned char)(0xC0 + ch), 0x0C);
}

static void opl_reset(void)
{
    int r;
    /* Silence every operator slot across all nine channels. Range 0x20..0xF5
     * covers operator regs and the 0xA0/0xB0 frequency block. Writing 0
     * everywhere is the brute-force reset the AdLib reference drivers use. */
    for (r = 0x20; r <= 0xF5; r++) {
        opl_write((unsigned char)r, 0);
    }
    /* 0x01 bit 5 enables waveform select; we keep it off (sine only). */
    opl_write(0x01, 0x00);
    /* 0xBD rhythm/vibrato/tremolo controller; zero to disable. */
    opl_write(0xBD, 0x00);
}

/* Note-on: load fnum low byte, then set key-on bit with high nibble of fnum
 * and block. Block=3 puts us near the low-growl register for low-A minor. */
static void opl_note_on(int ch, unsigned int fnum, unsigned char block)
{
    opl_write((unsigned char)(0xA0 + ch), (unsigned char)(fnum & 0xFF));
    opl_write((unsigned char)(0xB0 + ch),
              (unsigned char)(0x20 |
                              ((block & 0x07) << 2) |
                              ((fnum >> 8) & 0x03)));
}

static void opl_note_off(int ch)
{
    /* Clear key-on bit. Release rate in 0x80 governs how quickly the voice
     * decays; we leave the fnum registers alone so the release still tracks
     * pitch correctly. */
    opl_write((unsigned char)(0xB0 + ch), 0x00);
}

/* A quick bark. Note-on, short sustain, note-off. Used during the eye
 * cascade so each head's awakening carries an audible cue. */
static void opl_bark(int ch, unsigned int fnum, unsigned char block)
{
    opl_note_on(ch, fnum, block);
    short_delay(45000);  /* ~45 ms, short enough to feel punchy */
    opl_note_off(ch);
    short_delay(5000);   /* ~5 ms release tail before the next bark */
}

/* A-minor triad at block 3, approximate OPL2 fnums. Exact cents do not
 * matter for a stinger; the interval profile is what carries the menace. */
#define FNUM_A2  290U   /* ~110 Hz, root */
#define FNUM_C3  173U   /* ~130.8 Hz at block 4 */
#define FNUM_E3  218U   /* ~164.8 Hz at block 4 */
#define FNUM_D2  194U   /* ~73 Hz, first bark */
#define FNUM_F2  230U   /* ~87 Hz, second bark */

/* Sub-bass voice on channel 3. Sine carrier with long sustain, modulator
 * output backed off so the result is a clean deep rumble rather than a
 * metallic growl. Multiplier=0 halves frequency per operator, so an
 * A2-shaped fnum plays at A1 (~55 Hz). */
static void opl_program_sub(int ch)
{
    unsigned char mop = opl_mod_op[ch];
    unsigned char cop = opl_car_op[ch];
    opl_write((unsigned char)(0x20 + mop), 0x20);  /* sustain, mult 0 */
    opl_write((unsigned char)(0x20 + cop), 0x20);
    opl_write((unsigned char)(0x40 + mop), 0x1C);  /* modulator moderate */
    opl_write((unsigned char)(0x40 + cop), 0x00);  /* carrier loudest */
    opl_write((unsigned char)(0x60 + mop), 0xA4);
    opl_write((unsigned char)(0x60 + cop), 0xA2);
    opl_write((unsigned char)(0x80 + mop), 0x03);  /* high sustain */
    opl_write((unsigned char)(0x80 + cop), 0x03);
    opl_write((unsigned char)(0xE0 + mop), 0x00);
    opl_write((unsigned char)(0xE0 + cop), 0x00);
    opl_write((unsigned char)(0xC0 + ch),  0x06);  /* feedback 3, FM */
}

/* Flip the vibrato bit (0x20 register, bit 6) on both operators of the
 * given channel. Register 0xBD bit 6 is the global vibrato depth; caller
 * handles that separately if a wider wobble is wanted. */
static void opl_set_vibrato(int ch, int enable)
{
    unsigned char mop = opl_mod_op[ch];
    unsigned char cop = opl_car_op[ch];
    unsigned char v = enable ? 0x61 : 0x21;  /* preserves sustain + mult=1 */
    opl_write((unsigned char)(0x20 + mop), v);
    opl_write((unsigned char)(0x20 + cop), v);
}

/* AdLib rhythm-mode snare hit. Programs channel 7's carrier operator for
 * a short percussive sine, then drives the snare bit of 0xBD. Channels
 * 0..5 are untouched so our sustained chord keeps ringing. */
static void opl_snare(void)
{
    /* Program ch7 carrier (op 20) for a short percussive tone */
    opl_write((unsigned char)(0x20 + 20), 0x01);
    opl_write((unsigned char)(0x40 + 20), 0x00);
    opl_write((unsigned char)(0x60 + 20), 0xF8);
    opl_write((unsigned char)(0x80 + 20), 0x07);
    opl_write((unsigned char)(0xE0 + 20), 0x00);

    /* Snare pitch via ch7 frequency regs */
    opl_write(0xA0 + 7, 0x00);
    opl_write(0xB0 + 7, 0x09);  /* block 2 */

    /* Rhythm mode on, deep vibrato + tremolo so the sustained chord gets
     * a matching peak swell. Bit 5 = rhythm enable. */
    opl_write(0xBD, 0xE0);           /* AM depth + VIB depth + rhythm on */
    short_delay(2000);
    opl_write(0xBD, 0xE0 | 0x08);    /* + snare trigger */
    short_delay(90000UL);
    opl_write(0xBD, 0xE0);           /* release snare */
}

/* Reset the 0xBD rhythm and depth bits back to off. Call before OPL reset
 * at the end of the splash so the controller is in a known state. */
static void opl_rhythm_off(void)
{
    opl_write(0xBD, 0x00);
}

/* ----------------------------------------------------------------------- */
/* VGA DAC quick flashes. No-op on adapters without a DAC.                  */
/* ----------------------------------------------------------------------- */

static void dac_set_entry(unsigned char idx,
                          unsigned char r,
                          unsigned char g,
                          unsigned char b)
{
    outp(DAC_WRITE_INDEX, idx);
    outp(DAC_DATA, r);
    outp(DAC_DATA, g);
    outp(DAC_DATA, b);
}

/* Pulse DAC[0] (background) to solid white for `us` microseconds, then
 * back to black. Entire screen pops to white and returns. Used for the
 * chain-shatter camera flash. */
static void flash_dac_white(unsigned long us)
{
    if (!has_dac()) return;
    dac_set_entry(0, 63, 63, 63);
    short_delay(us);
    dac_set_entry(0, 0, 0, 0);
}

/* Pulse DAC[0] (background) to deep red for `us` microseconds, then back
 * to black. Used for the heartbeat pre-sequence. */
static void flash_dac_red(unsigned long us)
{
    if (!has_dac()) return;
    dac_set_entry(0, 30, 0, 0);
    short_delay(us);
    dac_set_entry(0, 0, 0, 0);
}

/* ----------------------------------------------------------------------- */
/* Tiny RNG for the hellfire ember flicker                                  */
/* ----------------------------------------------------------------------- */

static unsigned int rng_state = 0xACE1u;
static unsigned int rng_next(void)
{
    /* LCG: parameters from Numerical Recipes; 16-bit state for Watcom 16-bit
     * unsigned int. Output is the whole state. */
    rng_state = (unsigned int)((unsigned long)rng_state * 25173UL + 13849UL);
    return rng_state;
}

/* ----------------------------------------------------------------------- */
/* Embellishments. Chain rattle, breath sparks, serpent wiggle, tagline     */
/* flash. Each is a one-function tick driver called during sustain.         */
/* ----------------------------------------------------------------------- */

/* Row 16 is the body-with-chain row. Rewrite it with the chain link glyphs
 * offset by `shift` so the chain appears to rattle. If `broken_col` is a
 * column inside the chain range, leave that cell blank to show the chain
 * is severed at that link. */
static void rattle_chain(int shift, int broken_col,
                         unsigned char body_attr,
                         unsigned char chain_attr)
{
    int i;
    int left = DOG_LEFT_H1;
    int right = DOG_LEFT_H3 + 8;
    int width = right - left + 1;
    for (i = 1; i < width - 1; i++) {
        int col = left + i;
        unsigned char g;
        if (col == broken_col) {
            put_cell(16, col, ' ', A_BLACK);
            continue;
        }
        if (((i + shift) & 0x03) == 0) g = G_CHAIN;
        else                           g = G_BLOCK_DK;
        put_cell(16, col, g, (g == G_CHAIN) ? chain_attr : body_attr);
    }
}

/* Row 22 hellfire. Each tick, 60% of the interior columns get a random
 * block-shade glyph in an ember color; the rest are cleared. Looks like
 * a bank of coals at the foot of the gate. Only active during sustain,
 * cleared at sustain exit. */
static void hellfire_tick(void)
{
    int c;
    int mono = is_mono();
    for (c = 2; c < COLS - 2; c++) {
        unsigned int r = rng_next();
        unsigned char g;
        unsigned char a;
        if ((r & 0x07) >= 5) {
            /* No ember in this cell this tick */
            put_cell(22, c, ' ', A_BLACK);
            continue;
        }
        switch ((r >> 4) & 0x03) {
            case 0:  g = G_BLOCK_LT;   break;
            case 1:  g = G_BLOCK_MD;   break;
            case 2:  g = G_BLOCK_DK;   break;
            default: g = G_BLOCK_FULL; break;
        }
        if (mono) {
            a = ((r & 0x08) ? A_WHITE : A_GRAY);
        } else {
            /* Hot embers mostly red, occasional yellow, rare brown
             * to suggest cooling and resurging coals. */
            int pick = (r >> 6) & 0x07;
            if      (pick < 4) a = A_RED;
            else if (pick < 6) a = A_YELLOW;
            else               a = A_BROWN;
        }
        put_cell(22, c, g, a);
    }
}

static void hellfire_clear(void)
{
    int c;
    for (c = 2; c < COLS - 2; c++) put_cell(22, c, ' ', A_BLACK);
}

/* Breath sparks: small rising glyphs above each head's mouth. `tick` drives
 * a cheap random walk so the sparks feel alive rather than marching in
 * lockstep. */
static void draw_sparks(int tick, unsigned char attr)
{
    /* Three mouth positions, midline of each head at col+4, row DOG_TOP+3.
     * Sparks jitter between col+4 and col+5. Chosen to avoid colliding with
     * spine glyphs on row DOG_TOP-1 which sit at col+1, +3, +5, +7 (col+5
     * does overlap one spine slot; the spark overwrites it briefly, and
     * the spine is redrawn in erase_sparks). */
    static const int mouth_col[3] = {
        DOG_LEFT_H1 + 4, DOG_LEFT_H2 + 4, DOG_LEFT_H3 + 4
    };
    int i;
    for (i = 0; i < 3; i++) {
        int age = (tick + i) & 0x03;
        int row = DOG_TOP - 1 - age;
        int col = mouth_col[i] + (((tick + i * 3) & 1) ? 0 : 1);
        unsigned char g;
        if (age == 0)      g = '.';
        else if (age == 1) g = 0xF9;  /* small bullet */
        else if (age == 2) g = '\'';
        else               g = '`';
        if (row >= 2) put_cell(row, col, g, attr);
    }
}

/* Erase the sparks before redrawing or exiting, so stale glyphs do not
 * linger in the gate frame. Covers the jitter range (col+4, col+5) across
 * four rows above each mouth. Row DOG_TOP-1 shares columns with the spine
 * row, so spines are redrawn after clearing. */
static void erase_sparks(unsigned char spine_attr)
{
    static const int mouth_col[3] = {
        DOG_LEFT_H1 + 4, DOG_LEFT_H2 + 4, DOG_LEFT_H3 + 4
    };
    int i, a;
    for (i = 0; i < 3; i++) {
        for (a = 0; a < 4; a++) {
            int row = DOG_TOP - 1 - a;
            int col_m = mouth_col[i];
            int col_r = mouth_col[i] + 1;
            if (row >= 2) {
                put_cell(row, col_m, ' ', A_BLACK);
                put_cell(row, col_r, ' ', A_BLACK);
            }
        }
    }
    /* Rewrite spines so the clear pass does not leave gaps at col+5 slots. */
    draw_spines(spine_attr);
}

/* Serpent tail wiggle. Swap the last few cells of the tail between two
 * poses on alternating ticks. */
static void wiggle_tail(int tick, unsigned char attr)
{
    int tail_row = DOG_TOP + 8;
    int tail_col = DOG_LEFT_H3 + 10;
    if (tick & 1) {
        put_cell(tail_row, tail_col + 0, G_APPROX, attr);
        put_cell(tail_row, tail_col + 1, G_APPROX, attr);
        put_cell(tail_row, tail_col + 2, G_SER_UP, attr);
        put_cell(tail_row, tail_col + 3, G_SER_DN, attr);
        put_cell(tail_row, tail_col + 4, G_APPROX, attr);
    } else {
        put_cell(tail_row, tail_col + 0, G_SER_DN, attr);
        put_cell(tail_row, tail_col + 1, G_APPROX, attr);
        put_cell(tail_row, tail_col + 2, G_APPROX, attr);
        put_cell(tail_row, tail_col + 3, G_SER_UP, attr);
        put_cell(tail_row, tail_col + 4, G_SER_DN, attr);
    }
}

/* Tagline flash. During the hot sustain, swap the normal "Three heads.
 * One machine. Zero pretending." line for a shorter, louder BEHOLD cue
 * for one tick, then swap back. Keeps the main tagline on screen most of
 * the time. */
static void write_tagline(const char *s, unsigned char attr)
{
    /* Clear row 19 before writing to handle length changes between the
     * two messages. Row 19 is the tagline slot; the version line sits
     * at row 20 and is not rewritten. */
    int c;
    for (c = 2; c < COLS - 2; c++) put_cell(19, c, ' ', A_BLACK);
    put_str(19, (COLS - (int)strlen(s)) / 2, s, attr);
}

/* Wordmark color pulse. During sustain, cycle the wordmark attribute across
 * a palette that suggests the heat has not yet settled. Cheap way to keep
 * the eye moving during the hold phase. */
static void pulse_wordmark(int tick, int mono)
{
    static const unsigned char colors[4] = {
        A_BRIGHT_CYAN, A_WHITE, A_YELLOW, A_BRIGHT_CYAN
    };
    static const unsigned char mono_colors[4] = {
        A_WHITE, A_GRAY, A_WHITE, A_GRAY
    };
    unsigned char attr = mono ? mono_colors[tick & 3] : colors[tick & 3];
    /* Rewriting just the attribute bytes of the wordmark block. Cheaper
     * than re-rendering the letters. Wordmark occupies rows 3..7, cols
     * 16..62; 47 cols wide. */
    int r, c;
    unsigned char __far *v = vram_seg();
    for (r = 3; r <= 7; r++) {
        for (c = 16; c <= 62; c++) {
            unsigned int off = (unsigned int)((r * COLS + c) * 2);
            if (v[off] == G_BLOCK_FULL) v[off + 1] = attr;
        }
    }
}

/* ----------------------------------------------------------------------- */
/* Clear screen for handoff to display_banner                               */
/* ----------------------------------------------------------------------- */

static void clear_for_exit(void)
{
    union REGS r;
    fill_screen(' ', A_GRAY);
    /* Reset cursor to 0,0 via BIOS so the next printf lands at the top. */
    r.h.ah = 0x02;
    r.h.bh = 0;
    r.h.dh = 0;
    r.h.dl = 0;
    int86(0x10, &r, &r);
}

/* ----------------------------------------------------------------------- */
/* Public entry                                                             */
/* ----------------------------------------------------------------------- */

void intro_splash(const opts_t *o)
{
    unsigned char wordmark_attr;
    unsigned char body_attr;
    unsigned char chain_attr;
    unsigned char serpent_attr;
    unsigned char text_attr;
    unsigned char gate_attr;
    unsigned char saved_dac[16][3];
    int dac_ok;
    int early;
    int phase;

    if (!o || o->no_intro || o->no_ui) return;

    /* Pick palette for adapter class. */
    if (is_mono()) {
        wordmark_attr = A_WHITE;
        body_attr     = A_GRAY;
        chain_attr    = A_WHITE;
        serpent_attr  = A_GRAY;
        text_attr     = A_GRAY;
        gate_attr     = A_WHITE;
    } else {
        wordmark_attr = A_BRIGHT_CYAN;
        body_attr     = A_DIM_GRAY;
        chain_attr    = A_BROWN;
        serpent_attr  = A_BRIGHT_BLUE;
        text_attr     = A_GRAY;
        gate_attr     = A_BROWN;
    }

    /* Render the static frame with eyes at "off" attribute so the
     * fade-in presents black eyes, ready to be pulsed. On VGA we
     * first save and zero the DAC so the render is invisible until
     * we fade the DAC up. */
    dac_ok = 0;
    if (has_dac()) {
        dac_save16(saved_dac);
        /* Zero the DAC so anything we draw stays invisible */
        {
            unsigned char zero[16][3];
            memset(zero, 0, sizeof(zero));
            dac_write16((const unsigned char (*)[3])zero);
            dac_ok = 1;
        }
    }

    /* Silence any stuck OPL2 state and program our voices BEFORE the
     * visual begins: channels 0..2 carry the cascade and sustained chord,
     * channel 3 carries the sub-bass drone. The snare hit at peak uses
     * rhythm mode on channel 7. */
    opl_reset();
    opl_program_brass(0);
    opl_program_brass(1);
    opl_program_brass(2);
    opl_program_sub(3);

    /* Heartbeat pre-sequence. Runs with DAC still at saved colors (pre-
     * zero), so the red flashes show on the otherwise-black screen that
     * draw_frame has not yet populated. Two low-pitch thumps with
     * synchronized red pulses build anticipation before the dog appears.
     *
     * Non-VGA path skips the DAC-based flash; the thump still fires, and
     * a fill_screen cycle substitutes for the visual. */
    fill_screen(' ', A_BLACK);

    {
        /* Thump 1 */
        opl_note_on(3, FNUM_A2, 1);   /* A1, one octave below chord root */
        if (dac_ok) {
            flash_dac_red(80000UL);
        } else {
            /* Space chars have no fg pixels, so recolor alone is invisible.
             * Fill with solid block glyphs tinted hot, then clear back to
             * black spaces. Visible on CGA/EGA color and on mono where the
             * fill renders as light-gray block on mono. */
            fill_screen(G_BLOCK_FULL,
                        (unsigned char)(is_mono() ? A_GRAY : A_DARK_RED));
            short_delay(80000UL);
            fill_screen(' ', A_BLACK);
        }
        short_delay(50000);
        opl_note_off(3);
        wait_ticks_or_key(3);

        /* Thump 2 */
        opl_note_on(3, FNUM_A2, 1);
        if (dac_ok) {
            flash_dac_red(80000UL);
        } else {
            fill_screen(G_BLOCK_FULL,
                        (unsigned char)(is_mono() ? A_GRAY : A_DARK_RED));
            short_delay(80000UL);
            fill_screen(' ', A_BLACK);
        }
        short_delay(50000);
        opl_note_off(3);
        wait_ticks_or_key(2);
    }

    /* Now prepare for the main render: zero DAC (VGA) so draw_frame's
     * output is invisible until we fade it up. */
    if (dac_ok) {
        unsigned char zero[16][3];
        memset(zero, 0, sizeof(zero));
        dac_write16((const unsigned char (*)[3])zero);
    }

    draw_frame(wordmark_attr, body_attr, chain_attr, serpent_attr,
               text_attr, gate_attr,
               (unsigned char)(is_mono() ? A_DIM_GRAY : A_BLACK));

    /* Start sub-bass drone. Plays through the cascade, sustain, and
     * into the fade-out. Channel 3 carries it; the chord uses 0..2. */
    opl_note_on(3, FNUM_A2, 2);

    /* Fade in (VGA) */
    early = 0;
    if (dac_ok) {
        early = dac_fade_in(saved_dac, 6);
    } else {
        /* Non-VGA: brief pause so the static frame registers before
         * we start pulsing eyes. */
        early = wait_ticks_or_key(2);
    }

    /* Eye pulse animation: seven phases, ~2 ticks per phase.
     * Head 0 lights phase 0..1 (dim then bright), goes hot phase 2.
     * Head 1 lights phase 2..3, goes hot phase 4.
     * Head 2 lights phase 4..5, goes hot phase 6.
     * A bark fires at each head's transition to hot, escalating in pitch. */
    if (!early) {
        for (phase = 0; phase <= 6; phase++) {
            apply_eye_phase(phase);
            if      (phase == 2) opl_bark(0, FNUM_D2, 3);
            else if (phase == 4) opl_bark(1, FNUM_F2, 3);
            else if (phase == 6) opl_bark(2, FNUM_A2, 3);
            if (wait_ticks_or_key(2)) { early = 1; break; }
        }
    }

    /* Chain shatter moment at the climax of the cascade. Flash white
     * (whole screen via DAC pulse on VGA, solid-block fill on other
     * adapters), trigger the snare, mark one chain link broken so the
     * sustain-phase rattle draws that cell empty. Narrative beat: the
     * three-headed dog has just broken its chain. */
    {
        int chain_broken_col = 39;   /* middle chain link */
        if (!early) {
            if (dac_ok) {
                flash_dac_white(45000);
            } else {
                fill_screen(G_BLOCK_FULL,
                            (unsigned char)(is_mono() ? A_WHITE : A_WHITE));
                short_delay(45000);
                /* Do not clear; draw_frame content was overwritten, redraw */
                draw_frame(wordmark_attr, body_attr, chain_attr, serpent_attr,
                           text_attr, gate_attr,
                           (unsigned char)(is_mono() ? A_WHITE : A_YELLOW));
                /* Restore eye phase to fully hot since we just overwrote */
                apply_eye_phase(6);
            }
            opl_snare();
            put_cell(16, chain_broken_col, ' ', A_BLACK);
        }

        /* Sustain at peak with the ridiculous-hilarity layer on top.
         *   A-minor triad held across channels 0/1/2 with vibrato.
         *   Sub-bass drone continues on channel 3.
         *   Chain rattles by shifting glyph positions, link 39 stays broken.
         *   Hellfire embers flicker at row 22.
         *   Breath sparks rise above each mouth.
         *   Serpent tail wiggles between two poses.
         *   Wordmark pulses through a 4-step color cycle.
         *   Tagline briefly flashes BEHOLD at the mid-point.
         * Loop runs 10 ticks (~550 ms) unless the user dismisses. */
        if (!early) {
            int t;
            opl_set_vibrato(0, 1);
            opl_set_vibrato(1, 1);
            opl_set_vibrato(2, 1);
            opl_note_on(0, FNUM_A2, 3);
            opl_note_on(1, FNUM_C3, 4);
            opl_note_on(2, FNUM_E3, 4);
            for (t = 0; t < 10; t++) {
                rattle_chain(t, chain_broken_col, body_attr, chain_attr);
                hellfire_tick();
                draw_sparks(t,
                            (unsigned char)(is_mono() ? A_WHITE : A_YELLOW));
                wiggle_tail(t, serpent_attr);
                pulse_wordmark(t, is_mono());
                if (t == 5) {
                    write_tagline("B E H O L D .",
                                  (unsigned char)(is_mono() ? A_WHITE : A_RED));
                } else if (t == 7) {
                    write_tagline("Tough Times Demand Tough Tests", text_attr);
                }
                if (wait_ticks_or_key(1)) { early = 1; break; }
            }
            opl_note_off(0);
            opl_note_off(1);
            opl_note_off(2);
            opl_set_vibrato(0, 0);
            opl_set_vibrato(1, 0);
            opl_set_vibrato(2, 0);
            erase_sparks(serpent_attr);
            hellfire_clear();
        }
    }

    /* Fade out (VGA) or attribute dim-down (other). */
    if (!early) {
        if (dac_ok) {
            dac_fade_out(saved_dac, 6);
        } else {
            /* Sub-step fade via attribute: recolor whole screen to dim gray,
             * then black. Crude but adequate. */
            recolor_all(A_DIM_GRAY);
            wait_ticks_or_key(2);
            recolor_all(A_BLACK);
            wait_ticks_or_key(1);
        }
    } else {
        /* User dismissed early: silence OPL, clean up sparks, jump to
         * blank; still restore the DAC so the banner colors land right. */
        opl_note_off(0);
        opl_note_off(1);
        opl_note_off(2);
        opl_note_off(3);
        erase_sparks(serpent_attr);
        hellfire_clear();
        if (dac_ok) dac_write16(saved_dac);
    }

    /* Silence drone, cut rhythm mode, fully reset OPL, and restore DAC
     * so the subsequent display_banner inherits a clean machine state. */
    opl_note_off(3);
    opl_rhythm_off();
    if (dac_ok) dac_write16(saved_dac);
    opl_reset();

    clear_for_exit();
    drain_keys();
}
