/*
 * Shared __far buffer storage for diag_cache and bench_cache. See
 * cache_buffers.h for the rationale and access rules.
 *
 * DGROUP accounting: these allocations are __far, meaning Watcom places
 * them in a dedicated far data segment outside DGROUP. The 34 KB of
 * storage here does NOT count against the 56,000-byte DGROUP ceiling.
 */

#include <string.h>
#include "cache_buffers.h"

static unsigned char __far small_buf[CACHE_BUFFERS_SMALL_BYTES];
static unsigned char __far large_buf[CACHE_BUFFERS_LARGE_BYTES];

unsigned char __far *cache_buffers_small(void) { return small_buf; }
unsigned char __far *cache_buffers_large(void) { return large_buf; }

/* Establish a known-zero state in both buffers. Called at the top of
 * diag_cache() and bench_cache() after their skip-checks, so whichever
 * module runs second sees zeros rather than leftover pattern data from
 * the first. Uses _fmemset which Watcom compiles to a REP STOSB over the
 * FAR pointer — the natural primitive for FAR-buffer clears. */
void cache_buffers_reset(void)
{
    _fmemset(small_buf, 0, CACHE_BUFFERS_SMALL_BYTES);
    _fmemset(large_buf, 0, CACHE_BUFFERS_LARGE_BYTES);
}
