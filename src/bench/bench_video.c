/*
 * Video bandwidth benchmark. Two VRAM write-throughput numbers —
 * text-mode (any adapter) and mode 13h (VGA color / MCGA only). See
 * docs/plans/v0.4-benchmarks-and-polish.md §2.
 *
 * KEYS
 *   bench.video.text_write_kbps   — stride-byte writes to the detected
 *                                   text-mode segment (B000 for MDA/HGC/
 *                                   EGA-mono/VGA-mono; B800 for CGA/EGA-
 *                                   color/VGA-color/MCGA). Writes over
 *                                   the live 80x25 page; save-restore
 *                                   pattern preserves user-visible state.
 *   bench.video.mode13h_kbps      — stride-byte writes to A000:0000
 *                                   during VGA mode 13h (320x200x8bpp).
 *                                   Requires INT 10h mode-switch to 13h
 *                                   and back to text mode 03h. Gated on
 *                                   VGA adapter AND environment.emulator=
 *                                   none (mode-switch under emulators
 *                                   produces unreliable numbers).
 *
 * The save/restore pattern borrows from diag_dma's DMA-count-register
 * approach: capture the 4 KB that the bench pattern will overwrite, run
 * the timed loop, put the captured bytes back. Brief display garble
 * during the ~1 second write is intentional; CGA "snow" is not
 * suppressed because retrace sync would add variable delays that
 * corrupt the bandwidth measurement.
 *
 * DCE: VRAM writes target memory-mapped hardware whose content is NOT
 * provably unread by the compiler — Watcom cannot DCE them. Nevertheless
 * bench_video.obj compiles at CFLAGS_NOOPT like bench_cache and the
 * historical benchmarks, so future toolchain drift cannot invalidate
 * the synthetic-loop assumption.
 *
 * Timing: uses timing_start_long / timing_stop_long (BIOS-tick ~55 ms
 * resolution) because the text-write loop runs ~1 second on 486-class
 * hardware and mode 13h runs ~1-2 seconds, both past the single-wrap
 * PIT limit.
 *
 * Iteration calibration (BIOS-tick resolution ≥ 10 ticks = ~9%):
 *   - Text 4 KB × 5000 iters = 20 MB. 486 DX-2 VRAM write ~15-25 MB/s
 *     → ~0.8-1.3 sec (~15-24 ticks). Slower 486 / 386 → more ticks,
 *     better resolution.
 *   - Mode 13h 64000 B × 300 iters = 18.75 MB. ISA VGA write ~12-18 MB/s
 *     → ~1.0-1.6 sec. VLB / PCI VGA runs faster; still stays above the
 *     10-tick floor.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"
#include "../core/display.h"

#define BENCH_VIDEO_TEXT_BYTES   4096U
#define BENCH_VIDEO_TEXT_ITERS   5000UL
#define BENCH_VIDEO_M13H_BYTES   64000U
#define BENCH_VIDEO_M13H_ITERS     300UL

/* Save buffer for the 4 KB of text-mode VRAM that the text-write loop
 * temporarily overwrites. FAR so it doesn't weigh on DGROUP — 4 KB of
 * near data would push past the 56,000-byte ceiling after all v0.4
 * deliverables land. */
static unsigned char __far bench_video_saved_vram[BENCH_VIDEO_TEXT_BYTES];

