/*
 * Journey framework implementation — v0.6.0 T1.
 *
 * Text-mode primitives for title cards, result-flash banners, and the
 * non-blocking skip-key polling that runs during visual demonstrations.
 * All VRAM writes are direct (B800:0000 color / B000:0000 mono), same
 * pattern as ui.c, so rendering doesn't disturb scrolling stdout from
 * earlier detection output.
 *
 * Keyboard polling uses INT 16h:
 *   AH=01h: non-blocking peek — ZF=0 if key waiting
 *   AH=00h: blocking read — returns AL=ASCII, AH=scan
 *
 * Timing uses BIOS tick counter at 0040:006C (18.2 Hz, 32-bit wrap).
 * Title cards nominally hold for ~45 ticks (~2.5 s); any key ends the
 * hold early. Esc sets the skip-all latch.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include "journey.h"
#include "display.h"
#include "head_art.h"

/* ----------------------------------------------------------------------- */
/* Module state                                                             */
/* ----------------------------------------------------------------------- */

static int g_skip_all = 0;

/* ----------------------------------------------------------------------- */
/* VRAM helpers — local copies so journey.c doesn't need to link against    */
/* ui.c's statics.                                                          */
/* ----------------------------------------------------------------------- */

#define JCOLS 80
#define JROWS 25

static unsigned char __far *j_vram(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

static int j_is_mono(void)
{
    adapter_t a = display_adapter();
    return (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
            a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO);
}

static void j_putc(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v = j_vram();
    unsigned int off = (unsigned int)((row * JCOLS + col) * 2);
    v[off]     = ch;
    v[off + 1] = attr;
}

static void j_puts(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < JCOLS) {
        j_putc(row, col++, (unsigned char)*s++, attr);
    }
}

static void j_fill(int row_start, int row_end,
                   unsigned char ch, unsigned char attr)
{
    int r, c;
    for (r = row_start; r <= row_end; r++) {
        for (c = 0; c < JCOLS; c++) j_putc(r, c, ch, attr);
    }
}

static void j_hline(int row, int col_start, int col_end,
                    unsigned char glyph, unsigned char attr)
{
    int c;
    for (c = col_start; c <= col_end; c++) j_putc(row, c, glyph, attr);
}

/* ----------------------------------------------------------------------- */
/* BIOS tick + key helpers                                                  */
/* ----------------------------------------------------------------------- */

/* 32-bit tick counter at 0040:006C, 18.2 Hz. Read atomically: BIOS INT 8
 * may update it between our two 16-bit reads, so we retry on mismatch. */
static unsigned long j_ticks(void)
{
    unsigned int __far *low  = (unsigned int __far *)MK_FP(0x0040, 0x006C);
    unsigned int __far *high = (unsigned int __far *)MK_FP(0x0040, 0x006E);
    unsigned int h1, h2, l;
    do {
        h1 = *high;
        l  = *low;
        h2 = *high;
    } while (h1 != h2);
    return ((unsigned long)h1 << 16) | l;
}

/* Non-blocking: returns 1 if a key is waiting in the BIOS buffer.
 * Watcom's REGS union doesn't expose ZF, so use kbhit() (conio.h)
 * which wraps the same INT 16h AH=01h check portably. */
static int j_key_ready(void)
{
    return kbhit() ? 1 : 0;
}

/* Blocking read. AL=ASCII (0 for extended), AH=scan. */
static void j_read_key(unsigned char *ascii, unsigned char *scan)
{
    union REGS r;
    r.h.ah = 0x00;
    int86(0x16, &r, &r);
    *ascii = r.h.al;
    *scan  = r.h.ah;
}

/* Drain any pending key without acting on it, so stale keystrokes from
 * earlier interaction don't race with the next title card. */
static void j_drain_keys(void)
{
    unsigned char a, s;
    while (j_key_ready()) {
        j_read_key(&a, &s);
        (void)a; (void)s;
    }
}

/* Classify the most recently read key: 1 = Esc, 0 = anything else. */
static int j_key_is_esc(unsigned char ascii, unsigned char scan)
{
    (void)scan;
    return (ascii == 0x1B) ? 1 : 0;
}

/* Classify: 1 = 'S' or 's'. */
static int j_key_is_skip(unsigned char ascii, unsigned char scan)
{
    (void)scan;
    return (ascii == 'S' || ascii == 's') ? 1 : 0;
}

/* ----------------------------------------------------------------------- */
/* Public API                                                               */
/* ----------------------------------------------------------------------- */

void journey_init(void)
{
    g_skip_all = 0;
}

int journey_should_skip(const opts_t *o)
{
    if (!o) return 0;
    if (o->no_ui)   return 1;
    if (o->do_quick) return 1;
    if (g_skip_all) return 1;
    return 0;
}

int journey_poll_skip(void)
{
    unsigned char a, s;
    if (!j_key_ready()) return 0;
    j_read_key(&a, &s);
    if (j_key_is_esc(a, s)) { g_skip_all = 1; return 2; }
    if (j_key_is_skip(a, s)) return 1;
    /* any other key: also skip this visual — the user clearly wants to
     * move on. Matches Tony's "any key to continue" intuition. */
    return 1;
}

