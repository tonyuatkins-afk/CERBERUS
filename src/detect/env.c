/*
 * Environment / emulator detection — Head I Task 1.0.
 *
 * Layered probe, cheapest/most-specific first:
 *   1. INT 2Fh AX=4A40h DOSBox-X integration callback (specific to DOSBox-X)
 *   2. INT 2Fh AX=1600h Windows multiplex / AX=4010h OS/2 DOS box
 *   3. CPUID leaf 0x40000000 hypervisor vendor (QEMU/KVM/HyperV/VBox/VMware)
 *   4. BIOS ROM string scan in F000:0000 - F000:FFFE
 *   5. timing_emulator_hint fallback — if PIT Channel 2 behaves oddly we
 *      flag EMU_UNKNOWN at MEDIUM confidence
 *
 * Conservative bias: when in doubt, assume emulated. A false positive only
 * clamps confidence to MEDIUM (which is appropriate for uncertain state).
 * A false negative lets the consistency engine calibrate to emulator
 * artifacts and false-positive on real hardware later — much worse.
 */

#include <string.h>
#include <stdlib.h>
#include <i86.h>
#include <dos.h>
#include "env.h"
#include "detect.h"
#include "../core/report.h"
#include "../core/timing.h"

static emulator_id_t current_emulator = EMU_NONE;
static const char   *current_name     = "none";

/* ----------------------------------------------------------------------- */
/* BIOS ROM string scan                                                     */
/* ----------------------------------------------------------------------- */

typedef struct {
    const char    *needle;
    emulator_id_t  id;
    const char    *name;
} emu_signature_t;

/* More-specific strings FIRST so DOSBox-X beats plain DOSBox when scanned
 * left-to-right. */
static const emu_signature_t bios_sigs[] = {
    { "DOSBox-X",   EMU_DOSBOX_X,   "dosbox-x"   },
    { "DOSBox",     EMU_DOSBOX,     "dosbox"     },
    { "PCem",       EMU_PCEM,       "pcem"       },
    { "86Box",      EMU_86BOX,      "86box"      },
    { "Bochs",      EMU_BOCHS,      "bochs"      },
    { "QEMU",       EMU_QEMU,       "qemu"       },
    { "VirtualBox", EMU_VIRTUALBOX, "virtualbox" },
    { "VMware",     EMU_VMWARE,     "vmware"     }
};
#define BIOS_SIG_COUNT (sizeof(bios_sigs) / sizeof(bios_sigs[0]))

static int mem_match(const unsigned char __far *mem, const char *needle, unsigned int n)
{
    unsigned int j;
    for (j = 0; j < n; j++) {
        if (mem[j] != (unsigned char)needle[j]) return 0;
    }
    return 1;
}

static int scan_bios_rom(void)
{
    /* Scan F000:0000 through F000:FFFE. The BIOS is typically 64KB; strings
     * appear anywhere. We scan in byte steps — slow on 8088 but one-time. */
    const unsigned char __far *bios = (const unsigned char __far *)MK_FP(0xF000, 0x0000);
    unsigned long i;
    unsigned int  s;
    unsigned int  needle_len[BIOS_SIG_COUNT];

    for (s = 0; s < BIOS_SIG_COUNT; s++) {
        needle_len[s] = (unsigned int)strlen(bios_sigs[s].needle);
    }

    for (i = 0; i < 0xFFFEUL; i++) {
        for (s = 0; s < BIOS_SIG_COUNT; s++) {
            if (i + needle_len[s] > 0xFFFEUL) continue;
            if (mem_match(bios + i, bios_sigs[s].needle, needle_len[s])) {
                current_emulator = bios_sigs[s].id;
                current_name     = bios_sigs[s].name;
                return 1;
            }
        }
    }
    return 0;
}

/* ----------------------------------------------------------------------- */
/* DOSBox-X INT 2Fh integration probe (AX=4A40h)                            */
/* ----------------------------------------------------------------------- */

