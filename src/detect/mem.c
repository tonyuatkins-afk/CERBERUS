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
#include "../core/crumb.h"

/* Value-display buffers per emitted key. report_add_u32 stores the
 * `display` pointer verbatim (see report.c:61 and the lifetime note in
 * report.h). INI writing and UI rendering happen long after detect_mem
 * returns, so a stack-local sprintf target would dangle. Every key that
 * formats a dynamic display string gets its own dedicated file-scope
 * static below — NEVER share one buffer across keys, because the second
 * sprintf would silently clobber the first key's stored pointer.
 *
 * Critical: memory.conventional_kb and memory.extended_kb are canonical
 * signature keys (see report_hardware_signature). Dangling bytes here
 * mean the hardware identity hash varies run-to-run on the same
 * machine — the R6 systemic fatal this block resolves. */
static char mem_conv_val[12];       /* "65535" max */
static char mem_ext_val[12];        /* "4294967295" max — unsigned long */
static char mem_ems_total_val[12];
static char mem_ems_free_val[12];

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

/* XMS extended-memory query (see mem_a.asm). Returns 1 and populates
 * *out_largest_kb / *out_total_kb on success; returns 0 if XMS is
 * absent or the driver reports no free memory. */
extern int xms_query_free(unsigned long *out_largest_kb,
                          unsigned long *out_total_kb);
#pragma aux xms_query_free "xms_query_free_" \
    parm [ax] [dx] \
    value [ax] \
    modify exact [ax bx cx dx es];

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

/* Validate that INT 67h points at a real EMS driver before calling it.
 * DOS convention (LIM EMS 3.2+): a genuine EMS driver installs a device-
 * driver header at the INT 67h vector target; offset 10 contains the
 * literal string "EMMXXXX0" (the driver's device name). If the vector
 * points at a random uninitialized memory region — the failure mode
 * observed on the 486DX2-66 test bed whose DOS 6.22 has no EMS driver
 * loaded — calling INT 67h executes garbage bytes and hangs the
 * machine. Returns 1 if the EMMXXXX0 signature is present, 0 otherwise.
 *
 * Reference: LIM EMS 4.0 spec, section 3.1 "Device driver header". */
static int ems_driver_present(void)
{
    unsigned long vec;
    unsigned int  seg, off;
    const char __far *sig;
    static const char expected[] = "EMMXXXX0";
    int i;

    _disable();
    vec = (unsigned long)_dos_getvect(0x67);
    _enable();
    seg = (unsigned int)(vec >> 16);
    off = (unsigned int)(vec & 0xFFFFU);

    /* Null / obviously-uninitialized vectors — bail. */
    if (seg == 0 && off == 0) return 0;
    if (seg == 0xFFFF) return 0;

    /* Device driver header's device-name field lives at offset 10
     * from the vector target, 8 bytes long. */
    sig = (const char __far *)MK_FP(seg, off + 10);
    for (i = 0; i < 8; i++) {
        if (sig[i] != expected[i]) return 0;
    }
    return 1;
}

