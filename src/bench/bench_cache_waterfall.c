/*
 * Memory Cache Waterfall visual — v0.6.1 T4.
 *
 * Post-bench_memory visual. Nine horizontal bars, one per block size,
 * each filling left-to-right at a rate proportional to that block
 * size's measured write bandwidth to a RAM buffer. On cached systems
 * the small-block bars finish fast (L1 speed); the large-block bars
 * finish slowly (main-memory speed). The visible speed transition IS
 * the cache boundary.
 *
 * Text mode on every adapter. Block sizes swept:
 *   1B, 2B, 4B, 16B, 256B, 1KB, 4KB, 16KB, 64KB
 *
 * Measurement buffer is 64 KB FAR (segment-aligned) so it fits any
 * single write without wrap. Writes are timed via BIOS ticks; each
 * block size gets a minimum of 3 ticks (~165 ms) of work to land
 * above timing resolution even on fast cache.
 *
 * Render pattern: measure all 9 bandwidths first, then animate all
 * 9 bars in parallel — each char appearing at its band's cadence
 * so the waterfall reads as "fast bars finish first."
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include <malloc.h>
#include "bench.h"
#include "../core/display.h"
#include "../core/journey.h"

#define CW_COLS 80
#define CW_ROWS 25
#define CW_BANDS 9
#define CW_BAR_LEN 40
#define CW_BAR_COL 12
#define CW_BAR_ROW0 6

static const unsigned int cw_block_size[CW_BANDS] = {
    1, 2, 4, 16, 256, 1024, 4096, 16384, 65535
    /* 64KB - 1 because 64KB = 65536 doesn't fit in unsigned int */
};

static const char *const cw_block_label[CW_BANDS] = {
    "    1B", "    2B", "    4B", "   16B", "  256B",
    "   1KB", "   4KB", "  16KB", "  64KB"
};

/* Per-band measured rate in KB/s. */
static unsigned long cw_rates[CW_BANDS];

static unsigned char __far *cw_vram(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

static int cw_is_mono(void)
{
    adapter_t a = display_adapter();
    return (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
            a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO);
}

static void cw_putc(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v = cw_vram();
    unsigned int off = (unsigned int)((row * CW_COLS + col) * 2);
    v[off] = ch; v[off + 1] = attr;
}

static void cw_puts(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < CW_COLS) { cw_putc(row, col++, (unsigned char)*s++, attr); }
}

static void cw_fill(int r0, int r1, unsigned char ch, unsigned char attr)
{
    int r, c;
    for (r = r0; r <= r1; r++)
        for (c = 0; c < CW_COLS; c++) cw_putc(r, c, ch, attr);
}

static unsigned long cw_ticks(void)
{
    unsigned int __far *low  = (unsigned int __far *)MK_FP(0x0040, 0x006C);
    unsigned int __far *high = (unsigned int __far *)MK_FP(0x0040, 0x006E);
    unsigned int h1, h2, l;
    do { h1 = *high; l = *low; h2 = *high; } while (h1 != h2);
    return ((unsigned long)h1 << 16) | l;
}

/* Measure write bandwidth in KB/s for a given block size. Writes a
 * constant byte to `buf` in chunks of `block` bytes, looping until at
 * least `min_ticks` BIOS ticks have elapsed. Returns (bytes / ms). */
static unsigned long cw_measure(unsigned char __far *buf, unsigned long buf_size,
                                unsigned long block, unsigned long min_ticks)
{
    unsigned long start = cw_ticks();
    unsigned long bytes = 0;
    unsigned long elapsed;
    while ((cw_ticks() - start) < min_ticks) {
        /* Sweep the buffer with block-sized writes. */
        unsigned long off = 0;
        while (off + block <= buf_size) {
            unsigned char __far *p = buf + off;
            unsigned long i;
            for (i = 0; i < block; i++) p[i] = 0xA5;
            off += block;
            bytes += block;
        }
    }
    elapsed = cw_ticks() - start;
    if (elapsed == 0) return 0;
    /* bytes / (elapsed * 55ms) → KB/s = (bytes / (elapsed * 55)) * 1000 / 1024 */
    /* Simplify: KB/s ≈ bytes / (elapsed * 56) */
    return bytes / (elapsed * 56UL);
}

/* Pick a color class for this band's rate relative to max rate.
 *   fastest  → bright green
 *   mid      → yellow
 *   slowest  → red (or dim)
 * On mono: uses intensity + underscore. */
static unsigned char cw_band_attr(unsigned long rate, unsigned long max_rate,
                                  int mono)
{
    unsigned long ratio;
    if (max_rate == 0) return mono ? ATTR_NORMAL : ATTR_NORMAL;
    ratio = (rate * 100UL) / max_rate;
    if (mono) {
        return (ratio >= 50) ? ATTR_BOLD : ATTR_NORMAL;
    }
    if (ratio >= 66) return ATTR_GREEN | 0x08;   /* bright green */
    if (ratio >= 33) return ATTR_YELLOW;
    return ATTR_RED | 0x08;                      /* bright red */
}

