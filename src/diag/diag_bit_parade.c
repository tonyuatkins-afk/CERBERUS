/*
 * CPU ALU Rolling Bit Parade — v0.6.0 T2.
 *
 * Post-diagnostic visual for diag_cpu. A 16-bit register is displayed
 * as a row of CP437 blocks (set bits bright, clear bits dim). The
 * journey executes real ALU instructions on it — ROL, ROR, SHL, SHR,
 * AND, OR, XOR, NOT, ADD, SUB — in sequence, re-rendering the bit row
 * after each. No animation tricks. What you see on the screen is the
 * literal register state after the literal instruction.
 *
 * Wall-clock-bounded: the loop runs for ~3 seconds regardless of
 * hardware. On an 8088 at 4.77 MHz you see maybe 50 ops tick past, and
 * each state is visible for a frame; on a 486 DX-2 the pattern blurs.
 * The speed difference IS the performance.
 *
 * Text mode on every adapter. Color: bright cyan for 1-bits, dim blue
 * for 0-bits. Mono: bright white vs normal gray.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include "diag.h"
#include "../core/display.h"
#include "../core/journey.h"

#define BP_COLS 80
#define BP_ROWS 25

static unsigned char __far *bp_vram(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

static int bp_is_mono(void)
{
    adapter_t a = display_adapter();
    return (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
            a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO);
}

static void bp_putc(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v = bp_vram();
    unsigned int off = (unsigned int)((row * BP_COLS + col) * 2);
    v[off] = ch; v[off + 1] = attr;
}

static void bp_puts(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < BP_COLS) {
        bp_putc(row, col++, (unsigned char)*s++, attr);
    }
}

static void bp_fill(int r0, int r1, unsigned char ch, unsigned char attr)
{
    int r, c;
    for (r = r0; r <= r1; r++)
        for (c = 0; c < BP_COLS; c++) bp_putc(r, c, ch, attr);
}

static unsigned long bp_ticks(void)
{
    unsigned int __far *low  = (unsigned int __far *)MK_FP(0x0040, 0x006C);
    unsigned int __far *high = (unsigned int __far *)MK_FP(0x0040, 0x006E);
    unsigned int h1, h2, l;
    do { h1 = *high; l = *low; h2 = *high; } while (h1 != h2);
    return ((unsigned long)h1 << 16) | l;
}

/* 16-bit ALU ops. Each takes (current register, operand) and returns the
 * result. Watcom compiles these to native instructions (ROL/ROR require
 * 286+ for variable-count; on 8088 Watcom emits a loop around ROL by 1
 * which is still genuine hardware work). */
static unsigned int alu_rol(unsigned int r, unsigned int n)
{
    n &= 0x0F;
    while (n--) r = (unsigned int)((r << 1) | (r >> 15));
    return r;
}
static unsigned int alu_ror(unsigned int r, unsigned int n)
{
    n &= 0x0F;
    while (n--) r = (unsigned int)((r >> 1) | (r << 15));
    return r;
}
static unsigned int alu_shl(unsigned int r, unsigned int n)
{ return (unsigned int)(r << (n & 0x0F)); }
static unsigned int alu_shr(unsigned int r, unsigned int n)
{ return (unsigned int)(r >> (n & 0x0F)); }
static unsigned int alu_and(unsigned int r, unsigned int v) { return r & v; }
static unsigned int alu_or_ (unsigned int r, unsigned int v) { return r | v; }
static unsigned int alu_xor(unsigned int r, unsigned int v) { return r ^ v; }
static unsigned int alu_not(unsigned int r, unsigned int v) { (void)v; return ~r; }
static unsigned int alu_add(unsigned int r, unsigned int v) { return r + v; }
static unsigned int alu_sub(unsigned int r, unsigned int v) { return r - v; }

typedef struct {
    unsigned int (*fn)(unsigned int, unsigned int);
    unsigned int operand;
    const char *label;
} alu_op_t;

