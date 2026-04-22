/*
 * CPU class detection — Phase 1 Task 1.1 + 1.1a.
 *
 * Layered probe from oldest-instruction-safe to newest, then on CPUID-
 * capable CPUs, execute CPUID and look up the detailed identification
 * in cpu_db (regenerated from hw_db/cpus.csv).
 *
 *   1. FLAGS bits 12-15 test    — 8086/8088 vs 286+
 *   2. PUSHFD under INT 6 guard — 286 vs 386+
 *   3. AC flag toggle            — 386 vs 486 (needed when CPUID absent)
 *   4. ID flag toggle            — 486-with-CPUID vs earlier
 *   5. CPUID leaf 0/1            — vendor string + family/model/stepping
 *   6. cpu_db_lookup_*           — friendly name + notes
 *
 * Cyrix DIR port-22h probe stays deferred until a follow-up — it's safely
 * gated behind /NOCYRIX and pre-Pentium class. NEC V20/V30 disambiguation
 * from 8086/8088 is also deferred (the TEST1 instruction probe needs
 * another INT 6 handler).
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include "detect.h"
#include "cpu.h"
#include "env.h"
#include "cpu_db.h"
#include "unknown.h"
#include "../core/report.h"

/* --- NASM probes -------------------------------------------------- */

typedef struct {
    unsigned long eax, ebx, ecx, edx;
} cpuid_regs_t;

extern int  cpu_asm_flags_test(void);
extern int  cpu_asm_pushfd_test(void);
extern int  cpu_asm_ac_test(void);
extern int  cpu_asm_id_test(void);
extern int  cpu_asm_int6_fired(void);
extern void cpu_asm_int6_clear(void);
extern void __far cpu_asm_int6_handler(void);
extern void cpu_asm_cpuid(unsigned long leaf, cpuid_regs_t __far *out);

#pragma aux cpu_asm_flags_test     "cpu_asm_flags_test_"     modify exact [ax];
#pragma aux cpu_asm_pushfd_test    "cpu_asm_pushfd_test_"    modify exact [ax];
#pragma aux cpu_asm_ac_test        "cpu_asm_ac_test_"        modify exact [ax];
#pragma aux cpu_asm_id_test        "cpu_asm_id_test_"        modify exact [ax];
#pragma aux cpu_asm_int6_fired     "cpu_asm_int6_fired_"     modify exact [ax];
#pragma aux cpu_asm_int6_clear     "cpu_asm_int6_clear_"     modify exact [ax];
#pragma aux cpu_asm_int6_handler   "cpu_asm_int6_handler_";
#pragma aux cpu_asm_cpuid          "cpu_asm_cpuid_" \
    parm caller [] \
    modify exact [ax bx cx dx];

/* --- Internal state ----------------------------------------------- */

static cpu_class_t last_detected = CPU_CLASS_UNKNOWN;
cpu_class_t cpu_get_class(void) { return last_detected; }

/* Cached CPUID family for class-token derivation. Set in detect_cpu()
 * when class==CPU_CLASS_CPUID; otherwise 0xFF meaning "no CPUID data". */
static unsigned char cpuid_family = 0xFF;

static const char *legacy_token(cpu_class_t c)
{
    switch (c) {
        case CPU_CLASS_8086:        return "8088";  /* safer default — promoted to "8086" by higher layers if 16-bit bus confirmed */
        case CPU_CLASS_286:         return "286";
        case CPU_CLASS_386:         return "386";
        case CPU_CLASS_486_NOCPUID: return "486-no-cpuid";
        default:                    return "unknown";
    }
}

/* Family-to-class-token for CPUID-capable CPUs.
 *
 * v0.8.0 M1.4 normalization: prior to 0.8.0, cpu.class on CPUID-capable
 * paths carried the full vendor string (e.g. "GenuineIntel"), which
 * broke validator rules keyed on family tokens and produced INI drift
 * vs. pre-CPUID captures. The normalized token here is the single
 * canonical family name shared between CPUID and legacy paths. Vendor
 * string continues to be emitted separately as cpu.vendor for callers
 * that need it.
 *
 * Non-Intel vendors on family 5/6 (Cyrix 6x86, AMD K5/K6) collapse to
 * "pentium" or "pentium_pro"; that's intentional for 0.8.0 scope. The
 * DB-matched cpu.detected row carries the fine-grained identity. Add
 * vendor-prefixed tokens if a consistency rule later needs the
 * distinction. */