void bench_cache_waterfall_visual(const opts_t *o)
{
    unsigned char __far *buf;
    unsigned long buf_size;
    unsigned long max_rate;
    int i;
    unsigned char title_attr, label_attr, frame_attr;

    if (journey_should_skip(o)) return;

    if (journey_title_card(o, HEAD_CENTER,
                           "MEMORY BANDWIDTH",
                           "Measuring how fast your CPU can move data "
                           "at different block sizes. Watch for the "
                           "speed change when writes exceed your cache.") == 1)
        return;

    /* Allocate a 32 KB FAR buffer. Don't take 64KB — DOS tends to fail
     * the 64KB allocation when TSR'd, and 32 KB is plenty of sweep for
     * the cache-hit / cache-miss transition. */
    buf_size = 32768UL;
    buf = (unsigned char __far *)_fmalloc(buf_size);
    if (buf == (unsigned char __far *)0) {
        journey_result_flash(o, "Cache waterfall: allocation failed, skipped");
        return;
    }

    if (cw_is_mono()) {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL; frame_attr = ATTR_NORMAL;
    } else {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL; frame_attr = ATTR_CYAN;
    }

    cw_fill(0, CW_ROWS - 1, ' ', ATTR_NORMAL);
    cw_puts(3, (CW_COLS - 26) / 2,
            "Memory Cache Waterfall", title_attr);

    /* Draw labels and frame brackets for all 9 bands */
    for (i = 0; i < CW_BANDS; i++) {
        int row = CW_BAR_ROW0 + i;
        cw_puts(row, 3, cw_block_label[i], label_attr);
        cw_putc(row, CW_BAR_COL - 1, '[', frame_attr);
        cw_putc(row, CW_BAR_COL + CW_BAR_LEN, ']', frame_attr);
    }

    cw_puts(16, 3, "Measuring ...", label_attr);

    /* Phase 1: measure each band */
    max_rate = 1;
    for (i = 0; i < CW_BANDS; i++) {
        /* Small min_ticks (2) keeps total measurement phase short; large
         * blocks still land above timing resolution because each loop
         * iteration writes many bytes. */
        unsigned long block = cw_block_size[i];
        cw_rates[i] = cw_measure(buf, buf_size, block, 2UL);
        if (cw_rates[i] > max_rate) max_rate = cw_rates[i];
        /* Skip check between bands */
        if (journey_poll_skip()) {
            _ffree(buf);
            return;
        }
    }

    /* Clear "Measuring..." */
    cw_puts(16, 3, "                    ", ATTR_NORMAL);

    /* Phase 2: animate each band's fill. All bands advance in parallel,
     * but fast bands advance more chars per frame than slow bands.
     * Frame cadence ~1 tick (~55 ms). Total animation ~30-50 frames. */
    {
        int progress[CW_BANDS];
        int done = 0;
        int frame;
        for (i = 0; i < CW_BANDS; i++) progress[i] = 0;

        for (frame = 0; frame < 40 && !done; frame++) {
            unsigned long tick_deadline;
            done = 1;
            for (i = 0; i < CW_BANDS; i++) {
                /* Advance this band by (rate / max_rate) * CW_BAR_LEN / 40
                 * chars per frame. */
                int target;
                int new_progress;
                int c;
                unsigned char attr = cw_band_attr(cw_rates[i], max_rate,
                                                  cw_is_mono());
                target = (int)((unsigned long)CW_BAR_LEN *
                               cw_rates[i] / max_rate);
                if (target > CW_BAR_LEN) target = CW_BAR_LEN;
                /* advance chars-per-frame scaled by rate */
                new_progress = progress[i] +
                    (int)((unsigned long)target / 20UL + 1UL);
                if (new_progress > target) new_progress = target;
                for (c = progress[i]; c < new_progress; c++) {
                    cw_putc(CW_BAR_ROW0 + i, CW_BAR_COL + c, 0xDB, attr);
                }
                progress[i] = new_progress;
                if (new_progress < target) done = 0;
            }

            /* Frame delay ~1 tick */
            tick_deadline = cw_ticks() + 1UL;
            while (cw_ticks() < tick_deadline) {
                if (journey_poll_skip()) { done = 1; break; }
            }
        }

        /* Draw final KB/s annotations on each band */
        for (i = 0; i < CW_BANDS; i++) {
            char buf_str[20];
            sprintf(buf_str, "%8lu KB/s", cw_rates[i]);
            cw_puts(CW_BAR_ROW0 + i, CW_BAR_COL + CW_BAR_LEN + 2,
                    buf_str, label_attr);
        }
    }

    _ffree(buf);

    journey_result_flash(o, "Memory: cache boundary visible in bandwidth plot");
}
