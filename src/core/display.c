/*
 * CERBERUS display abstraction
 *
 * Text-mode detection and primitives covering MDA, CGA, Hercules, EGA, and
 * VGA. Detection is a newest-to-oldest waterfall per MS-DOS UI-UX research
 * Part B adapter-detection guidance:
 *
 *   1. INT 10h AH=1Ah        VGA/MCGA self-report (AL=1Ah on success)
 *   2. INT 10h AH=12h BL=10h  EGA info (BH changes from FFh sentinel)
 *   3. BDA equipment flag at 0040:0010h bits 4-5 (CGA/MDA distinguisher)
 *      NOTE: functionally equivalent to INT 11h AH=00h; direct BDA read
 *      avoids the BIOS call overhead and has the same data source.
 *   4. Port 3BAh bit 7 toggle (vsync on HGC, fixed on MDA) distinguishes
 *      Hercules from MDA.
 *
 * Output uses BIOS INT 10h AH=0Eh (teletype) for the portable path. Direct
 * VRAM writes live in tui_util.c and ui.c. CGA snow-avoidance via
 * tui_wait_cga_retrace_edge() synchronizes writes to the retrace edge
 * (v0.8.0-M3.1).
 *
 * v0.8.0-M3.5: display_set_force_mono() + display_is_mono() unify the
 * mono vs color attribute-mapping branch across all rendering code
 * paths so the /MONO command-line flag propagates cleanly without
 * adapter-specific conditionals sprinkled throughout callers.
 *
 * v0.8.0-M3.6: display_enable_16bg_colors() runs INT 10h AX=1003h BL=00h
 * during display_init() on EGA/VGA to get 16 background colors
 * (bg-intensity replaces blink-enable on the attribute byte's bit 7).
 */

#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
#include "display.h"
#include "../cerberus.h"

static adapter_t     current_adapter = ADAPTER_UNKNOWN;
static unsigned char current_attr    = ATTR_NORMAL;
static int           force_mono_flag = 0;  /* M3.5: /MONO forced-mono bit */

/* ----------------------------------------------------------------------- */
/* Adapter detection                                                        */
/* ----------------------------------------------------------------------- */

static int probe_vga(adapter_t *out)
{
    union REGS r;
    r.h.ah = 0x1A;
    r.h.al = 0x00;
    r.x.bx = 0;
    int86(0x10, &r, &r);
    if (r.h.al != 0x1A) return 0;
    /* BL: 01=MDA, 02=CGA, 04=EGA color, 05=EGA mono,
     *     07=VGA mono, 08=VGA color, 0A=MCGA color, 0B=MCGA mono, 0C=MCGA color */
    switch (r.h.bl) {
        case 0x07: *out = ADAPTER_VGA_MONO;  return 1;
        case 0x08: *out = ADAPTER_VGA_COLOR; return 1;
        case 0x0A:
        case 0x0C: *out = ADAPTER_MCGA;      return 1;
        default:
            /* AH=1A succeeded but BL is an older class — treat as VGA color */
            *out = ADAPTER_VGA_COLOR;
            return 1;
    }
}

static int probe_ega(adapter_t *out)
{
    union REGS r;
    r.h.ah = 0x12;
    r.h.bl = 0x10;  /* get EGA info */
    r.h.bh = 0xFF;  /* sentinel */
    r.h.cl = 0xFF;
    int86(0x10, &r, &r);
    if (r.h.bl == 0x10) return 0;  /* BL unchanged => not EGA */
    /* BH: 00 = color (5150/5153), 01 = mono (5151) */
    *out = (r.h.bh == 0x01) ? ADAPTER_EGA_MONO : ADAPTER_EGA_COLOR;
    return 1;
}

static int probe_hercules(void)
{
    /* MDA/HGC status register at 3BAh:
     *   bit 7 = vertical sync (HGC only — MDA holds this fixed)
     * Poll for transitions. If bit 7 ever toggles, it's HGC.
     */
    unsigned int  i;
    unsigned char first = (unsigned char)(inp(0x3BA) & 0x80);
    for (i = 0; i < 32000U; i++) {
        if ((unsigned char)(inp(0x3BA) & 0x80) != first) {
            return 1;
        }
    }
    return 0;
}

