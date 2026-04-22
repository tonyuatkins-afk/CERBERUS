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
 *     timing.pit_jitter_pct (v0.7.1: derived from the two clocks)
 * See timing_self_check for the contract and status key semantics. */
void          timing_emit_self_check(result_table_t *t,
                                     int dual_measure_rc,
                                     us_t pit_us,
                                     us_t bios_us);

/* =====================================================================
 * v0.7.1 additions: measurement stats + RDTSC backend + method info
 *
 * The original PIT-C2 / BIOS-tick API stays exactly as-is. These new
 * primitives layer on top: a timing_stats_t accumulator for repeat-
 * with-jitter measurement, and an opt-in RDTSC path for Pentium-class
 * CPUs where available. The emit helpers put all of this into the INI
 * under the timing.* section so downstream tools know which clock the
 * numbers came from and how tight the repeat spread was.
 * ===================================================================== */

typedef enum {
    TM_BIOS  = 0,      /* 54.925 ms/tick BIOS system timer */
    TM_PIT   = 1,      /* PIT Channel 2 gate, ~838 ns/tick */
    TM_RDTSC = 2       /* Pentium+ 64-bit cycle counter (low 32 bits used) */
} timing_method_t;

/* Repeat-measurement accumulator. The workflow is:
 *   timing_stats_init(&s);
 *   for (i = 0; i < N; i++) {
 *     timing_start(); <kernel>; us = timing_stop();
 *     timing_stats_add(&s, us);
 *   }
 *   timing_stats_finalize(&s);
 *
 * After finalize, s.mean_us / s.min_us / s.max_us / s.range_pct /
 * s.confidence are populated. range_pct = (max-min)*100/mean, clamped
 * at 999 for pathological inputs. confidence is derived via the pure
 * helper below; callers MAY override if they have domain knowledge.
 *
 * No floating point; all 32-bit integer math. Safe to use in bench
 * kernels compiled with or without -fpi. */
typedef struct {
    us_t          mean_us;
    us_t          min_us;
    us_t          max_us;
    unsigned int  n;
    unsigned int  range_pct;    /* (max-min)*100/mean, 0..999 */
    confidence_t  confidence;   /* derived at finalize, overridable */
} timing_stats_t;

void          timing_stats_init(timing_stats_t *s);
void          timing_stats_add(timing_stats_t *s, us_t sample);
void          timing_stats_finalize(timing_stats_t *s);

/* Pure map from range_pct to a CONF level. Thresholds:
 *   0..5%   -> CONF_HIGH
 *   6..20%  -> CONF_MEDIUM
 *   21%+    -> CONF_LOW
 * Host-testable. Used by timing_stats_finalize and also exposed for
 * callers that want to apply the same criterion to externally-computed
 * jitter numbers (e.g. cache probe confidence). */
confidence_t  timing_confidence_from_range_pct(unsigned int range_pct);

/* Short human-readable token for INI emission: "bios", "pit", "rdtsc". */
const char *  timing_method_name(timing_method_t m);

/* ---- RDTSC backend (HW-gated, behind CERBERUS_HOST_TEST guard) ----- */

/* 1 if this CPU advertises CPUID + leaf 1 EDX bit 4 (TSC). Result
 * cached after first call. Depends on cpu_get_class() having run
 * (Phase 1 detection) — returns 0 if class is UNKNOWN or pre-486. */
int           timing_has_rdtsc(void);

/* Low 32 bits of the TSC. Returns 0 if RDTSC is unavailable. Callers
 * interested in intervals should subtract two reads with unsigned
 * wrap: (end - start) handles single mid-interval wrap automatically. */
unsigned long timing_rdtsc_lo(void);

/* Best-effort CPU MHz estimate, calibrated once via RDTSC vs a 4-
 * BIOS-tick (~220 ms) interval. Returns 0 if RDTSC is unavailable,
 * if the measurement looks degenerate, or if the emulator flag is
 * set. Result cached after first call. */
unsigned long timing_cpu_mhz_est(void);

/* Returns the highest-fidelity clock available on this CPU: RDTSC if
 * present and non-emulator, else PIT (if timing_init's self-test
 * passed), else BIOS. This is the value emitted as timing.method in
 * the INI. */
timing_method_t timing_best_method(void);

/* Emit timing.method and timing.cpu_mhz into the result table. Call
 * once per run, alongside timing_self_check. Safe to call multiple
 * times (report_update_str semantics — but avoid, since downstream
 * consumers read these keys only at INI write time). */
void          timing_emit_method_info(result_table_t *t);

#endif
