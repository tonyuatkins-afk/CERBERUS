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

/* Display buffers per emitted key. report_add_str stores the value
 * pointer verbatim (report.c:55), so stack-local sprintf targets would
 * dangle after detect_audio returns. Two buffers at risk here:
 *   - audio.sb_dsp_version  when SB probe succeeds
 *   - audio.detected        on the DB-miss path (entry == NULL)
 * Each gets its own static so INI write and UI render see valid bytes. */
static char audio_sb_dsp_version_val[8];
static char audio_match_key[24];

#define OPL_DEFAULT_ADDR 0x388
#define OPL_DEFAULT_DATA 0x389

/* ----------------------------------------------------------------------- */
/* OPL probe (canonical 13-step sequence)                                   */
/*                                                                          */
/* Port layout: industry-standard Adlib mirror lives at 0x388/0x389 on      */
/* most ISA SB clones and when a PnP SB card is in its default/unmanaged    */
/* state. After Creative's CTCM.EXE configures a Vibra 16S PnP, the legacy  */
/* 0x388 mirror may be disabled and OPL only answers at BLASTER-base + 8.   */
/* Probe logic: try BLASTER-base+8 first if we have a base; fall back to    */
/* 0x388. Avoids a silent false-negative on CTCM-managed Vibra cards.       */
/* ----------------------------------------------------------------------- */

static int probe_opl_at(unsigned int addr_port, unsigned int data_port,
                        int *out_opl3)
{
    unsigned char s1, s2;

    *out_opl3 = 0;

    /* Reset timers (write 60h to R4) */
    outp(addr_port, 0x04);
    outp(data_port, 0x60);
    /* Reset IRQ status (write 80h to R4) */
    outp(addr_port, 0x04);
    outp(data_port, 0x80);
    /* Read status — expect bits 7:6 = 00 */
    s1 = (unsigned char)(inp(addr_port) & 0xE0);
    if (s1 != 0) {
        /* OPL in an unexpected state — retry once */
        outp(addr_port, 0x04); outp(data_port, 0x60);
        outp(addr_port, 0x04); outp(data_port, 0x80);
        s1 = (unsigned char)(inp(addr_port) & 0xE0);
        if (s1 != 0) return 0;  /* no OPL present */
    }
    /* Set Timer 1 to overflow immediately */
    outp(addr_port, 0x02);
    outp(data_port, 0xFF);
    /* Start Timer 1 (mask Timer 2) */
    outp(addr_port, 0x04);
    outp(data_port, 0x21);
    /* Wait for overflow — at least 80µs */
    timing_wait_us(100UL);
    /* Read status — expect bits 7:6 = 11 if OPL present */
    s2 = (unsigned char)(inp(addr_port) & 0xE0);
    /* Reset */
    outp(addr_port, 0x04);
    outp(data_port, 0x60);
    outp(addr_port, 0x04);
    outp(data_port, 0x80);

    if ((s2 & 0xC0) != 0xC0) return 0;

    /* OPL2 confirmed. OPL3 detection heuristic: OPL2 returns 0x06 in the
     * low 5 bits of status, OPL3 returns 0x00. */
    {
        unsigned char op_status = (unsigned char)(inp(addr_port) & 0x06);
        if (op_status == 0x00) *out_opl3 = 1;
    }
    return 1;
}

static int probe_opl(int *out_opl3)
{
    unsigned int sb_base = 0;

    /* Prefer BLASTER-base+8 when BLASTER is set — covers CTCM-managed
     * Vibra 16S and similar PnP cards whose Adlib mirror is off. */
    if (parse_blaster(&sb_base) && sb_base != 0) {
        if (probe_opl_at(sb_base + 0x08, sb_base + 0x09, out_opl3)) return 1;
    }

    /* Fallback: industry-standard Adlib port. */
    return probe_opl_at(OPL_DEFAULT_ADDR, OPL_DEFAULT_DATA, out_opl3);
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
    /*
     * Creative Sound Blaster DSP port layout (standard SB/SB16/Vibra):
     *   base+0x06  Reset (write 1, delay, write 0, delay)
     *   base+0x0A  Read Data (the byte the DSP hands us)
     *   base+0x0C  Write Command/Data
     *   base+0x0E  Read-Buffer Status (bit 7 = data available)
     *
     * Pre-fix, this code polled base+0x0A for bit 7 as a data-ready
     * signal. Reading base+0x0A returns the current data byte (or stale
     * contents when no data is pending); bit 7 set there depends on the
     * value of that byte, not on DSP readiness. On the Vibra 16S it
     * would either loop-timeout (stale byte had bit 7 clear) or read
     * garbage (bit 7 set coincidentally). Status polling goes through
     * base+0x0E — correct per the Creative programmer's reference.
     */
    unsigned int reset_port  = base + 0x06;
    unsigned int data_port   = base + 0x0A;
    unsigned int write_port  = base + 0x0C;
    unsigned int status_port = base + 0x0E;
    unsigned int i;
    unsigned char v;

    /* DSP reset */
    outp(reset_port, 1);
    timing_wait_us(5UL);
    outp(reset_port, 0);
    timing_wait_us(100UL);

    /* Wait for DSP to place 0xAA in the read buffer (status bit 7 = ready) */
    for (i = 0; i < 1000; i++) {
        if (inp(status_port) & 0x80) break;
    }
    if (i == 1000) return 0;

    v = (unsigned char)inp(data_port);
    if (v != 0xAA) return 0;

    /* Request DSP version — command E1h */
    outp(write_port, 0xE1);
    timing_wait_us(10UL);
    for (i = 0; i < 1000; i++) {
        if (inp(status_port) & 0x80) break;
    }
    if (i == 1000) return 0;
    *out_major = (unsigned int)inp(data_port);
    for (i = 0; i < 1000; i++) {
        if (inp(status_port) & 0x80) break;
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
        sprintf(audio_sb_dsp_version_val, "%u.%02u", dsp_major, dsp_minor);
        report_add_str(t, "audio.sb_present", "yes",
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        report_add_str(t, "audio.sb_dsp_version", audio_sb_dsp_version_val,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    } else {
        report_add_str(t, "audio.sb_present", "no",
                       env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
    }

    /* Build composite match key and look up friendly entry */
    if (!opl_present && !have_sb) {
        strcpy(audio_match_key, "pc-speaker-only");
    } else if (have_sb) {
        sprintf(audio_match_key, "%s:%02X%02X", opl_token, dsp_major & 0xFF, dsp_minor & 0xFF);
    } else {
        sprintf(audio_match_key, "%s:none", opl_token);
    }

    entry = audio_db_lookup(audio_match_key);
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
        report_add_str(t, "audio.detected", audio_match_key,
                       env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
    }
}
