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
#include "../core/crumb.h"

/* Display buffers per emitted key. report_add_str stores the value
 * pointer verbatim (report.c:55), so stack-local sprintf targets would
 * dangle after detect_audio returns. Three buffers at risk here:
 *   - audio.sb_dsp_version     when SB probe succeeds
 *   - audio.detected           on the DB-miss path (entry == NULL)
 *   - audio.opl_probe_trace    diagnostic trace (issue #2)
 * Each gets its own static so INI write and UI render see valid bytes.
 *
 * mixer_chip_observed and mixer_chip_expected both point at string
 * literals (from probe_mixer_chip's static returns and from the audio_db
 * entry's mixer_chip field), so they don't need buffers of their own. */
static char audio_sb_dsp_version_val[8];
static char audio_match_key[24];

/* OPL probe diagnostic buffer for issue #2 (Vibra 16 PnP intermittency).
 * Each probe_opl_at call appends its sequence of status-register reads.
 * Rendered as audio.opl_probe_trace in the INI so multiple cold-boot
 * captures can be diffed to identify which byte value differs between
 * "opl3 detected" runs and "none detected" runs. Format per port:
 *   <port>:s1=XX[/s1b=XX] s2=XX op=XX,<verdict> ...
 * with " fallback=<port>..." appended when the primary probe bailed. */
static char audio_opl_trace[128];

#define OPL_DEFAULT_ADDR 0x388
#define OPL_DEFAULT_DATA 0x389

/* Forward declarations. probe_opl (below) calls parse_blaster before its
 * body appears further down the file; without a prototype Watcom emits
 * W131 "No prototype found for function" and the call falls under C89
 * implicit-declaration rules (UB). Keeping function-body order reference-
 * faithful with the original layout; the forward decls are the surgical
 * fix. parse_blaster_t is forward-declared alongside for symmetry and
 * future-proofing against call-site reordering. */
static int parse_blaster(unsigned int *out_port);
static int parse_blaster_t(int *out_t);

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

/* Append a fragment to the diagnostic trace, bounded by the buffer size
 * so we never overrun audio_opl_trace regardless of how many fallback
 * retries append data. Silent truncation is acceptable here; the buffer
 * is diagnostic, not load-bearing. */
static void opl_trace_append(const char *fragment)
{
    unsigned int cur = 0;
    unsigned int cap = sizeof(audio_opl_trace);
    while (cur < cap && audio_opl_trace[cur] != '\0') cur++;
    while (cur < cap - 1 && *fragment != '\0') {
        audio_opl_trace[cur++] = *fragment++;
    }
    audio_opl_trace[cur] = '\0';
}

