/*
 * Video RAM diagnostic — Phase 2 Task 2.4.
 *
 * Write/read test patterns on a 64-byte region of video memory outside
 * the active display area. Saves the original bytes before probing and
 * restores them after, so the running CERBERUS display is not disturbed.
 *
 * Safe-offset strategy per adapter class:
 *   MDA / Hercules   B000:0F80  — past the 80x25 visible area
 *   CGA / EGA / VGA  B800:2F80  — end of CGA page 3 (off-screen)
 *   (pure-graphics VGA modes aren't exercised by CERBERUS — we only
 *    run in text mode 03h, so B800 is populated even on VGA)
 *
 * Patterns: 0x00, 0xFF, 0xAA, 0x55 — the classic four-pattern set.
 * Catches stuck bits and shorted data lines within the probe region.
 *
 * This is a plausibility check, not an exhaustive VRAM sweep. A full
 * walk of all video memory needs mode switches (to blank the screen
 * while testing), CGA snow handling, and EGA/VGA plane-by-plane
 * separation — each a meaningful implementation effort. For v0.3
 * minimum the small-region probe catches the common "video RAM
 * chip is dead" case without touching the live display.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include "diag.h"
#include "../core/display.h"
#include "../core/report.h"

#define PROBE_LEN 64

static int vram_probe(unsigned int segment, unsigned int offset,
                      unsigned int *out_bad_pattern, unsigned int *out_bad_offset)
{
    unsigned char __far *vram = (unsigned char __far *)MK_FP(segment, offset);
    unsigned char saved[PROBE_LEN];
    static const unsigned char patterns[4] = { 0x00, 0xFF, 0xAA, 0x55 };
    unsigned int p, i;
    int result = 1;

    /* Save original bytes */
    for (i = 0; i < PROBE_LEN; i++) saved[i] = vram[i];

    for (p = 0; p < 4; p++) {
        unsigned char want = patterns[p];
        for (i = 0; i < PROBE_LEN; i++) vram[i] = want;
        for (i = 0; i < PROBE_LEN; i++) {
            if (vram[i] != want) {
                *out_bad_pattern = (unsigned int)want;
                *out_bad_offset  = i;
                result = 0;
                goto restore;
            }
        }
    }

restore:
    /* Always restore original bytes, even on fault */
    for (i = 0; i < PROBE_LEN; i++) vram[i] = saved[i];
    return result;
}

void diag_video(result_table_t *t)
{
    adapter_t    a = display_adapter();
    unsigned int seg, off;
    unsigned int bad_pattern = 0, bad_offset = 0;
    char detail[80];

    switch (a) {
        case ADAPTER_MDA:
        case ADAPTER_HERCULES:
            seg = 0xB000; off = 0x0F80;
            break;
        case ADAPTER_CGA:
        case ADAPTER_EGA_COLOR:
        case ADAPTER_EGA_MONO:
        case ADAPTER_VGA_COLOR:
        case ADAPTER_VGA_MONO:
        case ADAPTER_MCGA:
            seg = 0xB800; off = 0x2F80;
            break;
        default:
            report_add_str(t, "diagnose.video", "skipped (unknown adapter)",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return;
    }

    if (vram_probe(seg, off, &bad_pattern, &bad_offset)) {
        report_add_str(t, "diagnose.video.pattern", "pass",
                       CONF_HIGH, VERDICT_PASS);
        report_set_verdict(t, "video.adapter", VERDICT_PASS);
    } else {
        sprintf(detail, "VRAM pattern %02X failed at %04X:%04X+%u",
                bad_pattern, seg, off, bad_offset);
        report_add_str(t, "diagnose.video.pattern", detail,
                       CONF_HIGH, VERDICT_FAIL);
        report_set_verdict(t, "video.adapter", VERDICT_FAIL);
    }
}
