/*
 * CPU class detection — Phase 1 Task 1.1.
 *
 * Layered probe from oldest-instruction-safe to newest:
 *   1. FLAGS bits 12-15 test    — 8086/8088 vs 286+
 *   2. PUSHFD under INT 6 guard — 286 vs 386+
 *   3. AC flag toggle            — 386 vs 486 (needed when CPUID absent)
 *   4. ID flag toggle            — 486-with-CPUID vs earlier
 *
 * The INT 6 handler is NASM-side (cpu_a.asm); we install it via DOS
 * INT 21h AH=25h (Watcom's `_dos_setvect`) before PUSHFD, and restore
 * the old vector immediately after. Scope is tight — the handler is
 * only live for the duration of a single probe.
 *
 * CPUID extraction, vendor-string decode, and the cpu_db friendly-name
 * lookup land in Task 1.1a. This file reports detected=<class> at
 * env-clamped confidence and defers the rich identification.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include "detect.h"
#include "env.h"
#include "../core/report.h"

/* --- NASM probe declarations — Watcom register convention, far calls --- */
extern int  cpu_asm_flags_test(void);
extern int  cpu_asm_pushfd_test(void);
extern int  cpu_asm_ac_test(void);
extern int  cpu_asm_id_test(void);
extern int  cpu_asm_int6_fired(void);
extern void cpu_asm_int6_clear(void);
extern void __far cpu_asm_int6_handler(void);

#pragma aux cpu_asm_flags_test     "cpu_asm_flags_test_"     modify exact [ax];
#pragma aux cpu_asm_pushfd_test    "cpu_asm_pushfd_test_"    modify exact [ax];
#pragma aux cpu_asm_ac_test        "cpu_asm_ac_test_"        modify exact [ax];
#pragma aux cpu_asm_id_test        "cpu_asm_id_test_"        modify exact [ax];
#pragma aux cpu_asm_int6_fired     "cpu_asm_int6_fired_"     modify exact [ax];
#pragma aux cpu_asm_int6_clear     "cpu_asm_int6_clear_"     modify exact [ax];
#pragma aux cpu_asm_int6_handler   "cpu_asm_int6_handler_";

typedef enum {
    CPU_CLASS_UNKNOWN = 0,
    CPU_CLASS_8086,          /* 8086 / 8088 / V20 / V30 */
    CPU_CLASS_286,
    CPU_CLASS_386,
    CPU_CLASS_486_NOCPUID,   /* 486 without CPUID — early i486DX-25/33 */
    CPU_CLASS_CPUID          /* CPUID available — 486+late / Pentium+ */
} cpu_class_t;

static const char *class_name(cpu_class_t c)
{
    switch (c) {
        case CPU_CLASS_8086:        return "8086";
        case CPU_CLASS_286:         return "286";
        case CPU_CLASS_386:         return "386";
        case CPU_CLASS_486_NOCPUID: return "486-no-cpuid";
        case CPU_CLASS_CPUID:       return "cpuid-capable";
        default:                    return "unknown";
    }
}

static const char *class_detected_string(cpu_class_t c)
{
    /* Human-readable family. Refined to model/stepping when cpu_db lands
     * in Task 1.1a. */
    switch (c) {
        case CPU_CLASS_8086:        return "8086/V20-class";
        case CPU_CLASS_286:         return "80286";
        case CPU_CLASS_386:         return "80386";
        case CPU_CLASS_486_NOCPUID: return "80486 (no CPUID)";
        case CPU_CLASS_CPUID:       return "CPUID-capable";
        default:                    return "unknown";
    }
}

/* ----------------------------------------------------------------------- */
/* INT 6 handler install / restore                                          */
/* ----------------------------------------------------------------------- */

static void (__interrupt __far *saved_int6)(void);

static void install_int6(void)
{
    saved_int6 = _dos_getvect(6);
    _dos_setvect(6, (void (__interrupt __far *)(void))cpu_asm_int6_handler);
}

static void restore_int6(void)
{
    _dos_setvect(6, saved_int6);
}

/* ----------------------------------------------------------------------- */
/* Orchestration                                                            */
/* ----------------------------------------------------------------------- */

static cpu_class_t probe_class(void)
{
    if (!cpu_asm_flags_test()) {
        return CPU_CLASS_8086;
    }

    /* 286+ confirmed. Test for 386+ via PUSHFD under INT 6 guard. */
    install_int6();
    cpu_asm_int6_clear();
    if (!cpu_asm_pushfd_test()) {
        restore_int6();
        return CPU_CLASS_286;
    }
    /* PUSHFD worked — 386+ is safe to probe further without INT 6 */
    restore_int6();

    /* AC flag toggle tells us 486+ vs 386 */
    if (!cpu_asm_ac_test()) {
        return CPU_CLASS_386;
    }

    /* 486+ — see if CPUID is there (some early 486 parts lack it) */
    if (cpu_asm_id_test()) {
        return CPU_CLASS_CPUID;
    }
    return CPU_CLASS_486_NOCPUID;
}

void detect_cpu(result_table_t *t, const opts_t *o)
{
    cpu_class_t c;
    (void)o;  /* /NOCYRIX gating lands with Cyrix probe in Task 1.1a */

    c = probe_class();

    report_add_str(t, "cpu.class",    class_name(c),
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    report_add_str(t, "cpu.detected", class_detected_string(c),
                   env_clamp(c == CPU_CLASS_CPUID ? CONF_MEDIUM : CONF_HIGH),
                   VERDICT_UNKNOWN);
    /* Cpu.detected is MEDIUM when CPUID is available because the FULL
     * identity (Intel/AMD/Cyrix, family/model/stepping, friendly name)
     * lands in Task 1.1a. For v0.1.2 without the DB, "CPUID-capable" is
     * an acceptable approximation at MEDIUM confidence. */
}
