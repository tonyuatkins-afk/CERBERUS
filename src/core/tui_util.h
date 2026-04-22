#ifndef CERBERUS_TUI_UTIL_H
#define CERBERUS_TUI_UTIL_H

/*
 * Shared text-mode UI primitives — v0.6.2 T1.
 *
 * Six visual modules (journey, bit_parade, lissajous, metronome,
 * audio_scale, cache_waterfall, latency_map) each carried a private
 * copy of the same ~60-line VRAM-helper block: vram_base selection
 * by adapter, is_mono predicate, putc / puts / fill / hline / ticks.
 * This module centralizes those primitives.
 *
 * Direct VRAM writes (B800:0000 color / B000:0000 mono) — same
 * pattern as ui.c, preserved so rendering doesn't disturb scrolled
 * stdout from earlier detect/diag/bench output.
 *
 * Each function is small and static-inline-friendly; Watcom with -ox
 * should inline the per-module call sites back into the callers.
 */

#define TUI_COLS 80
#define TUI_ROWS 25

/* Adapter-aware base segment selector. B000 mono / B800 color. */
unsigned char __far *tui_vram_base(void);

/* 1 if adapter is MDA/Hercules/EGA_MONO/VGA_MONO. */
int  tui_is_mono(void);

/* Write one cell (char + attr) at (row, col). No bounds check. */
void tui_putc(int row, int col, unsigned char ch, unsigned char attr);

/* Write a NUL-terminated string starting at (row, col). Silently
 * truncates at col 80. */
void tui_puts(int row, int col, const char *s, unsigned char attr);

/* Fill every cell in rows [r0, r1] inclusive with (ch, attr). */
void tui_fill(int r0, int r1, unsigned char ch, unsigned char attr);

/* Horizontal rule spanning cols [c0, c1] on `row` with `glyph, attr`. */
void tui_hline(int row, int c0, int c1, unsigned char glyph,
               unsigned char attr);

/* Position the BIOS cursor at (row, col) so the DOS prompt lands
 * there when the program exits. */
void tui_cursor(int row, int col);

/* Atomic-read 32-bit BIOS tick count at 0040:006C (18.2 Hz). */
unsigned long tui_ticks(void);

/* Non-blocking key-waiting poll. Returns 1 if a key is in the buffer. */
int  tui_kbhit(void);

/* Blocking read. *ascii = ASCII byte (0 for extended key),
 * *scan = scan code. */
void tui_read_key(unsigned char *ascii, unsigned char *scan);

/* Drain any pending keys from the BIOS buffer. Use before an
 * interactive hold to avoid stale keystroke pre-emption. */
void tui_drain_keys(void);

/* v0.8.0-M3.1: synchronize to CGA retrace edge before a VRAM write
 * to avoid snow on single-ported IBM CGA. No-op on non-CGA adapters.
 * tui_putc calls this internally; code that maintains its own private
 * VRAM helpers (ui.c predates this module) should call it explicitly
 * before each write-to-VRAM. Per 0.8.0 plan M3 exit gate. */
void tui_wait_cga_retrace_edge(void);

#endif
