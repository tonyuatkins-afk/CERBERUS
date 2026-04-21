/*
 * FPU Lissajous curve visual — v0.6.0 T3.
 *
 * Fires after diag_fpu on FPU-equipped VGA machines. Draws the
 * parametric curve x = sin(3t + pi/4), y = sin(2t) pixel by pixel as
 * the FPU computes each point. A working FPU produces a smooth,
 * symmetric 3:2 figure; a broken or miscomputing FPU shows visible
 * distortion.
 *
 * Genuine x87 math: each point is one sin(3t+phase), one sin(2t). No
 * lookup table. Watcom with -fpi inlines to FSIN; -oi keeps the call
 * chain tight.
 *
 * Gated on VGA-capable adapter (VGA_COLOR / MCGA) and FPU presence.
 * Non-VGA or no-FPU adapters silently skip.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <dos.h>
#include <conio.h>
#include "diag.h"
#include "../core/display.h"
#include "../core/journey.h"

#define LSJ_WIDTH  320
#define LSJ_HEIGHT 200
#define LSJ_CX     160
#define LSJ_CY     100
#define LSJ_R      80     /* amplitude */
#define LSJ_STEPS  1800

static void lsj_set_mode(int mode)
{
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = (unsigned char)mode;
    int86(0x10, &r, &r);
}

/* Amber oscilloscope palette: entry 0 black (background), entry 1
 * glowing amber for the trace. 6-bit DAC values. */
static void lsj_program_palette(void)
{
    outp(0x3C8, 0);
    outp(0x3C9, 0);  outp(0x3C9, 0);  outp(0x3C9, 0);
    outp(0x3C9, 63); outp(0x3C9, 40); outp(0x3C9, 0);
}

static void lsj_set_pixel(int x, int y, unsigned char color)
{
    unsigned char __far *vram = (unsigned char __far *)MK_FP(0xA000, 0x0000);
    unsigned int offset;
    if (x < 0 || x >= LSJ_WIDTH || y < 0 || y >= LSJ_HEIGHT) return;
    offset = (unsigned int)y * LSJ_WIDTH + (unsigned int)x;
    vram[offset] = color;
}

/* Caller-gates on FPU presence; this module only adds the VGA check. */
static int fpu_present(const result_table_t *t)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, "fpu.detected") == 0) {
            if (t->results[i].type != V_STR || !t->results[i].v.s) return 0;
            return strcmp(t->results[i].v.s, "none") != 0;
        }
    }
    return 0;
}

void diag_lissajous(const result_table_t *t, const opts_t *o)
{
    adapter_t adapter;
    double two_pi  = 6.283185307179586;
    double phase   = 0.7853981633974483;  /* pi/4 */
    double dt, t_val;
    int i;

    if (journey_should_skip(o)) return;
    if (!fpu_present(t))        return;

    adapter = display_adapter();
    if (adapter != ADAPTER_VGA_COLOR && adapter != ADAPTER_MCGA) return;

    if (journey_title_card(o, HEAD_CENTER,
                           "FPU PRECISION",
                           "Testing your math coprocessor with "
                           "trigonometric functions. Smooth curves "
                           "mean accurate math.") == 1) return;

    lsj_set_mode(0x13);
    lsj_program_palette();

    dt = two_pi / (double)LSJ_STEPS;
    for (i = 0; i < LSJ_STEPS; i++) {
        double x, y;
        int px, py;
        t_val = dt * (double)i;
        x = sin(3.0 * t_val + phase);
        y = sin(2.0 * t_val);
        px = LSJ_CX + (int)(x * (double)LSJ_R);
        py = LSJ_CY + (int)(y * (double)LSJ_R);
        lsj_set_pixel(px, py, 1);

        /* Skip check every 64 steps */
        if ((i & 63) == 0) {
            int s = journey_poll_skip();
            if (s) break;
        }
    }

    /* Brief pause then a non-blocking keypress gate */
    {
        union REGS r;
        r.h.ah = 0x00;
        int86(0x16, &r, &r);
    }

    lsj_set_mode(0x03);
}