static const char *family_to_class_token(unsigned char family)
{
    switch (family) {
        case 3:  return "386";
        case 4:  return "486";
        case 5:  return "pentium";
        case 6:  return "pentium_pro";
        case 15: return "pentium_4";
        default: return "cpuid_unknown";
    }
}

/* --- INT 6 vector install / restore ------------------------------- */

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

/* --- CPUID-derived buffers (DGROUP-resident) ---------------------- */

static cpuid_regs_t leaf0_regs;
static cpuid_regs_t leaf1_regs;
static char         vendor_string[13];  /* 12 chars + NUL */

int cpu_has_tsc(void)
{
    /* CPUID leaf 1 EDX bit 4 = Time Stamp Counter. leaf1_regs is
     * zero-initialized by C rules and populated by detect_cpu on
     * CPU_CLASS_CPUID, so the feature test is safe to call anytime. */
    if (last_detected != CPU_CLASS_CPUID) return 0;
    return (leaf1_regs.edx & 0x10UL) ? 1 : 0;
}

static void extract_vendor_string(void)
{
    /* CPUID leaf 0: EBX, EDX, ECX contain the 12-char vendor string in
     * little-endian order: EBX low to high, then EDX, then ECX. */
    unsigned long ebx = leaf0_regs.ebx;
    unsigned long edx = leaf0_regs.edx;
    unsigned long ecx = leaf0_regs.ecx;
    vendor_string[0]  = (char)( ebx        & 0xFF);
    vendor_string[1]  = (char)((ebx >>  8) & 0xFF);
    vendor_string[2]  = (char)((ebx >> 16) & 0xFF);
    vendor_string[3]  = (char)((ebx >> 24) & 0xFF);
    vendor_string[4]  = (char)( edx        & 0xFF);
    vendor_string[5]  = (char)((edx >>  8) & 0xFF);
    vendor_string[6]  = (char)((edx >> 16) & 0xFF);
    vendor_string[7]  = (char)((edx >> 24) & 0xFF);
    vendor_string[8]  = (char)( ecx        & 0xFF);
    vendor_string[9]  = (char)((ecx >>  8) & 0xFF);
    vendor_string[10] = (char)((ecx >> 16) & 0xFF);
    vendor_string[11] = (char)((ecx >> 24) & 0xFF);
    vendor_string[12] = '\0';
}

/* --- Probe orchestration ------------------------------------------ */

static cpu_class_t probe_class(void)
{
    if (!cpu_asm_flags_test()) return CPU_CLASS_8086;

    install_int6();
    cpu_asm_int6_clear();
    if (!cpu_asm_pushfd_test()) {
        restore_int6();
        return CPU_CLASS_286;
    }
    restore_int6();

    if (!cpu_asm_ac_test()) return CPU_CLASS_386;

    if (cpu_asm_id_test()) return CPU_CLASS_CPUID;
    return CPU_CLASS_486_NOCPUID;
}

