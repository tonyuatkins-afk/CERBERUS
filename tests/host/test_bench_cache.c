/*
 * Host-side test for bench_cache's pure-math kernel. Semantics:
 *
 *   bench_cache_kb_per_sec(bytes, elapsed_us) computes
 *
 *       (bytes / 1024) * 1000 / (elapsed_us / 1000)
 *
 *   — pre-divide-by-1024 scaling sized for bench_cache's envelope
 *   (up to 100 MB per call over 1+ second). Zero bytes, zero elapsed, or
 *   sub-millisecond elapsed all return zero; the full kernel contract is
 *   documented at bench_cache_kb_per_sec in src/bench/bench_cache.c.
 *
 *   The sub-ms branch cannot fire in the real bench_cache path because
 *   timing_start_long / timing_stop_long resolves to BIOS-tick precision
 *   (~55,000 us minimum interval). The defensive zero-return is there
 *   for host-test coverage and future timing-path changes.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"

/* bench_cache.c pulls cache_buffers.h (for the geometry constants) and
 * allocates __far buffers it never touches in the host path. The __far
 * keyword is a Watcom extension; on the NT target it's a no-op, so the
 * allocation is harmless for a host unit test — we only exercise the
 * kb_per_sec kernel, nothing that touches the buffers. */
#include "../../src/core/cache_buffers.c"

/* bench_cache.c expects timing_start_long / timing_stop_long in its link
 * graph because bench_cache_one calls them. The host test only exercises
 * bench_cache_kb_per_sec, which is a pure function that doesn't touch
 * timing — but including bench_cache.c pulls in the symbol references, so
 * we provide stubs consistent with test_diag_cache's approach. */
#include "../../src/core/timing.h"
void   timing_start_long(void) { /* no-op stub */ }
us_t   timing_stop_long(void)  { return (us_t)0; }
void   timing_start(void)      { /* no-op stub */ }
us_t   timing_stop(void)       { return (us_t)0; }

#include "../../src/bench/bench_cache.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

int main(void)
{
    unsigned long got;
    printf("=== CERBERUS host unit test: bench_cache kb_per_sec ===\n");

    /* Scenario A: zero elapsed → zero rate (defensive). Prevents a
     * divide-by-zero on a timing-degenerate run; the bench_cache
     * orchestration separately treats zero elapsed as an error via
     * its own `bench.cache.status` path. */
    got = bench_cache_kb_per_sec(4096UL, 0UL);
    CHECK(got == 0UL, "Scenario A: elapsed=0 → rate=0");

    /* Scenario B: 4 KB transferred in 1000 us (1 ms) = 4000 KB/s.
     *   kb = 4, elapsed_ms = 1, result = 4 * 1000 / 1 = 4000. */
    got = bench_cache_kb_per_sec(4096UL, 1000UL);
    CHECK(got == 4000UL, "Scenario B: 4 KB in 1 ms → 4000 KB/s");

    /* Scenario C: sub-millisecond elapsed — the kernel returns zero
     * because bench_cache's BIOS-tick timing path cannot observe intervals
     * shorter than ~55,000 us. If a future timing-path change lowers that
     * floor, this scenario catches the rate-computation regression rather
     * than silently emitting garbage numbers for the 486 DX-2 L1 case. */
    got = bench_cache_kb_per_sec(4096UL, 189UL);
    CHECK(got == 0UL, "Scenario C: sub-ms elapsed → rate=0 (bench_cache floor is 1 ms)");

    /* Scenario D: RAM read bench envelope. 32 KB × 200 iters = 6.4 MB of
     * bytes moved over a ~1.5 sec BIOS-tick window (1,500,000 us).
     *   bytes    = 32768 * 200 = 6553600
     *   step1    = 6553600 * 1000 / 1024 = 6400000
     *   result   = 6400000 * 1000 / 1500000 = 4266
     * — this represents ~4.3 MB/s, plausible for 486 DX-2 DRAM reads. */
    got = bench_cache_kb_per_sec(6553600UL, 1500000UL);
    CHECK(got == 4266UL, "Scenario D: RAM-read envelope → 4266 KB/s");

    /* Scenario E: L1 read bench envelope. 2 KB × 50000 iters = 100 MB
     * over ~1.5 sec.
     *   bytes    = 2048 * 50000 = 102400000
     *   step1    = 102400000 * 1000 / 1024 = 100000000
     *   result   = 100000000 * 1000 / 1500000 = 66666
     * — ~67 MB/s, plausible for 486 DX-2 L1 cached sustained reads. */
    got = bench_cache_kb_per_sec(102400000UL, 1500000UL);
    CHECK(got == 66666UL, "Scenario E: L1-read envelope → 66666 KB/s");

    /* Scenario F: very-slow RAM read on a 386 DX-40 class box. 32 KB ×
     * 200 iters / 8000000 us = ~800 KB/s. Sanity-check the low end of
     * the operating range. */
    got = bench_cache_kb_per_sec(6553600UL, 8000000UL);
    CHECK(got == 800UL, "Scenario F: slow-RAM envelope → 800 KB/s");

    /* Scenario G: very-fast L1 on a hypothetical future box — 100 MB in
     * 500 ms. 200 MB/s. Check the kernel doesn't overflow on faster-than-
     * 486 hardware.
     *   bytes    = 102400000; step1 = 100000000
     *   result   = 100000000 * 1000 / 500000 = 200000 */
    got = bench_cache_kb_per_sec(102400000UL, 500000UL);
    CHECK(got == 200000UL, "Scenario G: fast-L1 envelope → 200000 KB/s");

    /* Scenario H: zero bytes → zero rate. Degenerate but the kernel must
     * not divide-by-zero or mis-report. */
    got = bench_cache_kb_per_sec(0UL, 1000UL);
    CHECK(got == 0UL, "Scenario H: bytes=0 → rate=0");

    /* Scenario I: upper-envelope smoke test. 64 MB in 1 sec.
     *   bytes    = 67108864
     *   step1    = 67108864 * 1000 / 1024 = 65536000
     *   result   = 65536000 * 1000 / 1000000 = 65536
     * Verify no overflow at the advertised upper bound. */
    got = bench_cache_kb_per_sec(67108864UL, 1000000UL);
    CHECK(got == 65536UL, "Scenario I: 64 MB in 1 sec → 65536 KB/s");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
