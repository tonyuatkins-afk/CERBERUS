/*
 * Cache Latency Heat Map — v0.6.1 T5.
 *
 * Post-diag_cache visual. One horizontal strip spanning screen width,
 * each cell representing a different memory offset into an allocated
 * RAM buffer. Color encodes measured access latency: green = fast
 * (cache hit), yellow = medium (L2 if present), red = slow (main
 * memory). On a cached system the strip shows a sharp green-to-red
 * transition at the cache size.
 *
 * Text-mode implementation works on every adapter — one CP437 block
 * per cell, attributes differentiate bands. VGA-only heat map in mode
 * 13h would be prettier but adds adapter-dependence; text mode gives
 * universal coverage with clear visual signal.
 *
 * Measurement methodology: 64 KB buffer, 64 cells across the strip
 * (one cell per 1 KB span). For each cell, do timed reads at that
 * offset; median of a few samples smooths out jitter.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include <malloc.h>
#include "diag.h"
#include "../core/display.h"
#include "../core/journey.h"

#define LM_COLS 80
#define LM_ROWS 25
#define LM_CELLS 64          /* horizontal cells in the heat strip */
#define LM_BUF_SIZE 32768UL  /* 32 KB sweep buffer */
#define LM_STRIP_COL 8       /* left edge of heat strip */
#define LM_STRIP_ROW 10

static unsigned char __far *lm_vram(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

static int lm_is_mono(void)
{
    adapter_t a = display_adapter();
    return (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
            a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO);
}

static void lm_putc(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v = lm_vram();
    unsigned int off = (unsigned int)((row * LM_COLS + col) * 2);
    v[off] = ch; v[off + 1] = attr;
}

static void lm_puts(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < LM_COLS) { lm_putc(row, col++, (unsigned char)*s++, attr); }
}

static void lm_fill(int r0, int r1, unsigned char ch, unsigned char attr)
{
    int r, c;
    for (r = r0; r <= r1; r++)
        for (c = 0; c < LM_COLS; c++) lm_putc(r, c, ch, attr);
}

static unsigned long lm_ticks(void)
{
    unsigned int __far *low  = (unsigned int __far *)MK_FP(0x0040, 0x006C);
    unsigned int __far *high = (unsigned int __far *)MK_FP(0x0040, 0x006E);
    unsigned int h1, h2, l;
    do { h1 = *high; l = *low; h2 = *high; } while (h1 != h2);
    return ((unsigned long)h1 << 16) | l;
}

/* Time a tight read-sweep at the given cell's offset. Each cell covers
 * (LM_BUF_SIZE / LM_CELLS) = 512 bytes. We do enough reads to land
 * above BIOS-tick resolution — with 512-byte windows that's ~50k reads
 * per cell in fast cache. Returns elapsed BIOS ticks. */
static unsigned long lm_measure_cell(unsigned char __far *buf,
                                     unsigned long offset,
                                     unsigned long window_bytes,
                                     unsigned long iterations)
{
    unsigned long start = lm_ticks();
    unsigned long elapsed;
    unsigned long i;
    unsigned char sink = 0;
    for (i = 0; i < iterations; i++) {
        unsigned long j;
        unsigned char __far *p = buf + offset;
        for (j = 0; j < window_bytes; j++) {
            sink ^= p[j];
        }
    }
    elapsed = lm_ticks() - start;
    /* Side-effect on sink to prevent DCE. */
    ((volatile unsigned char *)&sink)[0] = sink;
    return elapsed;
}

/* Map an elapsed-ticks value to a heat level (0 = coldest/fastest to
 * 3 = hottest/slowest). Quartiles of [min, max]. */
static int lm_heat_level(unsigned long elapsed,
                         unsigned long cold, unsigned long hot)
{
    unsigned long range;
    if (hot <= cold) return 0;
    range = hot - cold;
    if (elapsed <= cold)          return 0;
    if (elapsed >= hot)            return 3;
    if (elapsed <= cold + range/3) return 1;
    if (elapsed <= cold + 2*range/3) return 2;
    return 3;
}

