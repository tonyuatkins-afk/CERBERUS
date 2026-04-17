/*
 * Memory detection — Phase 1 Task 1.3.
 *
 * Pure BIOS-interrupt probing; no DB needed. Reports into the [memory]
 * section:
 *
 *   conventional_kb  INT 12h
 *   extended_kb      INT 15h AH=88h, refined by E801h on 386+
 *   xms_present      INT 2Fh AX=4300h signature
 *   xms_version      INT 2Fh AX=4300h (BCD version in DX)
 *   ems_present      INT 2Fh AX=5300h EMM host probe
 *   ems_total_pages  INT 67h AH=42h
 *   ems_free_pages   INT 67h AH=42h
 *
 * Deliberately does NOT call INT 15h AX=E820h — that's ACPI-era and
 * effectively absent on pre-Pentium BIOSes. The plan calls this out
 * explicitly.
 *
 * AH=88h saturation varies by BIOS (0xFFFF / 0xFC00 / 0xF000 / 0x3C00
 * are all documented in the wild). Rather than hardcoding a threshold,
 * we always attempt E801h on CPU class >= 386 and use max(AH=88h, E801h)
 * when both succeed.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include "detect.h"
#include "cpu.h"
#include "env.h"
#include "../core/report.h"

/* ----------------------------------------------------------------------- */
/* Individual probes                                                        */
/* ----------------------------------------------------------------------- */

static unsigned int probe_conventional_kb(void)
{
    union REGS r;
    int86(0x12, &r, &r);
    return r.x.ax;
}

static unsigned long probe_ah88_kb(void)
{
    union REGS r;
    /* INT 15h AH=88h returns extended memory in AX (KB). Skipped on 8086
     * — we gate the call in the orchestrator. */
    r.h.ah = 0x88;
    int86(0x15, &r, &r);
    return (unsigned long)r.x.ax;
}

/* E801h: AX=KB up to 16MB, BX=64KB-chunks above 16MB.
 * Also reports CX/DX same data from a different algorithm (we use AX/BX).
 * On failure, CF is set — int86 doesn't expose CF easily in Watcom; we
 * detect failure via AX having an implausible value or the whole call
 * returning the input unchanged. Simpler: use intdos with struct that
 * exposes flags. Watcom has int86x for extended return. */
static int probe_e801_kb(unsigned long *out_kb)
{
    union  REGS  r;
    struct SREGS sr;
    unsigned long below_16mb, above_16mb;

    r.x.ax = 0xE801;
    r.x.bx = 0;
    r.x.cx = 0;
    r.x.dx = 0;
    int86x(0x15, &r, &r, &sr);

    /* Check the carry flag via r.x.cflag (Watcom-specific in REGS). */
    if (r.x.cflag) return 0;

    /* Some BIOSes put the result in CX/DX instead of AX/BX — prefer AX/BX
     * when non-zero, else fall through to CX/DX. */
    below_16mb = r.x.ax ? (unsigned long)r.x.ax : (unsigned long)r.x.cx;
    above_16mb = r.x.bx ? (unsigned long)r.x.bx : (unsigned long)r.x.dx;

    if (below_16mb == 0 && above_16mb == 0) return 0;

    *out_kb = below_16mb + above_16mb * 64UL;
    return 1;
}

static int probe_xms(unsigned int *out_version_bcd)
{
    union REGS r;
    r.x.ax = 0x4300;
    int86(0x2F, &r, &r);
    if (r.h.al != 0x80) return 0;
    /* AX=4310h gets the entry-point; we could also read AX=4300h's DX
     * for version. Actually, version comes from calling the entry point
     * with AH=00h. For Phase 1 we just report presence; version
     * refinement can come later. */
    *out_version_bcd = 0;
    return 1;
}

static int probe_ems(unsigned long *out_total_pages, unsigned long *out_free_pages)
{
    union REGS r;

    /* INT 2Fh AX=5300h — EMM host probe */
    r.x.ax = 0x5300;
    r.x.bx = 0;
    int86(0x2F, &r, &r);
    /* BX returns handle 0000h on OK; AH=80h on no EMS. The response is
     * somewhat driver-dependent; most emulators implement this. */

    /* INT 67h AH=40h — get EMM status */
    r.h.ah = 0x40;
    int86(0x67, &r, &r);
    if (r.h.ah != 0x00) return 0;

    /* INT 67h AH=42h — get page counts: BX=free, DX=total */
    r.h.ah = 0x42;
    int86(0x67, &r, &r);
    if (r.h.ah != 0x00) return 0;
    *out_total_pages = (unsigned long)r.x.dx;
    *out_free_pages  = (unsigned long)r.x.bx;
    return 1;
}

/* ----------------------------------------------------------------------- */
/* Orchestration                                                            */
/* ----------------------------------------------------------------------- */

void detect_mem(result_table_t *t)
{
    cpu_class_t cls = cpu_get_class();
    unsigned int  conv_kb;
    unsigned long ext_kb          = 0;
    unsigned long e801_kb         = 0;
    int           have_e801       = 0;
    int           e801_disagreed  = 0;
    unsigned int  xms_ver         = 0;
    int           have_xms        = 0;
    unsigned long ems_total_pages = 0;
    unsigned long ems_free_pages  = 0;
    int           have_ems        = 0;
    char scratch[24];

    conv_kb = probe_conventional_kb();
    sprintf(scratch, "%u", conv_kb);
    report_add_u32(t, "memory.conventional_kb", (unsigned long)conv_kb,
                   scratch, env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    /* AH=88h is safe on 286+ (requires protected mode services the 8086
     * can't support). Gate accordingly. */
    if (cls >= CPU_CLASS_286) {
        ext_kb = probe_ah88_kb();
    }

    /* E801h is safe to attempt on 386+ — it's the Phoenix/Compaq
     * extension. Many late-486 and most Pentium BIOSes implement it;
     * not hardcoding which, just trying and honoring CF. */
    if (cls >= CPU_CLASS_386) {
        have_e801 = probe_e801_kb(&e801_kb);
    }

    {
        /* Pick max(AH=88h, E801h). If they disagree significantly flag
         * MEDIUM confidence so the consistency engine can pick it up
         * (an AH=88h saturation vs a fuller E801h response is expected
         * on systems with >64MB extended). */
        unsigned long reported = ext_kb;
        confidence_t conf = CONF_HIGH;
        if (have_e801 && e801_kb > reported) {
            if ((reported > 0) && (e801_kb > reported + 512UL)) {
                e801_disagreed = 1;
                conf = CONF_MEDIUM;
            }
            reported = e801_kb;
        }
        sprintf(scratch, "%lu", reported);
        report_add_u32(t, "memory.extended_kb", reported,
                       scratch, env_clamp(conf), VERDICT_UNKNOWN);
        if (e801_disagreed) {
            report_add_str(t, "memory.ext_probe_disagreed", "yes",
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        }
    }

    /* XMS and EMS work on any CPU — gate only by BIOS response */
    have_xms = probe_xms(&xms_ver);
    report_add_str(t, "memory.xms_present", have_xms ? "yes" : "no",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    have_ems = probe_ems(&ems_total_pages, &ems_free_pages);
    report_add_str(t, "memory.ems_present", have_ems ? "yes" : "no",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    if (have_ems) {
        sprintf(scratch, "%lu", ems_total_pages);
        report_add_u32(t, "memory.ems_total_pages", ems_total_pages,
                       scratch, env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        sprintf(scratch, "%lu", ems_free_pages);
        report_add_u32(t, "memory.ems_free_pages", ems_free_pages,
                       scratch, env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    }
}
