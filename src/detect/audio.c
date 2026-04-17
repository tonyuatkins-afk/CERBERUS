/*
 * Audio detection — Phase 1 Task 1.7 + 1.7a.
 *
 * Probes:
 *   1. PC speaker — always present on IBM PC-class hardware, reported
 *      unconditionally at HIGH confidence.
 *   2. OPL2/OPL3 at port 388h/389h via the canonical timer probe. Uses
 *      timing_wait_us for the ~80µs post-timer-start delay.
 *   3. Sound Blaster DSP via BLASTER env → reset sequence at port
 *      2x6h → DSP version query (E1h to 2xCh, read from 2xAh).
 *
 * Composite match key for the audio_db lookup:
 *   "pc-speaker-only"   when neither OPL nor SB DSP responds
 *   "opl2:none"         OPL2 present, no SB
 *   "opl3:none"         OPL3 present, no SB
 *   "opl2:<dsp>"        OPL2 + SB with DSP version <dsp> (4 hex chars)
 *   "opl3:<dsp>"        OPL3 + SB with DSP version <dsp>
 *
 * DSP version is encoded as major*0x100 + minor, with leading zeros
 * (e.g. "0400" for DSP 4.00, "0302" for DSP 3.02).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include "detect.h"
#include "env.h"
#include "audio_db.h"
#include "../core/timing.h"
#include "../core/report.h"

#define OPL_ADDR 0x388
#define OPL_DATA 0x389

/* ----------------------------------------------------------------------- */
/* OPL probe (canonical 13-step sequence)                                   */
/* ----------------------------------------------------------------------- */

static int probe_opl(int *out_opl3)
{
    unsigned char s1, s2;

    *out_opl3 = 0;

    /* Reset timers (write 60h to R4) */
    outp(OPL_ADDR, 0x04);
    outp(OPL_DATA, 0x60);
    /* Reset IRQ status (write 80h to R4) */
    outp(OPL_ADDR, 0x04);
    outp(OPL_DATA, 0x80);
    /* Read status — expect bits 7:6 = 00 */
    s1 = (unsigned char)(inp(OPL_ADDR) & 0xE0);
    if (s1 != 0) {
        /* OPL in an unexpected state — retry once */
        outp(OPL_ADDR, 0x04); outp(OPL_DATA, 0x60);
        outp(OPL_ADDR, 0x04); outp(OPL_DATA, 0x80);
        s1 = (unsigned char)(inp(OPL_ADDR) & 0xE0);
        if (s1 != 0) return 0;  /* no OPL present */
    }
    /* Set Timer 1 to overflow immediately */
    outp(OPL_ADDR, 0x02);
    outp(OPL_DATA, 0xFF);
    /* Start Timer 1 (mask Timer 2) */
    outp(OPL_ADDR, 0x04);
    outp(OPL_DATA, 0x21);
    /* Wait for overflow — at least 80µs */
    timing_wait_us(100UL);
    /* Read status — expect bits 7:6 = 11 if OPL present */
    s2 = (unsigned char)(inp(OPL_ADDR) & 0xE0);
    /* Reset */
    outp(OPL_ADDR, 0x04);
    outp(OPL_DATA, 0x60);
    outp(OPL_ADDR, 0x04);
    outp(OPL_DATA, 0x80);

    if ((s2 & 0xC0) != 0xC0) return 0;

    /* OPL2 confirmed. Check for OPL3 by attempting the extended register
     * bank at 38Ah/38Bh — OPL2 ignores these, OPL3 accepts. A post-write
     * status read should have distinct bits on OPL3. For v0.2 we use a
     * simpler heuristic: OPL2 returns 0x06 in the low 5 bits of status,
     * OPL3 returns 0x00. */
    {
        unsigned char op_status = (unsigned char)(inp(OPL_ADDR) & 0x06);
        if (op_status == 0x00) *out_opl3 = 1;
    }
    return 1;
}

/* ----------------------------------------------------------------------- */
/* BLASTER env parse + DSP version                                          */
/* ----------------------------------------------------------------------- */

