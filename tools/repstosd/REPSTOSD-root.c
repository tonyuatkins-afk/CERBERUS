/*
 * REPSTOSD.EXE - issue #6 diagnostic quartet member
 *
 * Minimal DOS real-mode binary that writes 100 MB to mode 13h
 * VRAM via an inner loop that is pure NASM-compiled REP STOSD.
 * No CERBERUS dependencies, no CFLAGS_NOOPT layer, no detection
 * or consistency engine. Measures peak achievable bandwidth the
 * hardware + bus path delivers when the C-loop overhead is
 * eliminated from the measurement.
 *
 * Interpretation for issue #6:
 *  - If the ~4.6 MB/s CERBERUS bench_video result is a C-loop
 *    overhead artifact, this binary will show a markedly higher
 *    number (20+ MB/s expected on healthy 486 VLB hardware).
 *  - If the result here also lands in the 4 to 6 MB/s band, the
 *    hardware path is the ceiling and CERBERUS's measurement is
 *    accurate. UMC491 + Trio64V+ VLB genuinely delivers this
 *    speed and issue #6 closes as working-as-designed.
 *
 * Output format: single line "REPSTOSD: N MB/s" for easy paste.
 */

#include <stdio.h>
#include <dos.h>
#include <i86.h>

/* Inner loop: 16000-dword (64000-byte) REP STOSD frame write,
 * matching bench_video's mode 13h frame size so the per-call
 * workload is apples-to-apples with CERBERUS's measurement. */
extern void rep_stosd_frame(void);
#pragma aux rep_stosd_frame "*_"

#define ITER_COUNT      1562UL           /* 16000 * 4 * 1562 = 99,968,000 bytes */
#define BYTES_PER_ITER  64000UL
#define TICK_US         54925UL          /* standard BIOS tick */

int main(int argc, char *argv[])
{
    union REGS r;
    unsigned long __far *tick = (unsigned long __far *)MK_FP(0x0040, 0x006C);
    unsigned long t_start, t_end, t_delta;
    unsigned long total_bytes, elapsed_us, mb_per_sec_x100;
    unsigned long kb_per_sec;
    unsigned long i;
    unsigned char original_mode;

    (void)argc; (void)argv;

    /* Capture current video mode so we can restore on exit. Bit 7
     * of the returned AL carries the no-clear flag on VGA; mask
     * it off before treating AL as a mode number. */
    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    original_mode = (unsigned char)(r.h.al & 0x7F);

    /* Enter mode 13h. BIOS handles the CRTC programming and
     * palette load; we just write into A000 after this. */
    r.h.ah = 0x00;
    r.h.al = 0x13;
    int86(0x10, &r, &r);

    /* Warmup: one iteration outside the timing window so the
     * first-write bus-arbitration effects do not show up in the
     * measurement. */
    rep_stosd_frame();

    /* Timing window. BIOS tick at 0040:006C advances via the
     * PIT C0 IRQ every ~54.9 ms. For a workload that takes
     * 4 seconds (at 25 MB/s) through 20 seconds (at 5 MB/s),
     * we see 72 to 364 ticks, precision 0.3% to 1.4%. */
    t_start = *tick;
    for (i = 0UL; i < ITER_COUNT; i++) {
        rep_stosd_frame();
    }
    t_end = *tick;

    /* Restore original video mode. Do this before any printf so
     * the text lands in the restored mode, not mode 13h. */
    r.h.ah = 0x00;
    r.h.al = original_mode;
    int86(0x10, &r, &r);

    /* Handle midnight rollover if it happened (tick counter wraps
     * at 0x001800B0). The workload cannot be that long so this
     * branch is defensive. */
    if (t_end >= t_start) {
        t_delta = t_end - t_start;
    } else {
        t_delta = (0x001800B0UL - t_start) + t_end;
    }
    if (t_delta == 0UL) t_delta = 1UL;

    total_bytes = ITER_COUNT * BYTES_PER_ITER;
    elapsed_us = t_delta * TICK_US;

    /* MB/s with two decimal places of precision.
     * Want: bytes * 100 / (elapsed_us / 1000000) / (1024*1024)
     *   = bytes * 100 * 1000000 / (elapsed_us * 1024 * 1024)
     *   = bytes * 100000000 / (elapsed_us * 1048576)
     * bytes is ~10^8, elapsed_us is ~10^7. To stay in 32-bit:
     * precompute kb/s first = bytes * 1000 / (elapsed_ms)
     * with elapsed_ms = elapsed_us / 1000. Then
     * mb_per_sec_x100 = kb_per_sec * 100 / 1024. */
    {
        unsigned long elapsed_ms = elapsed_us / 1000UL;
        unsigned long total_kb;
        if (elapsed_ms == 0UL) elapsed_ms = 1UL;
        total_kb = total_bytes / 1024UL;            /* 97,625 KB */
        kb_per_sec = (total_kb * 1000UL) / elapsed_ms;
        mb_per_sec_x100 = (kb_per_sec * 100UL) / 1024UL;
    }

    printf("REPSTOSD: %lu.%02lu MB/s  (%lu KB/s over %lu ticks, %lu bytes)\n",
           mb_per_sec_x100 / 100UL, mb_per_sec_x100 % 100UL,
           kb_per_sec, t_delta, total_bytes);
    printf("Methodology: pure REP STOSD inner loop, 32-bit dwords,\n");
    printf("  operand-size override emitted by NASM, ES=A000, DI=0.\n");
    return 0;
}
