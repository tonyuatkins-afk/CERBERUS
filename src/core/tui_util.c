/*
 * Shared text-mode UI primitives — v0.6.2 T1 implementation.
 * See tui_util.h for rationale.
 */

#include <dos.h>
#include <conio.h>
#include "tui_util.h"
#include "display.h"

unsigned char __far *tui_vram_base(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

int tui_is_mono(void)
{
    adapter_t a = display_adapter();
    return (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
            a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO);
}

void tui_putc(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v = tui_vram_base();
    unsigned int off = (unsigned int)((row * TUI_COLS + col) * 2);
    v[off]     = ch;
    v[off + 1] = attr;
}

void tui_puts(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < TUI_COLS) {
        tui_putc(row, col++, (unsigned char)*s++, attr);
    }
}

void tui_fill(int r0, int r1, unsigned char ch, unsigned char attr)
{
    int r, c;
    for (r = r0; r <= r1; r++) {
        for (c = 0; c < TUI_COLS; c++) {
            tui_putc(r, c, ch, attr);
        }
    }
}

void tui_hline(int row, int c0, int c1, unsigned char glyph,
               unsigned char attr)
{
    int c;
    for (c = c0; c <= c1; c++) tui_putc(row, c, glyph, attr);
}

void tui_cursor(int row, int col)
{
    union REGS r;
    r.h.ah = 0x02;
    r.h.bh = 0;
    r.h.dh = (unsigned char)row;
    r.h.dl = (unsigned char)col;
    int86(0x10, &r, &r);
}

unsigned long tui_ticks(void)
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

int tui_kbhit(void)
{
    return kbhit() ? 1 : 0;
}

void tui_read_key(unsigned char *ascii, unsigned char *scan)
{
    union REGS r;
    r.h.ah = 0x00;
    int86(0x16, &r, &r);
    *ascii = r.h.al;
    *scan  = r.h.ah;
}

void tui_drain_keys(void)
{
    unsigned char a, s;
    while (tui_kbhit()) {
        tui_read_key(&a, &s);
        (void)a; (void)s;
    }
}