void detect_cpu(result_table_t *t, const opts_t *o)
{
    cpu_class_t class = probe_class();
    const cpu_db_entry_t *entry = (const cpu_db_entry_t *)0;
    (void)o;  /* /NOCYRIX gating lands with the DIR probe in a follow-up */

    last_detected = class;  /* expose for downstream detect modules */

    if (class == CPU_CLASS_CPUID) {
        unsigned char family, model, stepping;

        /* Leaf 0: vendor string and max supported leaf */
        cpu_asm_cpuid(0UL, &leaf0_regs);
        extract_vendor_string();

        /* Leaf 1: family, model, stepping, feature bits */
        if (leaf0_regs.eax >= 1UL) {
            cpu_asm_cpuid(1UL, &leaf1_regs);
            /*
             * EAX layout on 486 / Pentium:
             *   [3:0]   stepping
             *   [7:4]   model
             *   [11:8]  family
             * Extended family/model (bits [27:20] and [19:16]) are
             * Pentium-Pro+ and we ignore them for v0.2 — the DB entries
             * rely on base family/model only.
             */
            stepping = (unsigned char)( leaf1_regs.eax        & 0x0F);
            model    = (unsigned char)((leaf1_regs.eax >>  4) & 0x0F);
            family   = (unsigned char)((leaf1_regs.eax >>  8) & 0x0F);
            cpuid_family = family;  /* cache for class-token derivation */

            entry = cpu_db_lookup_cpuid(vendor_string, family, model, stepping);

            report_add_str(t, "cpu.vendor",    vendor_string,
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
            {
                /* family/model/stepping serialize into a stable display
                 * "family.model.stepping" so the raw values survive into
                 * the INI even without a DB hit. */
                static char fms_buf[16];
                sprintf(fms_buf, "%u.%u.%u",
                        (unsigned)family, (unsigned)model, (unsigned)stepping);
                report_add_str(t, "cpu.family_model_stepping", fms_buf,
                               env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
            }
        }
    } else if (class != CPU_CLASS_UNKNOWN) {
        entry = cpu_db_lookup_legacy(legacy_token(class));
    }

    /* Emit the canonical-signature keys (cpu.detected, cpu.class) and
     * the friendly name when we have it. */
    if (entry) {
        report_add_str(t, "cpu.detected", entry->friendly,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        if (entry->notes && *entry->notes) {
            report_add_str(t, "cpu.notes", entry->notes,
                           env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
        }
        /* Rule 4b inputs: empirical iters/sec range for bench_cpu on a
         * clean system. Only emitted when the DB has real data (both
         * nonzero); consist.c rule 4b no-ops on either key's absence.
         * File-scope static display buffers for the same lifetime
         * reason as cpu.family_model_stepping above. */
        if (entry->iters_low > 0 && entry->iters_high > 0) {
            static char iters_low_buf[16], iters_high_buf[16];
            sprintf(iters_low_buf,  "%lu", entry->iters_low);
            sprintf(iters_high_buf, "%lu", entry->iters_high);
            report_add_u32(t, "cpu.bench_iters_low",  entry->iters_low,
                           iters_low_buf,  env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
            report_add_u32(t, "cpu.bench_iters_high", entry->iters_high,
                           iters_high_buf, env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        }
    } else {
        static char unk_detail[80];
        report_add_str(t, "cpu.detected",
                       class == CPU_CLASS_CPUID ? vendor_string : "unknown",
                       env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
        if (class == CPU_CLASS_CPUID) {
            sprintf(unk_detail, "vendor=%s fms=%u.%u.%u features_edx=%08lx",
                    vendor_string,
                    (unsigned)((leaf1_regs.eax >> 8) & 0x0F),
                    (unsigned)((leaf1_regs.eax >> 4) & 0x0F),
                    (unsigned)(leaf1_regs.eax & 0x0F),
                    leaf1_regs.edx);
            unknown_record("cpu",
                           "CPUID-capable CPU not in DB, please submit",
                           unk_detail);
        } else if (class != CPU_CLASS_UNKNOWN) {
            sprintf(unk_detail, "legacy_class=%s", legacy_token(class));
            unknown_record("cpu",
                           "Legacy CPU class lookup missed",
                           unk_detail);
        }
    }

    {
        /* cpu.class — one of the canonical signature keys, and the input
         * to several consistency rules (486dx_fpu, extmem_cpu, 8086_bus,
         * dma_class_coherence) + the validator's cache_l1_within_known_max
         * rule. Must be a normalized family token, shared between CPUID
         * and legacy paths. Per 0.8.0 plan §6 M1.4: pre-0.8.0 the CPUID
         * path carried the full vendor string ("GenuineIntel") which
         * broke rules keyed on family. Now family-derived.
         *
         * Selection:
         *   1. DB entry with legacy_class set → use entry->legacy_class
         *      (authoritative; hand-curated in hw_db/cpus.csv).
         *   2. CPUID-capable without DB-supplied legacy_class → derive
         *      token from family field (3→"386", 4→"486", 5→"pentium",
         *      etc. via family_to_class_token).
         *   3. Pre-CPUID detection without DB entry → legacy_token(class).
         *   4. Unknown → "unknown".
         *
         * Vendor string is always emitted separately as cpu.vendor above. */
        const char *class_token;
        if (entry && entry->match_kind == CPU_DB_MATCH_LEGACY
                 && entry->legacy_class && entry->legacy_class[0]) {
            class_token = entry->legacy_class;
        } else if (class == CPU_CLASS_CPUID && cpuid_family != 0xFF) {
            class_token = family_to_class_token(cpuid_family);
        } else {
            class_token = legacy_token(class);
        }
        report_add_str(t, "cpu.class", class_token,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    }
}