/* Render one row of the section head at (screen_row, col) using the
 * shared head_art table. */
static void j_draw_head_row(int screen_row, head_dir_t variant, int head_row,
                            int screen_col,
                            unsigned char body_attr, unsigned char accent_attr)
{
    int c;
    for (c = 0; c < HEAD_COLS; c++) {
        head_cell_t cell = head_art[variant][head_row][c];
        unsigned char attr;
        switch (cell.kind) {
            case HEAD_CELL_EYE:
            case HEAD_CELL_FANG: attr = accent_attr; break;
            default:             attr = body_attr;   break;
        }
        j_putc(screen_row, screen_col + c, cell.glyph, attr);
    }
}

/* Word-wrap `desc` into up to 3 lines of `max_w` cells starting at
 * (row, col). Breaks on spaces. Lines past 3 silently truncate. */
static void j_draw_desc(int row, int col, int max_w,
                        const char *desc, unsigned char attr)
{
    int line;
    const char *p = desc;
    for (line = 0; line < 3 && *p; line++) {
        const char *line_start = p;
        const char *last_space = (const char *)0;
        int len = 0;
        while (*p && len < max_w) {
            if (*p == ' ') last_space = p;
            p++; len++;
        }
        if (*p && *p != ' ' && last_space && last_space > line_start) {
            /* break at last space instead of mid-word */
            len = (int)(last_space - line_start);
            p = last_space + 1;
        } else if (*p == ' ') {
            p++;  /* consume the wrap-boundary space */
        }
        /* emit [line_start, line_start+len) */
        {
            int i;
            for (i = 0; i < len; i++) {
                j_putc(row + line, col + i,
                       (unsigned char)line_start[i], attr);
            }
        }
    }
}

int journey_title_card(const opts_t *o,
                       head_dir_t variant,
                       const char *title,
                       const char *desc)
{
    unsigned char body_attr, accent_attr, title_attr, desc_attr, rule_attr;
    unsigned long start, deadline;
    int early = 0;

    if (journey_should_skip(o)) return g_skip_all ? 1 : 0;

    if (j_is_mono()) {
        body_attr   = ATTR_BOLD;
        accent_attr = ATTR_BOLD;
        title_attr  = ATTR_BOLD;
        desc_attr   = ATTR_NORMAL;
        rule_attr   = ATTR_NORMAL;
    } else {
        body_attr   = ATTR_CYAN;
        accent_attr = ATTR_YELLOW;
        title_attr  = ATTR_BOLD;          /* bright white */
        desc_attr   = ATTR_NORMAL;        /* gray */
        rule_attr   = ATTR_CYAN;
    }

    /* Clear the screen, then draw the card centered vertically.
     * Layout: top rule row 8, head rows 9-12 (title on row 10 beside
     * head_row 1), desc rows 14-16, bottom rule row 18. */
    j_fill(0, JROWS - 1, ' ', ATTR_NORMAL);

    /* Horizontal rules — double-line for emphasis */
    j_hline(8,  3, JCOLS - 4, CP437_DBL_HORIZ, rule_attr);
    j_hline(18, 3, JCOLS - 4, CP437_DBL_HORIZ, rule_attr);

    /* Head on left starting col 4, title to its right on the eye row */
    {
        int hr;
        for (hr = 0; hr < HEAD_ROWS; hr++) {
            j_draw_head_row(9 + hr, variant, hr, 4, body_attr, accent_attr);
        }
        j_puts(10, 4 + HEAD_COLS + 3, title, title_attr);
    }

    /* Description starting row 14, column 4, 70 cells wide */
    if (desc) j_draw_desc(14, 4, 70, desc, desc_attr);

    /* Hint on row 20 */
    j_puts(20, 4, "[any key to continue, S to skip, Esc to skip all]",
           desc_attr);

    /* Drain stale keys so the 2.5-second hold isn't pre-empted by
     * leftover keystrokes. */
    j_drain_keys();

    start    = j_ticks();
    deadline = start + 45UL;   /* ~2.5 s at 18.2 Hz */

    while (j_ticks() < deadline) {
        if (j_key_ready()) {
            unsigned char a, s;
            j_read_key(&a, &s);
            if (j_key_is_esc(a, s)) {
                g_skip_all = 1;
                early = 1;
            }
            break;
        }
    }
    (void)early;
    return g_skip_all ? 1 : 0;
}

void journey_result_flash(const opts_t *o, const char *result)
{
    unsigned char attr;
    int col;
    int len;
    if (journey_should_skip(o)) return;

    attr = (unsigned char)(j_is_mono() ? ATTR_BOLD : ATTR_GREEN);

    /* Clear a banner strip at row 12 and write centered result. */
    j_fill(12, 12, ' ', ATTR_NORMAL);
    len = (int)strlen(result);
    col = (JCOLS - len) / 2;
    if (col < 0) col = 0;
    j_puts(12, col, result, attr);

    /* Brief hold (~1 s), non-blocking on key. */
    {
        unsigned long start = j_ticks();
        unsigned long deadline = start + 18UL;
        j_drain_keys();
        while (j_ticks() < deadline) {
            if (j_key_ready()) break;
        }
    }
}
