/*
 * Shared __far buffer storage for diag_cache and bench_cache. See
 * cache_buffers.h for the rationale and access rules.
 *
 * DGROUP accounting: these allocations are __far, meaning Watcom places
 * them in a dedicated far data segment outside DGROUP. The 34 KB of
 * storage here does NOT count against the 56,000-byte DGROUP ceiling.
 */

#include "cache_buffers.h"

static unsigned char __far small_buf[CACHE_BUFFERS_SMALL_BYTES];
static unsigned char __far large_buf[CACHE_BUFFERS_LARGE_BYTES];

unsigned char __far *cache_buffers_small(void) { return small_buf; }
unsigned char __far *cache_buffers_large(void) { return large_buf; }
