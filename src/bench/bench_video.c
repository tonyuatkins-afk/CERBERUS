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
 * the synthetic-loop assumption. Additionally a per-iter volatile
 * observer (bench_video_iter_observer) runs inside vram_write_loop,
 * mirroring bench_cache_iter_observer — belt-and-braces alongside the
 * CFLAGS_NOOPT compile, so a future revert to -ox still can't fold the
 * outer loop via cross-iteration DSE.
 *
 * Known issue (deferred to v0.5 cleanup): the CFLAGS_NOOPT makefile
 * variable name is misleading — it means "no optimization" but the value
 * is actually -od -oi, which still enables intrinsic inlining. Same
 * issue flagged in bench_cache.c's header. Both benchmarks share the
 * variable, so renaming is a multi-file refactor tracked separately.
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

/* Per-iteration volatile observer. File-scope + volatile + external
 * linkage — Watcom -ox cannot prove this is unobserved between iters,
 * so it cannot fold the inner write loop across iterations (classic
 * cross-iteration DSE). Matches bench_cache_iter_observer's pattern. */
volatile unsigned char bench_video_iter_observer;

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
        /* Per-iter volatile observer — matches bench_cache_iter_observer.
         * Defeats cross-iteration DSE by forcing each iteration's writes
         * to be observable (reads back a byte just written at a wrapped
         * offset). Belt-and-braces alongside the CFLAGS_NOOPT compile.
         *
         * Modulo, not bitmask: bench_cache uses (size - 1U) as a mask
         * because its buffers are powers of two. bench_video's mode 13h
         * buffer is 64000 (NOT a power of two), so a mask would produce
         * a non-uniform wrap that could walk past size. The 32-bit
         * divide costs ~5300 total (5000 text + 300 mode13h) ≈ 0.5 ms
         * on a 486 — negligible against ~1-2 sec run times. */
        bench_video_iter_observer = vram[(unsigned int)(i % (unsigned long)size)];
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
            /* Skip path returns 0: a skip is NOT a measurement failure.
             * Returning 1 here would OR into bench_video's any_zero and
             * publish bench.video.status=warn_zero_elapsed, which means
             * "timing came back degenerate" — a different signal entirely.
             * The legitimate degenerate-rate path at the end of this
             * function still returns 1 (rate == 0 after measurement). */
            report_add_str(t, "bench.video.text_status",
                           "skipped_unknown_adapter",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return 0;
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
    unsigned char saved_mode;

    if (a != ADAPTER_VGA_COLOR && a != ADAPTER_MCGA) {
        report_add_str(t, "bench.video.mode13h_status",
                       "skipped_non_vga",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return 0;
    }
    /* Fail-safe gate: /ONLY:BENCH skips detect, so environment.emulator
     * is absent from the table. Proceeding with mode 13h under an
     * emulator mangles the host terminal; the original code short-
     * circuited FALSE on the NULL result and ran the mode switch anyway,
     * defeating the gate. Mirror bench_cache.c:283-288's
     * skipped_detect_not_run precedent. */
    emu = find_local(t, "environment.emulator");
    if (!emu) {
        report_add_str(t, "bench.video.mode13h_status",
                       "skipped_detect_not_run",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return 0;
    }
    if (emu->type == V_STR && emu->v.s &&
        strcmp(emu->v.s, "none") != 0) {
        report_add_str(t, "bench.video.mode13h_status",
                       "skipped_emulator",
                       CONF_HIGH, VERDICT_UNKNOWN);
        return 0;
    }

    /* Save the current video mode BEFORE switching, so we can restore to
     * whatever the user was running (mode 07 mono, a non-default text
     * mode, etc.) — the previous unconditional AL=03h restore dropped
     * mode 07 users into mode 03h. INT 10h AH=0Fh returns: AL=current
     * video mode, AH=column count, BH=active display page. */
    regs.h.ah = 0x0F;
    int86(0x10, &regs, &regs);
    saved_mode = regs.h.al;

    /* INT 10h AH=00h AL=13h — set mode 13h (320x200x256, VGA). */
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);

    vram = (unsigned char __far *)MK_FP(0xA000, 0x0000);

    timing_start_long();
    vram_write_loop(vram, BENCH_VIDEO_M13H_BYTES, BENCH_VIDEO_M13H_ITERS);
    elapsed = timing_stop_long();

    /* Restore the entry mode. Happens after timing_stop so the mode-
     * switch cost isn't charged to the measurement.
     *
     * Caveat: INT 10h AH=00h always clears the screen for the target
     * mode, so any text transcript on screen when the bench started is
     * gone — this is inherent to any mode 13h round-trip (mode 13h entry
     * already wiped the text-mode buffer). What THIS fix addresses is
     * geometry/attribute mismatch: a mode-07 user now gets mode 07 back
     * instead of being dumped into mode 03. */
    regs.h.ah = 0x00;
    regs.h.al = saved_mode;
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
