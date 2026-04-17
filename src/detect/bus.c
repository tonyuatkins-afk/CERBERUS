/*
 * Bus detection — Phase 1 Task 1.5.
 *
 * Layered probe:
 *   1. PCI BIOS — INT 1Ah AX=B101h returns signature "PCI " in EDX,
 *      version + mechanism in BX/CX/EDH. Authoritative when present.
 *   2. ISA bit-width — inferred from CPU class + BDA equipment word.
 *      8-bit ISA is 8086-class only; everything 286+ runs 16-bit ISA.
 *   3. VLB — no standard probe exists. Best-effort inference: 486-class
 *      CPU AND no PCI BIOS makes VLB plausible but not certain, so we
 *      report "maybe" at LOW confidence with an explanatory note.
 *
 * Reports the canonical signature key `bus.class` at HIGH confidence
 * when we have authoritative data (PCI or ISA-8 floor), MEDIUM when
 * we're inferring from CPU class, LOW when we're guessing at VLB.
 */

#include <string.h>
#include <dos.h>
#include <i86.h>
#include "detect.h"
#include "cpu.h"
#include "env.h"
#include "../core/report.h"

static int probe_pci(unsigned int *out_major, unsigned int *out_minor,
                     unsigned int *out_last_bus)
{
    union  REGS  r;
    struct SREGS sr;

    r.x.ax = 0xB101;
    r.x.bx = 0;
    r.x.cx = 0;
    r.x.dx = 0;
    int86x(0x1A, &r, &r, &sr);

    /* BIOS returns EDX = "PCI " (little-endian 'P''C''I'' ') on success.
     * Watcom REGS gives us DX (low 16 of EDX); on 16-bit call the full
     * EDX isn't exposed via union REGS. Reliable check: AH=00 on
     * success, PCI present flag in AL bit 0. */
    if (r.h.ah != 0x00) return 0;
    if ((r.h.al & 0x01) == 0) return 0;

    /* BX returns BCD version: BH=major, BL=minor */
    *out_major = r.h.bh;
    *out_minor = r.h.bl;
    /* CX returns number of last PCI bus */
    *out_last_bus = r.h.cl;
    return 1;
}

void detect_bus(result_table_t *t)
{
    cpu_class_t   cls = cpu_get_class();
    unsigned int  pci_major = 0, pci_minor = 0, last_bus = 0;
    int           have_pci = 0;
    char          scratch[32];

    /* PCI: authoritative when present */
    have_pci = probe_pci(&pci_major, &pci_minor, &last_bus);

    if (have_pci) {
        report_add_str(t, "bus.class", "pci",
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        report_add_str(t, "bus.pci_present", "yes",
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        sprintf(scratch, "%u.%u", pci_major, pci_minor);
        report_add_str(t, "bus.pci_version", scratch,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        sprintf(scratch, "%u", last_bus);
        report_add_u32(t, "bus.pci_last_bus", (unsigned long)last_bus,
                       scratch, env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        return;
    }

    report_add_str(t, "bus.pci_present", "no",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    /* No PCI. Infer primary bus from CPU class. */
    switch (cls) {
        case CPU_CLASS_8086:
            report_add_str(t, "bus.class", "isa8",
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
            break;
        case CPU_CLASS_286:
        case CPU_CLASS_386:
            report_add_str(t, "bus.class", "isa16",
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
            break;
        case CPU_CLASS_486_NOCPUID:
        case CPU_CLASS_CPUID:
            /* 486-class machine without PCI — VLB is plausible but not
             * detectable without per-card chipset probes. Flag as isa16
             * at MEDIUM and note the VLB possibility separately. */
            report_add_str(t, "bus.class", "isa16",
                           env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
            report_add_str(t, "bus.vlb_possible", "yes",
                           env_clamp(CONF_LOW), VERDICT_UNKNOWN);
            report_add_str(t, "bus.vlb_note",
                           "VLB presence cannot be reliably detected without per-card probes",
                           env_clamp(CONF_LOW), VERDICT_UNKNOWN);
            break;
        default:
            report_add_str(t, "bus.class", "unknown",
                           env_clamp(CONF_LOW), VERDICT_UNKNOWN);
            break;
    }
}
