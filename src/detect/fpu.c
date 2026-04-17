/*
 * FPU detection — Phase 1 Task 1.2 + 1.2a.
 *
 * Presence probe: sentinel-based FNINIT/FNSTSW test (fpu_a.asm). On
 * systems without a coprocessor installed, the "no wait" variants
 * execute as effective no-ops and leave the sentinel untouched.
 *
 * Classification: once presence is known, consult cpu_get_class() to
 * pick a tag that maps to an fpu_db entry. Coarse for v0.2 — we don't
 * distinguish 287 from 387 or identify Cyrix FasMath vs Intel 387 vs
 * IIT — those refinements land as follow-ups (affine/projective infinity
 * test, vendor-byte scan at known addresses for the well-known third
 * parties). The DB has entries for the refined tags so future detection
 * iterations plug in without schema changes.
 *
 * RapidCAD edge case: reports as 486 CPU class but uses a 387-class
 * FPU. Detection here will tag it as "integrated-486" which is wrong,
 * but plan allows deferring — RapidCAD probe is an async-clock timing
 * test and is scope-inflating for v0.2.
 */

#include <string.h>
#include "detect.h"
#include "cpu.h"
#include "env.h"
#include "fpu_db.h"
#include "../core/report.h"

extern int fpu_asm_probe(void);
#pragma aux fpu_asm_probe "fpu_asm_probe_" modify exact [ax];

/* DGROUP-resident sentinel — NASM probe writes into it via DS:offset */
unsigned short fpu_sentinel;

static const char *tag_for(int present, cpu_class_t cls)
{
    if (!present) return "none";

    switch (cls) {
        case CPU_CLASS_CPUID:
            /* Vendor refinement requires reading CPUID leaf 1 EDX bit 0 +
             * the vendor string — already extracted in cpu.c but not
             * exposed. For v0.2 we tag all CPUID-capable-with-FPU as
             * "integrated-486" and let the DB friendly name normalize
             * the display. Phase 3 bench code will need to consult the
             * vendor string directly for class_ipc tables anyway; we can
             * route through that when it lands. */
            return "integrated-486";
        case CPU_CLASS_486_NOCPUID:
            return "integrated-486";
        case CPU_CLASS_386:
            return "387";
        case CPU_CLASS_286:
            return "287";
        case CPU_CLASS_8086:
            return "8087";
        default:
            return "external-unknown";
    }
}

void detect_fpu(result_table_t *t)
{
    int present = fpu_asm_probe();
    cpu_class_t cls = cpu_get_class();
    const char *tag = tag_for(present, cls);
    const fpu_db_entry_t *entry = fpu_db_lookup(tag);

    /* fpu.detected is one of the canonical signature keys (per PF-4) so
     * the display must be stable. The DB's `tag` is our canonical
     * serialization; the friendly name is display-only and may drift
     * across CSV updates without changing the signature. */
    report_add_str(t, "fpu.detected", tag,
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    if (entry) {
        report_add_str(t, "fpu.friendly", entry->friendly,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        if (entry->vendor && *entry->vendor) {
            report_add_str(t, "fpu.vendor", entry->vendor,
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        }
        if (entry->notes && *entry->notes) {
            report_add_str(t, "fpu.notes", entry->notes,
                           env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
        }
    }
}