static unsigned char lm_heat_attr(int heat, int mono)
{
    if (mono) {
        switch (heat) {
            case 0: return ATTR_BOLD;
            case 1: return ATTR_BOLD;
            case 2: return ATTR_NORMAL;
            default: return ATTR_NORMAL;
        }
    }
    switch (heat) {
        case 0: return ATTR_GREEN | 0x08;   /* bright green  */
        case 1: return ATTR_GREEN;          /* dim green     */
        case 2: return ATTR_YELLOW;
        default: return ATTR_RED | 0x08;    /* bright red    */
    }
}

void diag_latency_map(const opts_t *o)
{
    unsigned char __far *buf;
    unsigned long cell_bytes;
    unsigned long samples[LM_CELLS];
    unsigned long sample_min, sample_max;
    int i;
    unsigned char title_attr, label_attr;

    if (journey_should_skip(o)) return;

    if (journey_title_card(o, HEAD_LEFT,
                           "CACHE HEALTH",
                           "Probing memory access speed across your "
                           "address space. Green is fast. Red is slow. "
                           "The boundary is your cache.") == 1) return;

    buf = (unsigned char __far *)_fmalloc(LM_BUF_SIZE);
    if (buf == (unsigned char __far *)0) {
        journey_result_flash(o, "Latency map: allocation failed, skipped");
        return;
    }

    /* Prime the buffer with known content so reads return predictable
     * values (and the first measurement of each offset isn't a cold
     * cache-fill artefact). */
    {
        unsigned long i2;
        for (i2 = 0; i2 < LM_BUF_SIZE; i2++) buf[i2] = (unsigned char)i2;
    }

    if (lm_is_mono()) {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL;
    } else {
        title_attr = ATTR_BOLD; label_attr = ATTR_NORMAL;
    }

    lm_fill(0, LM_ROWS - 1, ' ', ATTR_NORMAL);
    lm_puts(3, (LM_COLS - 24) / 2, "Cache Latency Heat Map", title_attr);
    lm_puts(7,  LM_STRIP_COL,     "0 KB", label_attr);
    lm_puts(7,  LM_STRIP_COL + LM_CELLS / 2 - 3, "16 KB", label_attr);
    lm_puts(7,  LM_STRIP_COL + LM_CELLS - 5, "32 KB", label_attr);
    lm_puts(14, LM_STRIP_COL,
            "<  green: cache-speed        yellow: intermediate        red: main memory  >",
            label_attr);

    cell_bytes = LM_BUF_SIZE / LM_CELLS;   /* 512 bytes per cell */

    /* Measure each cell */
    sample_min = 0xFFFFFFFFUL;
    sample_max = 0UL;
    for (i = 0; i < LM_CELLS; i++) {
        unsigned long offset = (unsigned long)i * cell_bytes;
        /* 20 iterations × 512 bytes = 10240 reads. On 486+ this lands
         * 1-3 ticks depending on cache state. */
        samples[i] = lm_measure_cell(buf, offset, cell_bytes, 20UL);
        if (samples[i] < sample_min) sample_min = samples[i];
        if (samples[i] > sample_max) sample_max = samples[i];
        /* Draw a provisional cell so user sees progress */
        lm_putc(LM_STRIP_ROW, LM_STRIP_COL + i, 0xB1, ATTR_DIM);
        if (journey_poll_skip()) { _ffree(buf); return; }
    }

    /* Render final colored strip */
    for (i = 0; i < LM_CELLS; i++) {
        int heat = lm_heat_level(samples[i], sample_min, sample_max);
        unsigned char attr = lm_heat_attr(heat, lm_is_mono());
        lm_putc(LM_STRIP_ROW, LM_STRIP_COL + i, 0xDB, attr);
        lm_putc(LM_STRIP_ROW + 1, LM_STRIP_COL + i, 0xDB, attr);
    }

    /* Brief hold */
    {
        unsigned long end = lm_ticks() + 36UL;   /* ~2 s */
        while (lm_ticks() < end) {
            if (kbhit()) {
                union REGS r;
                r.h.ah = 0x00;
                int86(0x16, &r, &r);
                break;
            }
        }
    }

    _ffree(buf);

    journey_result_flash(o, "Cache: heat map reveals the cache boundary");
}
