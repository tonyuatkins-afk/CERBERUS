#ifndef CERBERUS_TIMING_H
#define CERBERUS_TIMING_H

#include "../cerberus.h"

typedef unsigned long us_t;
typedef unsigned int  ticks_t;

void          timing_init(void);
void          timing_start(void);
us_t          timing_stop(void);
us_t          timing_ticks_to_us(unsigned long ticks);
void          timing_wait_us(us_t microseconds);
int           timing_emulator_hint(void);

/* Pure helper — exposed so host tests can exercise both rollover branches.
 * PIT counts DOWN. Normal: start >= stop. Wrap: stop > start. */
unsigned long timing_elapsed_ticks(unsigned int start_count,
                                   unsigned int stop_count);

/* Pure helper — convert BIOS system-tick count (~54.925 ms/tick) to us.
 * Exposed for host tests. */
us_t          timing_bios_ticks_to_us(unsigned long bios_ticks);

/* Dual-clock self-check: span a real-time interval of `target_bios_ticks`
 * (1..4 typical) and return elapsed us computed two independent ways —
 * PIT Channel 2 (what timing_start/stop uses) and Channel-0-driven BIOS
 * tick. Both channels share the 1.193182 MHz crystal, so disagreement
 * between them points at a bug in timing.c's math (e.g. 16-bit overflow)
 * rather than a crystal drift. Returns 0 on success, nonzero if the PIT
 * C2 wrap counter looked unreliable. */
int           timing_dual_measure(unsigned int target_bios_ticks,
                                  us_t *out_pit_us,
                                  us_t *out_bios_us);

/* Runs timing_dual_measure and emits timing.cross_check.pit_us and
 * timing.cross_check.bios_us into the result table so consist_check can
 * interpret them. Call once per run, before consist_check. Lives behind
 * the CERBERUS_HOST_TEST guard in timing.c. */
void          timing_self_check(result_table_t *t);

#endif