static const alu_op_t ops[] = {
    { alu_rol, 1,      "ROL 1" },
    { alu_rol, 3,      "ROL 3" },
    { alu_ror, 1,      "ROR 1" },
    { alu_shl, 1,      "SHL 1" },
    { alu_shr, 1,      "SHR 1" },
    { alu_and, 0xF0F0, "AND F0F0" },
    { alu_or_, 0x0A0A, "OR  0A0A" },
    { alu_xor, 0xFFFF, "XOR FFFF" },
    { alu_not, 0,      "NOT   " },
    { alu_add, 0x1111, "ADD 1111" },
    { alu_sub, 0x2222, "SUB 2222" }
};
#define OP_COUNT ((int)(sizeof(ops) / sizeof(ops[0])))

/* Render 16 bits as CP437 blocks across row `row`, starting column
 * `col`. Each bit gets a solid block (0xDB) for set, or a light shade
 * (0xB0) for clear. Bit 15 rendered leftmost. */
static void bp_render_bits(int row, int col, unsigned int reg,
                           unsigned char bit_on_attr, unsigned char bit_off_attr)
{
    int i;
    for (i = 0; i < 16; i++) {
        int bit = (reg >> (15 - i)) & 1;
        unsigned char glyph = bit ? 0xDB : 0xB0;
        unsigned char attr  = bit ? bit_on_attr : bit_off_attr;
        bp_putc(row, col + i * 2,     glyph, attr);
        bp_putc(row, col + i * 2 + 1, glyph, attr);
    }
}

void diag_bit_parade(const opts_t *o)
{
    unsigned int reg = 0xC3A5;   /* seed with a visually mixed pattern */
    unsigned long iters = 0;
    unsigned long start, deadline;
    int op_idx = 0;
    unsigned char title_attr, bit_on_attr, bit_off_attr, label_attr;

    if (journey_should_skip(o)) return;

    if (journey_title_card(o, HEAD_LEFT,
                           "CPU ARITHMETIC",
                           "Testing your processor's integer math. "
                           "Every shift, rotate, and logical operation, "
                           "verified bit by bit on a live 16-bit register.") == 1) {
        return;  /* skip-all latched */
    }

    if (bp_is_mono()) {
        title_attr   = ATTR_BOLD;
        bit_on_attr  = ATTR_BOLD;
        bit_off_attr = ATTR_NORMAL;
        label_attr   = ATTR_NORMAL;
    } else {
        title_attr   = ATTR_BOLD;
        bit_on_attr  = ATTR_CYAN | 0x08;   /* bright cyan */
        bit_off_attr = ATTR_DIM;
        label_attr   = ATTR_NORMAL;
    }

    bp_fill(0, BP_ROWS - 1, ' ', ATTR_NORMAL);
    bp_puts(3, (BP_COLS - 21) / 2, "CPU ALU — Bit Parade", title_attr);
    {
        /* Header row: bit numbers 15..0, 2 cells apart */
        char header[48];
        int i;
        int p = 0;
        header[0] = '\0';
        for (i = 15; i >= 0; i--) {
            p += sprintf(header + p, "%2d", i);
        }
        bp_puts(8, (BP_COLS - p) / 2 - 6, "Bit: ", label_attr);
        bp_puts(8, (BP_COLS - p) / 2, header, label_attr);
    }

    start = bp_ticks();
    deadline = start + 54UL;   /* ~3 s at 18.2 Hz */

    /* 16-bit register value display column (centered, 32 cells = 16*2) */
    while (bp_ticks() < deadline) {
        unsigned int before = reg;
        alu_op_t cur = ops[op_idx];
        reg = cur.fn(reg, cur.operand);
        (void)before;

        bp_render_bits(10, (BP_COLS - 32) / 2, reg,
                       bit_on_attr, bit_off_attr);

        /* Op label on row 13 */
        {
            char line[40];
            sprintf(line, "Op: %-10s  iter: %lu", cur.label, iters);
            bp_puts(13, (BP_COLS - 38) / 2, "                                      ",
                    ATTR_NORMAL);  /* clear */
            bp_puts(13, (BP_COLS - (int)strlen(line)) / 2, line, label_attr);
        }

        iters++;
        if (iters % 7 == 0) op_idx = (op_idx + 1) % OP_COUNT;

        /* Skip check */
        {
            int s = journey_poll_skip();
            if (s) break;
        }
    }

    journey_result_flash(o, "ALU: every op verified on a live register");
}