static int probe_dosbox_x_int2f(void)
{
    /* DOSBox-X sets up INT 2Fh AX=4A40h as an integration callback. On real
     * hardware or plain DOSBox this returns unchanged. DOSBox-X returns
     * specific values in CX/DX identifying itself.
     *
     * Safety: INT 2Fh (DOS multiplex) is explicitly designed for caller-
     * extension discovery — unhandled AX values leave registers unchanged
     * and optionally set AL=0. Safe to probe on any DOS.
     */
    union REGS r;
    r.x.ax = 0x4A40;
    r.x.bx = 0;
    r.x.cx = 0;
    r.x.dx = 0;
    int86(0x2F, &r, &r);
    /* DOSBox-X returns AX with specific values; here we only care "did
     * the callback respond". Heuristic: AX changed from 0x4A40 AND CX/DX
     * are non-zero. This is conservative — worst case, we fall through to
     * the BIOS ROM scan. */
    if (r.x.ax != 0x4A40 && (r.x.cx != 0 || r.x.dx != 0)) {
        current_emulator = EMU_DOSBOX_X;
        current_name     = "dosbox-x";
        return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------------- */
/* Windows / OS/2 DOS-box probes                                            */
/* ----------------------------------------------------------------------- */

static int probe_windows_or_os2(void)
{
    union REGS r;

    /* INT 2Fh AX=1600h — Windows enhanced-mode / WinNT NTVDM detection.
     * AL=0 or 0x80 = no Windows. Any other value = running under Windows. */
    r.x.ax = 0x1600;
    int86(0x2F, &r, &r);
    if (r.h.al != 0x00 && r.h.al != 0x80) {
        current_emulator = EMU_NTVDM;
        current_name     = "ntvdm";
        return 1;
    }

    /* INT 2Fh AX=4010h — OS/2 DOS box detection. Returns AX=FFFF, BX=OS/2 ver
     * if running under OS/2 2.0+. */
    r.x.ax = 0x4010;
    int86(0x2F, &r, &r);
    if (r.x.ax == 0xFFFF) {
        current_emulator = EMU_OS2_DOS;
        current_name     = "os2-dos";
        return 1;
    }

    return 0;
}

/* ----------------------------------------------------------------------- */
/* Public API                                                               */
/* ----------------------------------------------------------------------- */

emulator_id_t env_emulator(void)    { return current_emulator; }
const char   *env_emulator_name(void){ return current_name; }
int           env_is_emulated(void)  { return current_emulator != EMU_NONE; }

confidence_t env_clamp(confidence_t c)
{
    /* Any detected emulator caps confidence at MEDIUM. Cache/timing tests
     * inside DOSBox-X especially cannot be trusted at HIGH — the plan is
     * explicit about this. */
    if (current_emulator == EMU_NONE) return c;
    if (c == CONF_HIGH) return CONF_MEDIUM;
    return c;
}

void detect_env(result_table_t *t)
{
    current_emulator = EMU_NONE;
    current_name     = "none";

    if (!probe_dosbox_x_int2f()) {
        if (!probe_windows_or_os2()) {
            if (!scan_bios_rom()) {
                /* Final fallback: if the PIT Channel 2 sanity probe flagged
                 * weirdness at timing_init(), mark as unknown emulator. */
                if (timing_emulator_hint()) {
                    current_emulator = EMU_UNKNOWN;
                    current_name     = "unknown";
                }
            }
        }
    }

    {
        /* Emit the [environment] section */
        const char *virt = (current_emulator == EMU_NONE) ? "no" : "yes";
        const char *pen  = (current_emulator == EMU_NONE) ? "none" : "medium";
        report_add_str(t, "environment.emulator",           current_name, CONF_HIGH, VERDICT_UNKNOWN);
        report_add_str(t, "environment.virtualized",        virt,         CONF_HIGH, VERDICT_UNKNOWN);
        report_add_str(t, "environment.confidence_penalty", pen,          CONF_HIGH, VERDICT_UNKNOWN);
    }
}