static const result_t *find_local(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

/* Stride-byte write loop to the supplied VRAM base. Pattern is
 * (iter_low_byte ^ offset_low_byte) so each byte stored is a function
 * of both loop indices and the Watcom optimizer cannot hoist any store
 * across iterations even at -ox (which bench_video doesn't use — see
 * CFLAGS_NOOPT note in the module header). */
static void vram_write_loop(unsigned char __far *vram, unsigned int size,
                             unsigned long iters)
{
    unsigned long i;
    unsigned int j;
    unsigned char pattern;
    for (i = 0UL; i < iters; i++) {
        pattern = (unsigned char)(i & 0xFFUL);
        for (j = 0U; j < size; j++) {
            vram[j] = (unsigned char)(pattern ^ (unsigned char)j);
        }
    }
}

/* Text-mode benchmark. Picks the segment by adapter class (same mapping
 * diag_video uses), saves the 4 KB, runs the timed write loop, restores.
 * Returns 0 on success, 1 if the measurement is degenerate (adapter
 * unknown, zero elapsed, or kbps math returned 0). */
static int bench_video_text(result_table_t *t)
{
    adapter_t a = display_adapter();
    unsigned int seg;
    unsigned char __far *vram;
    us_t elapsed;
    unsigned long rate;
    unsigned int j;

    switch (a) {
        case ADAPTER_MDA:
        case ADAPTER_HERCULES:
        case ADAPTER_EGA_MONO:
        case ADAPTER_VGA_MONO:
            seg = 0xB000;
            break;
        case ADAPTER_CGA:
        case ADAPTER_EGA_COLOR:
        case ADAPTER_VGA_COLOR:
        case ADAPTER_MCGA:
            seg = 0xB800;
            break;
        default:
            report_add_str(t, "bench.video.text_status",
                           "skipped_unknown_adapter",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return 1;
    }
    vram = (unsigned char __far *)MK_FP(seg, 0x0000);

    /* Capture the live display bytes so they can be put back after. */
    for (j = 0U; j < BENCH_VIDEO_TEXT_BYTES; j++) {
        bench_video_saved_vram[j] = vram[j];
    }

    timing_start_long();
    vram_write_loop(vram, BENCH_VIDEO_TEXT_BYTES, BENCH_VIDEO_TEXT_ITERS);
    elapsed = timing_stop_long();

    /* Restore display. This happens AFTER timing_stop, so the restore
     * cost doesn't contaminate the measurement. */
    for (j = 0U; j < BENCH_VIDEO_TEXT_BYTES; j++) {
        vram[j] = bench_video_saved_vram[j];
    }

    rate = bench_cache_kb_per_sec(
        (unsigned long)BENCH_VIDEO_TEXT_BYTES * BENCH_VIDEO_TEXT_ITERS,
        (unsigned long)elapsed);
    report_add_u32(t, "bench.video.text_write_kbps", rate,
                   (const char *)0, CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.video.text_write_us",
                   (unsigned long)elapsed, (const char *)0,
                   CONF_HIGH, VERDICT_UNKNOWN);
    return rate == 0UL ? 1 : 0;
}

/* Mode 13h benchmark. Gated on VGA/MCGA (BIOS VGA required for mode
 * 13h) AND environment.emulator=none (mode switches under emulators
 * mangle the host terminal and produce unreliable timings). Returns 0
 * on success, 1 on degenerate measurement; the skip path also returns
 * 0 because skipping is not a measurement failure. */
static int bench_video_mode13h(result_table_t *t)
{
    adapter_t a = display_adapter();
    const result_t *emu;
    unsigned char __far *vram;
    us_t elapsed;
    unsigned long rate;
    union REGS regs;

    if (a != ADAPTER_VGA_COLOR && a != ADAPTER_MCGA) {
        report_add_str(t, "bench.video.mode13h_status",
                       "skipped_non_vga",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return 0;
    }
    emu = find_local(t, "environment.emulator");
    if (emu && emu->type == V_STR && emu->v.s &&
        strcmp(emu->v.s, "none") != 0) {
        report_add_str(t, "bench.video.mode13h_status",
                       "skipped_emulator",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return 0;
    }

    /* INT 10h AH=00h AL=13h — set mode 13h (320x200x256, VGA). */
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);

    vram = (unsigned char __far *)MK_FP(0xA000, 0x0000);

    timing_start_long();
    vram_write_loop(vram, BENCH_VIDEO_M13H_BYTES, BENCH_VIDEO_M13H_ITERS);
    elapsed = timing_stop_long();

    /* Restore text mode 03h — 80x25 color. Happens after timing_stop so
     * the mode-switch cost isn't charged to the measurement. */
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);

    rate = bench_cache_kb_per_sec(
        (unsigned long)BENCH_VIDEO_M13H_BYTES * BENCH_VIDEO_M13H_ITERS,
        (unsigned long)elapsed);
    report_add_u32(t, "bench.video.mode13h_kbps", rate,
                   (const char *)0, CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "bench.video.mode13h_us",
                   (unsigned long)elapsed, (const char *)0,
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "bench.video.mode13h_status", "ok",
                   CONF_HIGH, VERDICT_UNKNOWN);
    return rate == 0UL ? 1 : 0;
}

void bench_video(result_table_t *t, const opts_t *o)
{
    int any_zero = 0;
    (void)o;
    any_zero |= bench_video_text(t);
    any_zero |= bench_video_mode13h(t);
    if (any_zero) {
        report_add_str(t, "bench.video.status",
                       "warn_zero_elapsed",
                       CONF_HIGH, VERDICT_WARN);
    } else {
        report_add_str(t, "bench.video.status", "ok",
                       CONF_HIGH, VERDICT_UNKNOWN);
    }
}