static adapter_t probe_bda_cga_mda(void)
{
    /* BDA 0040:0010h equipment flag, bits 5:4:
     *   00 = EGA/VGA (shouldn't reach here — caught above)
     *   01 = 40x25 CGA
     *   10 = 80x25 CGA
     *   11 = 80x25 MDA
     */
    unsigned char __far *bda_equipment = (unsigned char __far *)MK_FP(0x0040, 0x0010);
    unsigned char eq = *bda_equipment;
    unsigned char mode_bits = (unsigned char)((eq >> 4) & 0x03);
    switch (mode_bits) {
        case 0x01:
        case 0x02:
            return ADAPTER_CGA;
        case 0x03:
            return probe_hercules() ? ADAPTER_HERCULES : ADAPTER_MDA;
        default:
            return ADAPTER_UNKNOWN;
    }
}

static adapter_t detect_adapter(void)
{
    adapter_t a;
    if (probe_vga(&a)) return a;
    if (probe_ega(&a)) return a;
    return probe_bda_cga_mda();
}

/* ----------------------------------------------------------------------- */
/* Primitives                                                               */
/* ----------------------------------------------------------------------- */

void display_putc(char c)
{
    union REGS r;
    r.h.ah = 0x0E;
    r.h.al = (unsigned char)c;
    r.h.bh = 0;              /* page 0 */
    r.h.bl = current_attr;   /* foreground in graphics modes; ignored in text */
    int86(0x10, &r, &r);
}

void display_puts(const char *s)
{
    while (*s) display_putc(*s++);
}

void display_goto(int row, int col)
{
    union REGS r;
    r.h.ah = 0x02;
    r.h.bh = 0;
    r.h.dh = (unsigned char)row;
    r.h.dl = (unsigned char)col;
    int86(0x10, &r, &r);
}

void display_set_attr(unsigned char attr)
{
    current_attr = attr;
}

void display_wait_retrace(void)
{
    /* Only meaningful on CGA — MDA/EGA/VGA already synchronize via BIOS.
     * On CGA, poll port 3DAh bit 0: 1 = in retrace, 0 = display active. */
    if (current_adapter != ADAPTER_CGA) return;
    /* Wait for any in-progress retrace to finish, then wait for next one */
    while (inp(0x3DA) & 0x01) { /* in retrace */ }
    while (!(inp(0x3DA) & 0x01)) { /* display active */ }
}

/* ----------------------------------------------------------------------- */
/* Box drawing                                                              */
/* ----------------------------------------------------------------------- */

static void box_impl(int row, int col, int w, int h,
                     unsigned char hc, unsigned char vc,
                     unsigned char tl, unsigned char tr,
                     unsigned char bl, unsigned char br)
{
    int i;
    if (w < 2 || h < 2) return;
    display_goto(row, col);
    display_putc((char)tl);
    for (i = 0; i < w - 2; i++) display_putc((char)hc);
    display_putc((char)tr);
    for (i = 1; i < h - 1; i++) {
        display_goto(row + i, col);
        display_putc((char)vc);
        display_goto(row + i, col + w - 1);
        display_putc((char)vc);
    }
    display_goto(row + h - 1, col);
    display_putc((char)bl);
    for (i = 0; i < w - 2; i++) display_putc((char)hc);
    display_putc((char)br);
}

void display_box(int row, int col, int w, int h)
{
    box_impl(row, col, w, h,
             CP437_HORIZ, CP437_VERT,
             CP437_TL, CP437_TR, CP437_BL, CP437_BR);
}

void display_box_double(int row, int col, int w, int h)
{
    box_impl(row, col, w, h,
             CP437_DBL_HORIZ, CP437_DBL_VERT,
             CP437_DBL_TL, CP437_DBL_TR, CP437_DBL_BL, CP437_DBL_BR);
}

/* ----------------------------------------------------------------------- */
/* Lifecycle + banner                                                       */
/* ----------------------------------------------------------------------- */

const char *display_adapter_name(adapter_t a)
{
    switch (a) {
        case ADAPTER_MDA:       return "MDA";
        case ADAPTER_CGA:       return "CGA";
        case ADAPTER_HERCULES:  return "Hercules";
        case ADAPTER_EGA_MONO:  return "EGA (mono)";
        case ADAPTER_EGA_COLOR: return "EGA";
        case ADAPTER_VGA_MONO:  return "VGA (mono)";
        case ADAPTER_VGA_COLOR: return "VGA";
        case ADAPTER_MCGA:      return "MCGA";
        default:                return "unknown";
    }
}

