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

/* Long-interval timing via BIOS tick count. Resolution ~55 ms. Use
 * for multi-wrap intervals (>50 ms) where timing_start/timing_stop
 * would return modulo-wrap garbage — specifically Dhrystone and
 * Whetstone, which target 5-second runs (91 PIT C2 wraps). */
void          timing_start_long(void);
us_t          timing_stop_long(void);

/* Pure math kernel for timing_start_long/timing_stop_long — exposed
 * so host tests can exercise the BIOS-tick delta arithmetic without
 * poking the BDA. Returns (end_ticks - start_ticks) * 54925 us,
 * handling the midnight-rollover wrap at 0x1800B0 ticks (~24h) so a
 * benchmark that straddles midnight still produces the correct
 * elapsed us. Normal case: end >= start. Rollover: end < start. */
us_t          timing_bios_ticks_to_us_delta(unsigned long start_ticks,
                                            unsigned long end_ticks);

/* Pure helper — exposed so host tests can exercise both rollover branches.
 * PIT counts DOWN. Normal: start >= stop. Wrap: stop > start. */
unsigned long timing_elapsed_ticks(unsigned int start_count,
                                   unsigned int stop_count);

/* Pure helper — convert BIOS system-tick count (~54.925 ms/tick) to us.
 * Exposed for host tests. */
us_t          timing_bios_ticks_to_us(unsigned long bios_ticks);

/* Pure math kernel extracted from timing_dual_measure so it can be host
 * tested without touching hardware. Given the post-sync C2 reference
 * (initial_c2), the final C2 reading (final_c2), the wrap counter
 * observed by the polling loop (c2_wraps_observed), the BIOS-tick delta
 * across the measurement (bios_ticks_elapsed) and the target interval
 * (target_bios_ticks), compute the elapsed us on both clocks.
 *
 * Returns 0 on success, nonzero if the wrap-count sanity check trips
 * (implies the polling loop missed wraps and the PIT total would be
 * biased low). On nonzero return, *out_pit_us / *out_bios_us are not
 * updated. */
int           timing_compute_dual(unsigned int initial_c2,
                                  unsigned int final_c2,
                                  unsigned long c2_wraps_observed,
                                  unsigned long bios_ticks_elapsed,
                                  unsigned int target_bios_ticks,
                                  us_t *out_pit_us,
                                  us_t *out_bios_us);

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

/* v0.6.0 T6: PIT metronome visual — fires after timing_self_check.
 * Text mode + PC speaker clicks on each 18.2 Hz tick for ~4 s.
 * Skipped under /NOUI, /QUICK, or skip-all latch. */
void          timing_metronome_visual(const opts_t *o);

/* Pure emit helper (host-testable). Given a dual_measure return code
 * and the two us_t outputs, writes the appropriate rows into t:
 *   failure (rc != 0 or pit_us == 0 or bios_us == 0):
 *     timing.cross_check.status = "measurement_failed" (WARN)
 *     consistency.timing_self_check WARN (so the UI alert renderer,
 *       which filters on "consistency." prefix, surfaces the problem)
 *   success:
 *     timing.cross_check.pit_us, bios_us (numeric)
 *     timing.cross_check.status = "ok"
 * See timing_self_check for the contract and status key semantics. */
void          timing_emit_self_check(result_table_t *t,
                                     int dual_measure_rc,
                                     us_t pit_us,
                                     us_t bios_us);

#endif
