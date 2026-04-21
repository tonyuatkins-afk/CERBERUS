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
#include "../core/journey.h"

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
 * CFLAGS_NOOPT note in the module header).
 *
 * observer_mask convention (avoids per-iter 32-bit modulo):
 *   - non-zero: AND the iteration index with this mask to pick an
 *     observer offset. Callers whose buffers are powers of two pass
 *     (size - 1U). The text loop (size=4096) passes 4095U.
 *   - zero: use the iteration index directly as the offset. Callers
 *     MUST guarantee iters <= size so the index never walks past the
 *     buffer. The mode 13h loop (size=64000, iters=300) uses this.
 * Prior implementation used `i % size` which invoked Watcom's _U4D
 * helper (~150 cycles on 8088); at 5000 iterations that was ~15%
 * measurement bias on floor hardware. */
static void vram_write_loop(unsigned char __far *vram, unsigned int size,
                             unsigned long iters, unsigned int observer_mask)
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
         * to be observable. Belt-and-braces alongside CFLAGS_NOOPT. */
        bench_video_iter_observer = vram[observer_mask
            ? (unsigned int)(i & (unsigned long)observer_mask)
            : (unsigned int)i];
    }
}

/* Text-mode benchmark. Picks the segment by the CURRENT video mode
 * (INT 10h AH=0Fh) rather than adapter class: an EGA-color adapter
 * running mode 7 has text VRAM at B000, not B800; a prior program may
 * have left the adapter in a graphics mode; and the active display
 * page may not be page 0. Saves the 4 KB, runs the timed write loop,
 * restores. Returns 0 on success, 1 if the measurement is degenerate
 * (zero elapsed or kbps math returned 0). */
static int bench_video_text(result_table_t *t)
{
    unsigned int seg;
    unsigned char __far *vram;
    us_t elapsed;
    unsigned long rate;
    unsigned int j;
    union REGS regs;
    unsigned char cur_mode;
    unsigned char active_page;

    /* INT 10h AH=0Fh returns AL=current mode, AH=columns, BH=active
     * page. Mask bit 7 of AL — some BIOSes set it to signal "no-clear"
     * on the last mode set, which confuses a raw switch(). */
    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x0F;
    int86(0x10, &regs, &regs);
    cur_mode = (unsigned char)(regs.h.al & 0x7FU);
    active_page = regs.h.bh;

    switch (cur_mode) {
        case 0: case 1: case 2: case 3:
            seg = 0xB800;
            break;
        case 7:
            seg = 0xB000;
            break;
        default:
            /* Graphics mode or unknown text mode — skip. A skip returns
             * 0 because it is NOT a measurement failure; returning 1
             * would propagate into bench_video's any_zero and publish
             * warn_zero_elapsed, which means "timing degenerate". */
            report_add_str(t, "bench.video.text_status",
                           "skipped_not_text_mode",
                           CONF_HIGH, VERDICT_UNKNOWN);
            return 0;
    }
    /* Page stride differs by mode: 40x25 modes (0/1) allocate 0x0800
     * bytes per page (2000 displayed + padding), 80x25 modes (2/3/7)
     * allocate 0x1000. A hardcoded 0x1000 on mode 0/1 with active_page
     * > 0 would point past the live page into the next page's displayed
     * region — the bench would corrupt and then "restore" the wrong
     * bytes, visibly mangling the adjacent page. F1 from the v0.4 QG
     * R3 characterization. */
    {
        unsigned int page_stride = (cur_mode <= 1U) ? 0x0800U : 0x1000U;
        vram = (unsigned char __far *)MK_FP(seg,
            (unsigned int)active_page * page_stride);
    }

    /* Capture the live display bytes so they can be put back after. */
    for (j = 0U; j < BENCH_VIDEO_TEXT_BYTES; j++) {
        bench_video_saved_vram[j] = vram[j];
    }

    timing_start_long();
    vram_write_loop(vram, BENCH_VIDEO_TEXT_BYTES,
                    BENCH_VIDEO_TEXT_ITERS,
                    BENCH_VIDEO_TEXT_BYTES - 1U);
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
     * video mode, AH=column count, BH=active display page.
     *
     * Zero the REGS union before each int86 call: otherwise residual BL/
     * BH/CX state from the prior call leaks into the next BIOS call.
     * Some BIOSes treat AL bit 7 on AH=00h as "preserve VRAM"; carrying
     * garbage into unrelated registers risks triggering vendor-specific
     * paths. */
    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x0F;
    int86(0x10, &regs, &regs);
    /* Mask bit 7 of AL — some VGA BIOSes reflect the "no-clear VRAM"
     * flag in the returned mode byte. Writing that value back via
     * AH=00h would honor the flag and preserve the garbled mode-13h
     * VRAM during restore, leaving the user's display corrupted. The
     * text-mode path at line 153 already masks; this parity with F2
     * from the v0.4 QG R3 characterization closes the inconsistency. */
    saved_mode = (unsigned char)(regs.h.al & 0x7FU);

    /* INT 10h AH=00h AL=13h — set mode 13h (320x200x256, VGA). */
    memset(&regs, 0, sizeof(regs));
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);

    vram = (unsigned char __far *)MK_FP(0xA000, 0x0000);

    timing_start_long();
    /* observer_mask=0: iters (300) < size (64000), so i is always a
     * valid offset; skip the mask AND avoid a per-iter 32-bit modulo. */
    vram_write_loop(vram, BENCH_VIDEO_M13H_BYTES,
                    BENCH_VIDEO_M13H_ITERS, 0U);
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
    memset(&regs, 0, sizeof(regs));
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
    /* v0.6.0 T8: title card framing for the Video Bandwidth section.
     * The pattern fill IS the visual — the measurement and the
     * demonstration are the same code path. */
    (void)journey_title_card(o, HEAD_CENTER,
                             "VIDEO BANDWIDTH",
                             "Measuring how fast your CPU can push "
                             "pixels to your video card's frame buffer. "
                             "The pattern you see IS the measurement.");
    /* Warn the user before obliterating their display: the text-mode
     * bench writes ~20 MB through live VRAM (visible garble for ~1 sec)
     * and the mode 13h bench clears the screen via INT 10h mode switch.
     * Without this line users panic and Ctrl-C. */
    puts("[bench.video] display will flicker for ~2 sec - mode 13h will clear screen");
    any_zero |= bench_video_text(t);
    any_zero |= bench_video_mode13h(t);
    /* After mode 13h round-trip, screen is cleared. Identify the
     * transition so subsequent bench output isn't confusing. */
    puts("[bench.video] mode 13h done; screen restored");
    if (any_zero) {
        report_add_str(t, "bench.video.status",
                       "warn_zero_elapsed",
                       CONF_HIGH, VERDICT_WARN);
    } else {
        report_add_str(t, "bench.video.status", "ok",
                       CONF_HIGH, VERDICT_UNKNOWN);
    }
}