static int probe_ems(unsigned long *out_total_pages, unsigned long *out_free_pages)
{
    union REGS r;

    /* Hard gate: if no EMS driver is loaded (the common case on
     * DOS 6.22 with HIMEM only, no EMM386), INT 67h may point at
     * uninitialized memory and calling it will hang. The EMMXXXX0
     * signature is the DOS-documented way to detect a real driver. */
    if (!ems_driver_present()) return 0;

    /* Sub-probe crumbs: each crumb_enter OVERWRITES the file (O_TRUNC)
     * with the more-specific name. We deliberately do NOT call
     * crumb_exit between sub-probes — that would delete the file and
     * leave a diagnostic gap if a hang occurred in C code between
     * INT calls. The outer WRAP_DETECT("mem", ...) in detect_all.c
     * owns the paired exit that cleans up when detect_mem returns. */

    /* INT 2Fh AX=5300h — EMM host probe */
    crumb_enter("detect.mem.ems.host");
    r.x.ax = 0x5300;
    r.x.bx = 0;
    int86(0x2F, &r, &r);
    /* BX returns handle 0000h on OK; AH=80h on no EMS. The response is
     * somewhat driver-dependent; most emulators implement this. */

    /* INT 67h AH=40h — get EMM status */
    crumb_enter("detect.mem.ems.status");
    r.h.ah = 0x40;
    int86(0x67, &r, &r);
    if (r.h.ah != 0x00) return 0;

    /* INT 67h AH=42h — get page counts: BX=free, DX=total */
    crumb_enter("detect.mem.ems.pages");
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

    /* Sub-probe crumbs: each BIOS INT gets its own crumb so a hang
     * narrows to the specific INT call rather than the whole function.
     * Task 1.10 validation on the 486DX2-66 bench box hung inside
     * detect.mem and the coarse "detect.mem" crumb left us guessing
     * which INT was the culprit. Sub-crumbs remove that ambiguity.
     *
     * We deliberately do NOT call crumb_exit between sub-probes —
     * that would erase the file and leave a diagnostic gap if a hang
     * occurred in C code between the INT calls. crumb_enter uses
     * O_TRUNC so each call just overwrites with the more-specific
     * name. The outer WRAP_DETECT("mem", ...) in detect_all owns the
     * paired exit that cleans up when detect_mem returns. */
    crumb_enter("detect.mem.int12");
    conv_kb = probe_conventional_kb();
    sprintf(mem_conv_val, "%u", conv_kb);
    report_add_u32(t, "memory.conventional_kb", (unsigned long)conv_kb,
                   mem_conv_val, env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    /* AH=88h is safe on 286+ (requires protected mode services the 8086
     * can't support). Gate accordingly. */
    if (cls >= CPU_CLASS_286) {
        crumb_enter("detect.mem.ah88");
        ext_kb = probe_ah88_kb();
    }

    /* E801h is safe to attempt on 386+ — it's the Phoenix/Compaq
     * extension. Many late-486 and most Pentium BIOSes implement it;
     * not hardcoding which, just trying and honoring CF. */
    if (cls >= CPU_CLASS_386) {
        crumb_enter("detect.mem.e801");
        have_e801 = probe_e801_kb(&e801_kb);
    }

    /* XMS detection (presence check only for now). Runs BEFORE the
     * extended_kb emit so we can choose the XMS value over the HIMEM-
     * blinded INT 15h value. */
    crumb_enter("detect.mem.xms");
    have_xms = probe_xms(&xms_ver);

    /* HIMEM.SYS hooks INT 15h AH=88h / AX=E801h and returns 0 extended
     * memory to steer DOS clients through XMS. Our INT 15h probes
     * therefore report 0 whenever HIMEM is active (seen on the 486 DX-2
     * bench box with 64 MB RAM). Re-query via the XMS entry point
     * (mem_a.asm) and use it as the authoritative value when XMS is
     * loaded. Emitted exactly once below so we don't pollute the INI
     * with duplicate rows. */
    if (have_xms) {
        unsigned long xms_largest = 0, xms_total = 0;
        crumb_enter("detect.mem.xms_query");
        if (xms_query_free(&xms_largest, &xms_total) && xms_total > ext_kb) {
            ext_kb = xms_total;
            have_e801 = 0;           /* XMS supersedes the INT 15h probes */
            e801_disagreed = 0;
        }
    }

    {
        /* Pick max(AH=88h, E801h, XMS). If AH=88h and E801h disagree
         * significantly (but XMS didn't override), flag MEDIUM confidence
         * so consist can pick it up (AH=88h saturation at 64MB vs a
         * fuller E801h response is expected on systems with >64MB). */
        unsigned long reported = ext_kb;
        confidence_t conf = CONF_HIGH;
        if (have_e801 && e801_kb > reported) {
            if ((reported > 0) && (e801_kb > reported + 512UL)) {
                e801_disagreed = 1;
                conf = CONF_MEDIUM;
            }
            reported = e801_kb;
        }
        sprintf(mem_ext_val, "%lu", reported);
        report_add_u32(t, "memory.extended_kb", reported,
                       mem_ext_val, env_clamp(conf), VERDICT_UNKNOWN);
        if (e801_disagreed) {
            report_add_str(t, "memory.ext_probe_disagreed", "yes",
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        }
    }

    report_add_str(t, "memory.xms_present", have_xms ? "yes" : "no",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    /* EMS: probe_ems internally crumbs each INT sub-call. It also
     * gates the INT 67h calls behind a driver-signature check, so a
     * system with no EMS driver loaded (DOS 6.22 + HIMEM only) doesn't
     * execute whatever bytes happen to live at the uninitialized
     * INT 67h vector. */
    have_ems = probe_ems(&ems_total_pages, &ems_free_pages);
    report_add_str(t, "memory.ems_present", have_ems ? "yes" : "no",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    if (have_ems) {
        sprintf(mem_ems_total_val, "%lu", ems_total_pages);
        report_add_u32(t, "memory.ems_total_pages", ems_total_pages,
                       mem_ems_total_val, env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        sprintf(mem_ems_free_val, "%lu", ems_free_pages);
        report_add_u32(t, "memory.ems_free_pages", ems_free_pages,
                       mem_ems_free_val, env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    }
}
