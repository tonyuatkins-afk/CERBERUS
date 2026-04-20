/*
 * REPSTOSD.EXE - Reference peak-throughput test for mode 13h VRAM writes.
 *
 * Purpose
 * -------
 * Issue #6 of CERBERUS reports bench_video.mode13h_kbps around 4.6 MB/s on
 * a Trio64 VLB card that theoretically supports 25-50 MB/s. The CERB-VOX
 * diagnostic (bench_video rebuilt at -ox instead of CFLAGS_NOOPT) showed
 * the CFLAGS_NOOPT tax is only 7-9%, so the bottleneck is somewhere other
 * than the compile flags: either C-loop overhead, or the hardware path
 * (ISA transfer mode, VLB slot not engaged, BIOS shadow settings).
 *
 * This binary isolates the C-loop variable. It writes to mode 13h VRAM
 * via a pure-assembly REP STOSD inner loop with zero C overhead. If it
 * produces ~5 MB/s, the bottleneck is the hardware path and the next
 * test is BIOS-Setup / physical-slot verification. If it produces 20+
 * MB/s, the bottleneck is C-loop overhead in CERBERUS's bench_video and
 * the fix lives in that module.
 *
 * Methodology
 * -----------
 * Matches CERBERUS bench_video mode 13h parameters: 64000 bytes per
 * iteration (size of the visible portion of A000 segment at 320x200x8)
 * multiplied by 2000 iterations for a 128 MB workload. 2000 was chosen
 * to produce at least ~90 BIOS ticks elapsed time on optimistic 25 MB/s
 * hardware (giving 1% tick-precision) while staying under 30 seconds
 * worst-case on pessimistic 5 MB/s hardware.
 *
 * Timing uses the BIOS tick counter at 0040:006C directly. Resolution
 * is ~54.9 ms per tick. The approximation `tick * 55 ms` is accepted
 * as a <1% systematic error against the actual 54.925 ms value.
 *
 * The inner loop writes a fixed 32-bit pattern (0x55AA55AA) to the full
 * mode 13h framebuffer 2000 times. DOS sees no output during the run.
 * The visible display IS the VRAM being written to - you will see a
 * checkerboard-ish pattern flashing for the duration.
 *
 * The program saves and restores the original video mode so it is
 * safe to run from any text-mode DOS prompt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <i86.h>

/* Inner loop: write 32000 words (64000 bytes) to ES:DI using REP STOSW
 * at the tightest possible encoding. Watcom 16-bit pragma aux cannot
 * emit 32-bit register forms (no eax/ecx), so we use the 16-bit word
 * store. On a 32-bit VLB data path the hardware can still combine two
 * adjacent word writes into one 32-bit bus cycle; on pure 16-bit ISA
 * each word is a bus cycle. Either way this measures the ceiling the
 * hardware path allows with zero C-loop overhead.
 *
 * Caller responsibility: ES must be 0xA000 (framebuffer segment) and
 * DI must be 0. */
extern void rep_stosw_one_frame(void);
#pragma aux rep_stosw_one_frame = \
    "mov ax, 55AAh" \
    "mov cx, 32000" \
    "rep stosw" \
    modify [ax cx di];

/* Set up segment registers before the inner loop. Separated from
 * rep_stosd_one_frame so the ES/DI setup cost is not counted in the
 * per-iteration overhead. Loading ES once per loop instead of per
 * iteration is a correctness requirement, not an optimization: an inner
 * loop that reloads ES each time would measure a different thing. */
extern void load_vga_es(void);
#pragma aux load_vga_es = \
    "mov ax, 0A000h" \
    "mov es, ax" \
    "xor di, di" \
    modify [ax di es];

#define ITER_COUNT     2000UL
#define BYTES_PER_ITER 64000UL

int main(int argc, char *argv[])
{
    union REGS r;
    unsigned long __far *tick_ptr =
        (unsigned long __far *)MK_FP(0x0040, 0x006C);
    unsigned long tick_start, tick_end, tick_delta;
    unsigned long total_bytes, total_kb;
    unsigned long elapsed_ms, kbps;
    unsigned long i;
    unsigned char original_mode;

    (void)argc; (void)argv;

    printf("REPSTOSD - mode 13h VRAM peak-throughput reference\n");
    printf("Issue #6 test-2: isolate hardware path vs C-loop overhead\n\n");

    /* Read current mode via INT 10h AH=0Fh. Bit 7 of AL can carry the
     * 'no-clear' flag on VGA; mask it off before using as a mode number. */
    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    original_mode = (unsigned char)(r.h.al & 0x7F);

    /* Set mode 13h (320x200x256 VGA). */
    r.h.ah = 0x00;
    r.h.al = 0x13;
    int86(0x10, &r, &r);

    /* Load the VRAM segment once. Inner loop assumes ES=A000, DI=0 at
     * entry to each iteration; since each iteration writes 64000 bytes
     * starting at DI=0 and increments to DI=64000, we reset DI via
     * load_vga_es before each call. */

    /* Warmup: one iteration outside the timing window to let any
     * one-time bus arbitration settle. */
    load_vga_es();
    rep_stosw_one_frame();

    /* Timing window */
    tick_start = *tick_ptr;
    for (i = 0UL; i < ITER_COUNT; i++) {
        load_vga_es();
        rep_stosw_one_frame();
    }
    tick_end = *tick_ptr;

    /* Restore original video mode before any printf lands. */
    r.h.ah = 0x00;
    r.h.al = original_mode;
    int86(0x10, &r, &r);

    /* Handle the midnight-rollover case: if the run crossed 24:00:00 the
     * BIOS tick counter wraps back to 0 at 0x001800B0 (1,573,040). A 128 MB
     * run at even 1 MB/s finishes in minutes, so this branch is unlikely
     * but cheap to cover. */
    if (tick_end >= tick_start) {
        tick_delta = tick_end - tick_start;
    } else {
        tick_delta = (0x001800B0UL - tick_start) + tick_end;
    }
    if (tick_delta == 0UL) tick_delta = 1UL;

    total_bytes = ITER_COUNT * BYTES_PER_ITER;   /* 128,000,000 bytes */
    total_kb = total_bytes / 1024UL;             /* 125,000 KB */
    elapsed_ms = tick_delta * 55UL;              /* ~1% systematic error */

    if (elapsed_ms > 0UL) {
        kbps = (total_kb * 1000UL) / elapsed_ms;
    } else {
        kbps = 0UL;
    }

    printf("Methodology: pure REP STOSW, 16-bit words, ES=A000, DI=0\n");
    printf("Per-iteration: %lu bytes (full mode 13h visible frame)\n",
           BYTES_PER_ITER);
    printf("Iterations:    %lu\n", ITER_COUNT);
    printf("Total written: %lu bytes (%lu KB)\n", total_bytes, total_kb);
    printf("BIOS ticks:    %lu (approx %lu ms at 55 ms/tick)\n",
           tick_delta, elapsed_ms);
    printf("Throughput:    %lu KB/s\n", kbps);
    printf("\n");
    printf("Interpretation vs CERBERUS bench_video mode13h_kbps:\n");
    printf("  If ~5 MB/s : issue #6 bottleneck is hardware path\n");
    printf("  If 20+ MB/s: issue #6 bottleneck is C-loop overhead\n");

    return 0;
}
