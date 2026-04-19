#ifndef CERBERUS_CACHE_BUFFERS_H
#define CERBERUS_CACHE_BUFFERS_H

/*
 * Shared __far byte buffers used by the cache diagnostic (diag_cache) and
 * the cache bandwidth benchmark (bench_cache). Both consumers need a
 * buffer comfortably smaller than any plausible L1 (2 KB) and a buffer
 * comfortably larger than any plausible 486-class L1 (32 KB); duplicating
 * the allocation across two TUs would waste 34 KB of FAR storage and
 * introduce two sources of truth for the line-size constant. Centralizing
 * the allocation here keeps DGROUP untouched (these live in their own
 * FAR segment under medium model) and keeps the geometry single-sourced.
 *
 * Access rules:
 *   - Buffers are initialized to zero by the C runtime startup (BSS). No
 *     caller should assume any particular content.
 *   - Callers that write then read MUST treat the two buffers as volatile
 *     across calls; there is no inter-caller ownership protocol.
 *   - Buffers are NOT thread-safe. CERBERUS is single-threaded; the only
 *     contention is between diag_cache and bench_cache. Both call during
 *     their respective phases, which are serialized by main.
 */

#define CACHE_BUFFERS_SMALL_BYTES  2048U
#define CACHE_BUFFERS_LARGE_BYTES  32768U
#define CACHE_BUFFERS_LINE_SIZE    16U   /* 486 cache line — per-line
                                           stride granularity for
                                           diag_cache's stride_read_loop. */

/* Return pointers to the shared FAR buffers. Always succeed — allocation
 * happens at static link time; there is no runtime failure mode. */
unsigned char __far *cache_buffers_small(void);
unsigned char __far *cache_buffers_large(void);

#endif
