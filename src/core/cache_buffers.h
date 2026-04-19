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
 *   - Callers MUST call `cache_buffers_reset()` before first read; the
 *     helper establishes a known-zero state regardless of prior consumers.
 *     This is the sole inter-caller ownership protocol — both diag_cache
 *     and bench_cache invoke it after their skip-checks and before any
 *     measurement loop, so whichever module runs second sees zeros rather
 *     than leftover pattern data from the first.
 *   - Buffers are NOT thread-safe. CERBERUS is single-threaded; the only
 *     contention is between diag_cache and bench_cache. Both call during
 *     their respective phases, which are serialized by main.
 */

/* Both byte sizes must fit in medium-model size_t (16-bit on Watcom
 * medium). 32768 fits; bumping CACHE_BUFFERS_LARGE_BYTES to 65536 or
 * above requires changing cache_buffers_reset() to loop _fmemset on
 * sub-64-KB chunks, because a single _fmemset(far_ptr, 0, 65536U) wraps
 * the count argument to zero. Any consumer that strides size as
 * `unsigned int j` (both stride loops in diag_cache and bench_cache do)
 * would also need a widening pass. */
#define CACHE_BUFFERS_SMALL_BYTES  2048U
#define CACHE_BUFFERS_LARGE_BYTES  32768U
#define CACHE_BUFFERS_LINE_SIZE    16U   /* 486 cache line — per-line
                                           stride granularity for
                                           diag_cache's stride_read_loop. */

/* Return pointers to the shared FAR buffers. Always succeed — allocation
 * happens at static link time; there is no runtime failure mode. */
unsigned char __far *cache_buffers_small(void);
unsigned char __far *cache_buffers_large(void);

/* Zero both buffers. Callers MUST invoke this before their first read of
 * either buffer; the helper establishes a known-zero starting state and
 * is the explicit inter-caller ownership protocol between diag_cache and
 * bench_cache. Cheap — two _fmemset calls totalling 34 KB, a small cost
 * paid once per module entry (not per-iteration). */
void cache_buffers_reset(void);

#endif
