/*
 * Mandelbrot FPU visual demo — v0.5.0 T4b.
 *
 * Fires at the end of bench_whetstone() on FPU-equipped VGA-capable
 * machines. Not timed, not part of any reported benchmark number; a
 * post-run visual coda that proves the FPU is live by working it
 * visibly. Genuine x87 arithmetic, not a pre-rendered lookup table.
 *
 * Render strategy: pixel-by-pixel, row-by-row, directly to VGA mode
 * 13h (320x200x256) linear framebuffer at A000:0000. Each pixel gets
 * one Mandelbrot iteration loop (z = z*z + c, bail at |z|^2 > 4 or
 * MAX_ITER). User sees the fractal emerge in real time on a 486,
 * which IS the "ooh ahh" moment.
 *
 * Gates:
 *   - FPU presence — the caller (bench_whetstone) already gated on
 *     fpu_looks_present(); we only run when dispatched from that
 *     context, so no redundant check here. An FPU-less machine that
 *     somehow reached this function would trap on the first FMUL.
 *   - Video adapter — VGA or MCGA only. CGA/EGA/MDA/Hercules return
 *     early without mode-switching, so the DOS prompt stays clean.
 *   - /NOUI — batch mode skips the visual entirely.
 *
 * Palette: port 0x3C8/0x3C9 DAC programming. 64 entries used,
 * classic blue → teal → white gradient. Entry 0 is black for pixels
 * inside the set (iter hit MAX_ITER).
 *
 * On completion: blocking INT 16h keypress, then restore text mode 3h.
 */

#include <stdio.h>
#include <dos.h>
#include <conio.h>
#include "bench.h"
#include "../core/display.h"
#include "../core/journey.h"

#define MB_WIDTH    320
#define MB_HEIGHT   200
#define MB_MAX_ITER 64

/* Coordinate window — classical Mandelbrot frame, slightly left-
 * centered to put the cardioid in the middle of the screen. */
#define MB_X_MIN (-2.0)
#define MB_X_MAX ( 0.8)
#define MB_Y_MIN (-1.2)
#define MB_Y_MAX ( 1.2)

static void mb_set_mode(int mode)
{
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = (unsigned char)mode;
    int86(0x10, &r, &r);
}

/* Program DAC entries 0..MB_MAX_ITER-1 with a smooth gradient.
 * Each component is 6-bit (0..63 range). Entry 0 is black, entries
 * 1..31 go dark blue → cyan, 32..63 go cyan → white. */
static void mb_program_palette(void)
{
    int i;

    /* Port 0x3C8 takes the starting palette index; 0x3C9 then
     * accepts R, G, B triples with the index auto-incrementing. */
    outp(0x3C8, 0);

    /* Entry 0: inside-the-set black. */
    outp(0x3C9, 0);
    outp(0x3C9, 0);
    outp(0x3C9, 0);

    for (i = 1; i < MB_MAX_ITER; i++) {
        unsigned char r, g, b;
        if (i < 32) {
            /* blue → cyan */
            r = 0;
            g = (unsigned char)(i * 2);
            b = (unsigned char)(32 + i);
        } else {
            /* cyan → white */
            r = (unsigned char)((i - 32) * 2);
            g = 63;
            b = 63;
        }
        outp(0x3C9, r);
        outp(0x3C9, g);
        outp(0x3C9, b);
    }
}

static void mb_set_pixel(int x, int y, unsigned char color)
{
    unsigned char __far *vram = (unsigned char __far *)MK_FP(0xA000, 0x0000);
    unsigned int offset = (unsigned int)y * MB_WIDTH + (unsigned int)x;
    vram[offset] = color;
}

/* One pixel's worth of Mandelbrot iteration. Returns iter count at
 * escape, or MB_MAX_ITER if the point is (likely) in the set. */
static int mb_iterate(double cr, double ci)
{
    double zr = 0.0, zi = 0.0;
    double zr2, zi2;
    int iter;
    for (iter = 0; iter < MB_MAX_ITER; iter++) {
        zr2 = zr * zr;
        zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) return iter;
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
    }
    return MB_MAX_ITER;
}

/* Entry. Caller must have verified FPU presence. Returns cleanly if
 * video adapter isn't VGA/MCGA or if /NOUI is active. */
void bench_mandelbrot_demo(const opts_t *o)
{
    adapter_t adapter;
    double cr, ci;
    double dx, dy;
    int px, py;
    unsigned char color;

    if (journey_should_skip(o)) return;

    adapter = display_adapter();
    if (adapter != ADAPTER_VGA_COLOR && adapter != ADAPTER_MCGA) return;

    /* v0.6.0 T8: no dedicated title card here — the FPU Benchmark
     * title card fires at the start of bench_whetstone, which then
     * runs the Whetstone measurement and dispatches to this Mandelbrot
     * visual. One section, one card. */

    dx = (MB_X_MAX - MB_X_MIN) / (double)MB_WIDTH;
    dy = (MB_Y_MAX - MB_Y_MIN) / (double)MB_HEIGHT;

    mb_set_mode(0x13);
    mb_program_palette();

    for (py = 0; py < MB_HEIGHT; py++) {
        ci = MB_Y_MIN + (double)py * dy;
        for (px = 0; px < MB_WIDTH; px++) {
            int iter;
            cr = MB_X_MIN + (double)px * dx;
            iter = mb_iterate(cr, ci);
            /* iter == MB_MAX_ITER → inside set → entry 0 (black).
             * Otherwise map iter directly to palette entry — since
             * MB_MAX_ITER == palette entry count, no scaling needed. */
            color = (iter == MB_MAX_ITER) ? 0 : (unsigned char)iter;
            mb_set_pixel(px, py, color);
        }
    }

    /* Block on keypress, then restore text mode. */
    {
        union REGS r;
        r.h.ah = 0x00;
        int86(0x16, &r, &r);
    }

    mb_set_mode(0x03);
}