static int probe_opl_at(unsigned int addr_port, unsigned int data_port,
                        int *out_opl3)
{
    unsigned char s1, s1b, s2, op;
    char frag[32];

    *out_opl3 = 0;

    sprintf(frag, " %04X:", addr_port);
    opl_trace_append(frag);

    /* Reset timers (write 60h to R4) */
    outp(addr_port, 0x04);
    outp(data_port, 0x60);
    /* Reset IRQ status (write 80h to R4) */
    outp(addr_port, 0x04);
    outp(data_port, 0x80);
    /* Read status — expect bits 7:6 = 00 */
    s1 = (unsigned char)(inp(addr_port) & 0xE0);
    sprintf(frag, "s1=%02X", s1);
    opl_trace_append(frag);
    if (s1 != 0) {
        /* OPL in an unexpected state — retry once */
        outp(addr_port, 0x04); outp(data_port, 0x60);
        outp(addr_port, 0x04); outp(data_port, 0x80);
        s1b = (unsigned char)(inp(addr_port) & 0xE0);
        sprintf(frag, "/s1b=%02X", s1b);
        opl_trace_append(frag);
        if (s1b != 0) {
            opl_trace_append(",absent");
            return 0;  /* no OPL present */
        }
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
    sprintf(frag, " s2=%02X", s2);
    opl_trace_append(frag);
    /* Reset */
    outp(addr_port, 0x04);
    outp(data_port, 0x60);
    outp(addr_port, 0x04);
    outp(data_port, 0x80);

    if ((s2 & 0xC0) != 0xC0) {
        opl_trace_append(",no-overflow");
        return 0;
    }

    /* OPL2 confirmed. OPL3 detection heuristic: OPL2 returns 0x06 in the
     * low 5 bits of status, OPL3 returns 0x00. */
    op = (unsigned char)(inp(addr_port) & 0x06);
    sprintf(frag, " op=%02X", op);
    opl_trace_append(frag);
    if (op == 0x00) {
        *out_opl3 = 1;
        opl_trace_append(",opl3");
    } else {
        opl_trace_append(",opl2");
    }
    return 1;
}

static int probe_opl(int *out_opl3)
{
    unsigned int sb_base = 0;
    char frag[32];

    /* Reset the trace buffer once per detect_audio invocation. Subsequent
     * probe_opl_at calls append their findings in order. */
    audio_opl_trace[0] = '\0';

    /* Prefer BLASTER-base+8 when BLASTER is set — covers CTCM-managed
     * Vibra 16S and similar PnP cards whose Adlib mirror is off. */
    if (parse_blaster(&sb_base) && sb_base != 0) {
        sprintf(frag, "blaster=%04X", sb_base);
        opl_trace_append(frag);
        if (probe_opl_at(sb_base + 0x08, sb_base + 0x09, out_opl3)) {
            opl_trace_append(" result=primary");
            return 1;
        }
        opl_trace_append(" fallback");
    } else {
        opl_trace_append("no-blaster");
    }

    /* Fallback: industry-standard Adlib port.
     * INVESTIGATION: suspected BEK-V409 NULL-write trigger. /SKIP:oplfb
     * disables this branch so the removal-at-a-time protocol can
     * isolate whether this path is what reaches near-DGROUP:0. */
    if (!crumb_skiplist_has("oplfb")) {
        crumb_enter("detect.audio.oplfb");
        if (probe_opl_at(OPL_DEFAULT_ADDR, OPL_DEFAULT_DATA, out_opl3)) {
            opl_trace_append(" result=fallback");
            crumb_exit();
            return 1;
        }
        crumb_exit();
    }
    opl_trace_append(" result=none");
    return 0;
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

/* Parse BLASTER for the T<n> token, which Creative's install sets to
 * identify the card family: T1 SB-orig, T3 SB Pro, T4 SB Pro 2, T6 SB16
 * (including Vibra 16), T8 AWE32, T9 AWE64. Used as a secondary
 * discriminator when DSP version alone isn't unique. Returns 1 and
 * writes the decimal value into *out_t on success; returns 0 if T
 * isn't present or isn't a valid single/two-digit decimal. */
static int parse_blaster_t(int *out_t)
{
    const char *env = getenv("BLASTER");
    const char *p;
    if (!env) return 0;
    for (p = env; *p; p++) {
        if ((*p == 'T' || *p == 't') && p[1] >= '0' && p[1] <= '9') {
            int v = 0;
            const char *q = p + 1;
            while (*q >= '0' && *q <= '9') { v = v*10 + (*q - '0'); q++; }
            if (v > 0 && v < 100) {
                *out_t = v;
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
/* Mixer chip probe (CT1745 discriminator)                                  */
/*                                                                          */
/* SB16 family carries a CT1745 mixer chip at BLASTER-base+4 (address) and  */
/* base+5 (data). Register 0x80 is the Interrupt Setup Register on CT1745; */
/* it encodes the programmed IRQ as a bitmap in the low nibble (IRQ 2/9 =  */
/* 0x01, 5 = 0x02, 7 = 0x04, 10 = 0x08) with the high nibble clear. SB Pro */
/* (CT1345) has no register at that index — open-bus reads return 0xFF, or */
/* on some bus-contention cases the index byte (0x80) is echoed back.      */
/*                                                                          */
/* Reference: Creative Labs Sound Blaster 16 Series Hardware Programming   */
/* Reference Manual, April 1994, §3.1 "Mixer Chip Registers".              */
/*                                                                          */
/* Returns a classification string (static lifetime, safe for report_add): */
/*   "CT1745"  — read looks like a valid Interrupt Setup byte              */
/*   "none"    — open-bus / index-echo / zero — no CT1745 at this base     */
/*   "unknown" — byte doesn't fit either bucket (weird mixer, not CT1745   */
/*              but also not clearly absent — needs human triage via       */
/*              Rule 7 WARN)                                                */
/* ----------------------------------------------------------------------- */

static const char *probe_mixer_chip(unsigned int sb_base)
{
    unsigned int addr = sb_base + 0x04;
    unsigned int data = sb_base + 0x05;
    unsigned char v;

    outp(addr, 0x80);
    /* Classic DOS I/O delay between index write and data read (S3
     * round-2 fix). The CT1745 needs a few hundred ns between address-
     * select (write to 0x04) and latched-data read (read from 0x05);
     * on fast hardware the consecutive outp/inp can complete inside
     * the mixer chip's settling window and return 0xFF or stale
     * contents. Port 0x80 is the POST diagnostic output port (AT) /
     * DMA channel-0 page register (XT); reading it is side-effect-free
     * on both and costs one ISA bus cycle (~1 μs), which is the
     * canonical DOS "jmp $+2" equivalent for forcing a pause between
     * back-to-back I/O operations. */
    (void)inp(0x80);
    v = (unsigned char)inp(data);

    if (v == 0xFF || v == 0x80 || v == 0x00) return "none";
    if ((v & 0xF0) == 0x00 && (v & 0x0F) != 0) return "CT1745";
    return "unknown";
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

    /* Issue #2 diagnostic: emit the full OPL probe trace so cross-boot
     * captures can be diffed to find the byte value that differs between
     * "opl3 detected" and "none detected" runs on the same Vibra 16 PnP
     * card. Silent-absent on builds without the trace buffer; but the
     * buffer is always initialized so the emit is always safe. */
    report_add_str(t, "audio.opl_probe_trace", audio_opl_trace,
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

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
        /* Mixer chip probe runs only when SB DSP answered — otherwise
         * there's no meaningful I/O base to probe against. Emitted
         * independently of DB lookup so Rule 7 can compare observed
         * against expected even when DB has no mixer_chip record. */
        report_add_str(t, "audio.mixer_chip_observed",
                       probe_mixer_chip(sb_base),
                       env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
    } else {
        report_add_str(t, "audio.sb_present", "no",
                       env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
    }

    /* Build composite match key and look up friendly entry. When we have
     * SB DSP, try the T-augmented key first (splits e.g. DSP 4.13 Vibra
     * from AWE32, both of which would otherwise land on opl3:040D). If
     * the T-augmented key doesn't resolve, fall back to the bare key. */
    if (!opl_present && !have_sb) {
        strcpy(audio_match_key, "pc-speaker-only");
        entry = audio_db_lookup(audio_match_key);
    } else if (have_sb) {
        int blaster_t = 0;
        entry = (const audio_db_entry_t *)0;
        if (parse_blaster_t(&blaster_t)) {
            sprintf(audio_match_key, "%s:%02X%02X:T%d", opl_token,
                    dsp_major & 0xFF, dsp_minor & 0xFF, blaster_t);
            entry = audio_db_lookup(audio_match_key);
        }
        if (!entry) {
            sprintf(audio_match_key, "%s:%02X%02X", opl_token,
                    dsp_major & 0xFF, dsp_minor & 0xFF);
            entry = audio_db_lookup(audio_match_key);
        }
    } else {
        sprintf(audio_match_key, "%s:none", opl_token);
        entry = audio_db_lookup(audio_match_key);
    }

    /* audio_match_key holds whichever key actually resolved (or the
     * last-tried key if none resolved, which is the raw fallback). */
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
        if (entry->mixer_chip && *entry->mixer_chip) {
            report_add_str(t, "audio.mixer_chip_expected", entry->mixer_chip,
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        }
    } else {
        report_add_str(t, "audio.detected", audio_match_key,
                       env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
    }
}