int display_has_color(void)
{
    switch (current_adapter) {
        case ADAPTER_CGA:
        case ADAPTER_EGA_COLOR:
        case ADAPTER_VGA_COLOR:
        case ADAPTER_MCGA:
            return 1;
        default:
            return 0;
    }
}

adapter_t display_adapter(void)
{
    return current_adapter;
}

void display_set_force_mono(int force)
{
    force_mono_flag = force ? 1 : 0;
}

int display_is_mono(void)
{
    /* /MONO wins over detected adapter. Otherwise fall through to the
     * adapter's own monochrome-tier classification. */
    if (force_mono_flag) return 1;
    switch (current_adapter) {
        case ADAPTER_MDA:
        case ADAPTER_HERCULES:
        case ADAPTER_EGA_MONO:
        case ADAPTER_VGA_MONO:
            return 1;
        default:
            return 0;
    }
}

/* M3.6: INT 10h AX=1003h BL=00h switches attribute-byte bit 7 from
 * blink-enable to background-intensity on EGA/VGA. Result: 16
 * background colors (0..F) are available instead of 8 with blink.
 *
 * No-op on pre-EGA adapters (MDA/CGA/Hercules): the BIOS call is
 * EGA+ only; older adapters ignore it. Also no-op when /MONO is
 * forced (renderers use mono attr set which doesn't need this).
 *
 * Called from display_init() after adapter detection. */
void display_enable_16bg_colors(void)
{
    union REGS r;
    if (force_mono_flag) return;
    switch (current_adapter) {
        case ADAPTER_EGA_COLOR:
        case ADAPTER_VGA_COLOR:
        case ADAPTER_MCGA:
            break;
        default:
            return;  /* MDA/CGA/Hercules/EGA_MONO/VGA_MONO: skip */
    }
    r.h.ah = 0x10;
    r.h.al = 0x03;
    r.h.bl = 0x00;  /* 0 = background intensity, 1 = blink */
    r.h.bh = 0;
    int86(0x10, &r, &r);
}

void display_init(void)
{
    current_adapter = detect_adapter();
    current_attr = ATTR_NORMAL;
    display_enable_16bg_colors();
}

/* ----------------------------------------------------------------------- */
/* v0.8.1 M3.3: Hercules variant discrimination                              */
/* ----------------------------------------------------------------------- */

/* Pure classifier + token mapper live in a separate include so host
 * tests can exercise them without pulling in display.c's DOS-only
 * conio/dos/i86 dependencies. Guarded so display.c is the sole
 * inclusion site in the target build. */
#include "display_hercules_ids.c"

hercules_variant_t display_hercules_variant(void)
{
    unsigned char status, id_bits;
    unsigned int  i;

    if (current_adapter != ADAPTER_HERCULES) return HERCULES_VARIANT_NA;

    /* Sample 3BAh outside the vertical-retrace window so bit 7 doesn't
     * skew the ID bits. Wait for retrace to start then end, then read
     * mid-display. If we time out without seeing a full retrace cycle,
     * fall through to read whatever is there and let the classifier
     * bucket it as UNKNOWN. */
    for (i = 0; i < 32000U; i++) {
        if (inp(0x3BA) & 0x80) break;
    }
    for (i = 0; i < 32000U; i++) {
        if (!(inp(0x3BA) & 0x80)) break;
    }
    status  = (unsigned char)inp(0x3BA);
    id_bits = (unsigned char)((status >> 4) & 0x07);
    return display_classify_hercules_id(id_bits);
}

void display_shutdown(void)
{
    /* Reset attribute so we don't leave the terminal colored after exit */
    current_attr = ATTR_NORMAL;
}

void display_banner(void)
{
    printf("CERBERUS %s - Retro PC System Intelligence\n", CERBERUS_VERSION);
    printf("(c) 2026 Tony Atkins / Barely Booting - MIT License\n");
    printf("Display: %s%s\n",
           display_adapter_name(current_adapter),
           display_has_color() ? " (color)" : " (mono)");
    printf("\n");
}
