/*
 * BIOS info detection — Phase 1 Task 1.8 + 1.8a.
 *
 * BIOS ROM resides at F000:0000 through F000:FFFF (64KB). This module:
 *
 *   1. Reads the BIOS date stamp at F000:FFF5 (8 chars MM/DD/YY).
 *   2. Scans the F000 segment for a bios_db signature to identify
 *      the BIOS family (Award, AMI, Phoenix, IBM, MR, ...).
 *   3. Probes INT 15h AH=C0h for the system config / extensions.
 *   4. Scans for the $PnP PnP header signature.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include "detect.h"
#include "env.h"
#include "bios_db.h"
#include "../core/report.h"

static int mem_match(const unsigned char __far *mem, const char *needle, unsigned int n)
{
    unsigned int j;
    for (j = 0; j < n; j++) {
        if (mem[j] != (unsigned char)needle[j]) return 0;
    }
    return 1;
}

static const bios_db_entry_t *scan_bios_for_family(void)
{
    const unsigned char __far *bios = (const unsigned char __far *)MK_FP(0xF000, 0x0000);
    unsigned long i;
    unsigned int  s;
    unsigned int  need_len[32];

    for (s = 0; s < bios_db_count && s < 32; s++) {
        need_len[s] = (unsigned int)strlen(bios_db[s].signature);
    }

    for (i = 0; i < 0xFFFEUL; i++) {
        for (s = 0; s < bios_db_count && s < 32; s++) {
            if (i + need_len[s] > 0xFFFEUL) continue;
            if (mem_match(bios + i, bios_db[s].signature, need_len[s])) {
                return &bios_db[s];
            }
        }
    }
    return (const bios_db_entry_t *)0;
}

static void read_bios_date(char out[9])
{
    const unsigned char __far *p = (const unsigned char __far *)MK_FP(0xF000, 0xFFF5);
    unsigned int i;
    for (i = 0; i < 8; i++) {
        unsigned char c = p[i];
        /* Some BIOSes put non-printable or junk here; substitute with '?'  */
        out[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    out[8] = '\0';
}

static int scan_pnp_header(void)
{
    const unsigned char __far *bios = (const unsigned char __far *)MK_FP(0xF000, 0x0000);
    unsigned long i;
    for (i = 0; i < 0xFFFCUL; i++) {
        if (bios[i]     == '$' && bios[i + 1] == 'P' &&
            bios[i + 2] == 'n' && bios[i + 3] == 'P') {
            return 1;
        }
    }
    return 0;
}

void detect_bios(result_table_t *t)
{
    char date_buf[9];
    const bios_db_entry_t *entry;

    read_bios_date(date_buf);
    report_add_str(t, "bios.date", date_buf,
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    entry = scan_bios_for_family();
    if (entry) {
        report_add_str(t, "bios.vendor", entry->vendor,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        report_add_str(t, "bios.family", entry->family,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        if (entry->era && *entry->era) {
            report_add_str(t, "bios.era", entry->era,
                           env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
        }
        if (entry->notes && *entry->notes) {
            report_add_str(t, "bios.notes", entry->notes,
                           env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
        }
    } else {
        report_add_str(t, "bios.family", "unknown",
                       env_clamp(CONF_LOW), VERDICT_UNKNOWN);
    }

    report_add_str(t, "bios.pnp_header",
                   scan_pnp_header() ? "yes" : "no",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
}