static int parse_blaster(unsigned int *out_port)
{
    const char *env = getenv("BLASTER");
    const char *p;
    if (!env) return 0;
    for (p = env; *p; p++) {
        if ((*p == 'A' || *p == 'a') && p[1]) {
            unsigned int port = 0;
            const char *q = p + 1;
            while (*q && *q != ' ' && *q != '\t') {
                char c = *q++;
                unsigned int d;
                if      (c >= '0' && c <= '9') d = (unsigned int)(c - '0');
                else if (c >= 'a' && c <= 'f') d = (unsigned int)(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') d = (unsigned int)(c - 'A' + 10);
                else break;
                port = (port << 4) | d;
            }
            if (port != 0) {
                *out_port = port;
                return 1;
            }
        }
    }
    return 0;
}

static int probe_sb_dsp(unsigned int base, unsigned int *out_major, unsigned int *out_minor)
{
    unsigned int reset_port = base + 0x06;
    unsigned int data_port  = base + 0x0A;
    unsigned int write_port = base + 0x0C;
    unsigned int i;
    unsigned char v;

    /* DSP reset */
    outp(reset_port, 1);
    timing_wait_us(5UL);
    outp(reset_port, 0);
    timing_wait_us(100UL);

    /* Wait for DSP to indicate data available (bit 7 of data_port) */
    for (i = 0; i < 1000; i++) {
        if (inp(data_port) & 0x80) break;
    }
    if (i == 1000) return 0;

    v = (unsigned char)inp(data_port);
    if (v != 0xAA) return 0;

    /* Request DSP version — command E1h */
    outp(write_port, 0xE1);
    timing_wait_us(10UL);
    for (i = 0; i < 1000; i++) {
        if (inp(data_port) & 0x80) break;
    }
    if (i == 1000) return 0;
    *out_major = (unsigned int)inp(data_port);
    for (i = 0; i < 1000; i++) {
        if (inp(data_port) & 0x80) break;
    }
    if (i == 1000) return 0;
    *out_minor = (unsigned int)inp(data_port);
    return 1;
}

/* ----------------------------------------------------------------------- */
/* Orchestration                                                            */
/* ----------------------------------------------------------------------- */

void detect_audio(result_table_t *t)
{
    int          opl_present, opl3;
    unsigned int sb_base = 0, dsp_major = 0, dsp_minor = 0;
    int          have_sb = 0;
    const char  *opl_token = "none";
    char         match_key[24];
    const audio_db_entry_t *entry;

    /* PC speaker */
    report_add_str(t, "audio.pc_speaker", "yes",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    /* OPL */
    opl_present = probe_opl(&opl3);
    if (opl_present) {
        opl_token = opl3 ? "opl3" : "opl2";
        report_add_str(t, "audio.opl", opl_token,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    } else {
        report_add_str(t, "audio.opl", "none",
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    }

    /* Sound Blaster (only probed if BLASTER env names a port) */
    if (parse_blaster(&sb_base)) {
        have_sb = probe_sb_dsp(sb_base, &dsp_major, &dsp_minor);
    }
    if (have_sb) {
        char dsp_buf[8];
        sprintf(dsp_buf, "%u.%02u", dsp_major, dsp_minor);
        report_add_str(t, "audio.sb_present", "yes",
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        report_add_str(t, "audio.sb_dsp_version", dsp_buf,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    } else {
        report_add_str(t, "audio.sb_present", "no",
                       env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
    }

    /* Build composite match key and look up friendly entry */
    if (!opl_present && !have_sb) {
        strcpy(match_key, "pc-speaker-only");
    } else if (have_sb) {
        sprintf(match_key, "%s:%02X%02X", opl_token, dsp_major & 0xFF, dsp_minor & 0xFF);
    } else {
        sprintf(match_key, "%s:none", opl_token);
    }

    entry = audio_db_lookup(match_key);
    if (entry) {
        report_add_str(t, "audio.detected", entry->friendly,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        if (entry->vendor && *entry->vendor) {
            report_add_str(t, "audio.vendor", entry->vendor,
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        }
        if (entry->notes && *entry->notes) {
            report_add_str(t, "audio.notes", entry->notes,
                           env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
        }
    } else {
        report_add_str(t, "audio.detected", match_key,
                       env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
    }
}
