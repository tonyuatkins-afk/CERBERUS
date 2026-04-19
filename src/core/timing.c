/*
 * CERBERUS timing subsystem
 *
 * PIT Channel 2 gate-based measurement. Channel 2 is used because its gate
 * is software-controlled (via port 61h bit 0) without affecting Channel 0,
 * the BIOS system tick.
 *
 * Resolution: ~838ns per PIT tick (1.193182 MHz).
 *
 * CRITICAL: PIT counts DOWN in Mode 2. The "normal" case is stop_count <
 * start_count (counter decreased). Wraparound is stop_count > start_count
 * (counter passed zero and reloaded). Getting this backwards silently
 * corrupts every measurement on slow real hardware where the wrap case is
 * actually exercised (DOSBox-X rarely exercises wrap because emulation is
 * too fast — this makes the bug invisible in emulation).
 *
 * The math primitives below are guarded against CERBERUS_HOST_TEST so the
 * host unit tests can exercise them without dragging in <conio.h>.
 */

#include "timing.h"
#include "report.h"
#include <stdio.h>

/* ----------------------------------------------------------------------- */
/* Pure portable math — host-testable                                      */
/* ----------------------------------------------------------------------- */

us_t timing_ticks_to_us(unsigned long ticks)
{
    /*
     * Target: ticks * (1000000 / 1193182) microseconds.
     * Approximation used: ticks * 838 / 1000, +500 for round-to-nearest.
     * Systematic error ~0.01% — acceptable for the ±10% benchmark match goal.
     * MUST use unsigned long throughout — ticks * 838 overflows 16-bit int
     * at ticks >= 78 (feedback_dos_16bit_int.md).
     */
    return (us_t)((ticks * 838UL + 500UL) / 1000UL);
}

unsigned long timing_elapsed_ticks(unsigned int start_count,
                                   unsigned int stop_count)
{
    /*
     * PIT Channel 2 counts DOWN in Mode 2, reloading from 0xFFFF at zero.
     * If the counter didn't wrap during measurement, start > stop and
     * elapsed = start - stop.
     * If the counter wrapped once, stop > start and we need to add 65536.
     * Equal values are the zero-elapsed edge case.
     */
    if (stop_count <= start_count) {
        return (unsigned long)start_count - (unsigned long)stop_count;
    } else {
        return 65536UL + (unsigned long)start_count - (unsigned long)stop_count;
    }
}

us_t timing_bios_ticks_to_us(unsigned long bios_ticks)
{
    /*
     * One BIOS system tick = 65536 PIT cycles at 1.193182 MHz =
     * 54925.4 us. We round to 54925 us/tick for 32-bit integer math;
     * the 0.0007% systematic error is below any useful threshold.
     *
     * bios_ticks * 54925UL overflows unsigned long at bios_ticks
     * >= 78199 (~71 minutes). No measurement we take spans that long,
     * but the caller should never hand us a pathological value from
     * an uninitialized BIOS-tick read.
     */
    return (us_t)(bios_ticks * 54925UL);
}

int timing_compute_dual(unsigned int initial_c2,
                        unsigned int final_c2,
                        unsigned long c2_wraps_observed,
                        unsigned long bios_ticks_elapsed,
                        unsigned int target_bios_ticks,
                        us_t *out_pit_us,
                        us_t *out_bios_us)
{
    /*
     * Pure math kernel for the dual-clock self-check. Kept outside the
     * CERBERUS_HOST_TEST guard so host builds can link and exercise it
     * without poking PIT ports. timing_dual_measure calls this after
     * the HW polling loop concludes. See timing_dual_measure for the
     * semantics of each argument and why initial_c2 is the load-bearing
     * reference (counter is mid-count after sync-to-edge).
     *
     * Watcom C89: hoist locals to the function top (no mid-block
     * declarations allowed).
     */
    unsigned long c2_wraps;
    unsigned long sub_ticks;
    unsigned long c2_total_ticks;
    us_t          pit_us, bios_us;

    if (target_bios_ticks == 0) target_bios_ticks = 1;

    /* Defense-in-depth: HW layer bails at c2_wraps>8, but a direct caller
     * (host tests, future refactor) could hand us a pathological wrap count
     * that overflows timing_ticks_to_us's 32-bit intermediate. Cap at 16
     * (2x any realistic target_bios_ticks).
     *
     * Because the lower-bail requires `c2_wraps + 1 >= target_bios_ticks`
     * AND the upper-bail rejects `c2_wraps > 16`, this kernel can only
     * accommodate `target_bios_ticks <= 17`. Any future self-check variant
     * that wants a longer interval (target=18+) must either (a) raise this
     * cap or (b) chain multiple timing_dual_measure calls. The current
     * timing_self_check uses target=4; well within the cap. */
    if (c2_wraps_observed > 16UL) return 1;

    /* Upper-bound sanity check: PIT C2 should wrap at most once per BIOS
     * tick (they share the 1.193 MHz crystal with a 65536:1 ratio). A
     * genuine measurement over `target_bios_ticks` BIOS ticks sees at
     * most `target_bios_ticks + 1` C2 wraps (the +1 accounts for a
     * partial wrap boundary at each end).
     *
     * If we observed more than that, phantom wraps from chipset latch
     * races slipped past the shape-check + verify-reread defenses in the
     * observer loop. Observed on the 486 DX-2 bench box: exactly 6 wraps
     * over a 4-tick target = PIT reports 1.5x the real interval.
     *
     * Reject the measurement as unreliable. Rule 4a will no-op on key
     * absence (timing_emit_self_check emits measurement_failed status
     * and a consistency WARN row in the emit helper's failure branch). */
    if (c2_wraps_observed > (unsigned long)target_bios_ticks) return 1;

    c2_wraps = c2_wraps_observed;

    /* Expected: one C2 wrap per BIOS tick (they share the crystal at
     * 1:65536). If we counted materially fewer than target_bios_ticks,
     * something caused the poll loop to miss wraps — could be a chatty
     * TSR hogging INT 8, or just slow iron under load. Bail rather
     * than emit biased data; rule 4a will then correctly no-op on key
     * absence. The +1 tolerance accounts for the partial tick at the
     * end that may not have rolled over yet.
     *
     * The `c2_wraps + 1` tolerance is calibrated for target_bios_ticks=4
     * (the `timing_self_check` call site). Lower targets (1 or 2) may
     * false-positive on fast machines where fewer wraps are observed
     * than BIOS ticks elapsed; if we add a lower-target call site,
     * re-derive this tolerance. */
    if (c2_wraps + 1UL < (unsigned long)target_bios_ticks) {
        return 1;
    }

    /* C2 counts DOWN. We measure elapsed sub-wrap ticks relative to
     * initial_c2 (the post-sync starting value), NOT 0xFFFF — the
     * counter was already mid-count when initial_c2 was captured, so
     * assuming 0xFFFF would inflate pit_us by however many ticks had
     * already elapsed since c2_gate_on() (up to one full wrap / 55 ms
     * because of sync-to-edge). Total elapsed C2 ticks across the
     * whole interval = wraps * 65536 + (initial_c2 - final_c2). */
    if (final_c2 <= initial_c2) {
        sub_ticks = (unsigned long)initial_c2 - (unsigned long)final_c2;
    } else {
        /* Defensive: if the loop's wrap-edge detection ever undercounts
         * and we exit with final_c2 numerically greater than initial_c2
         * (e.g. initial_c2 was captured low in the count, multiple
         * wraps elapsed, final c2_now landed high), account for the
         * extra wrap. */
        c2_wraps++;
        sub_ticks = 65536UL + (unsigned long)initial_c2
                            - (unsigned long)final_c2;
    }
    c2_total_ticks = c2_wraps * 65536UL + sub_ticks;
    pit_us  = timing_ticks_to_us(c2_total_ticks);
    bios_us = timing_bios_ticks_to_us(bios_ticks_elapsed);

    /* Post-defensive sanity: if pit_us and bios_us diverge by more than
     * 25%, the measurement is unreliable — either the defensive wrap
     * increment fired on a legitimate near-equal init/final pair, or a
     * phantom wrap slipped past the shape-check + verify-reread in the
     * observer loop. Rejecting here keeps biased data out of the INI;
     * rule 4a will no-op on key absence and timing_emit_self_check will
     * surface a WARN row instead.
     *
     * 25% threshold picked to be well above expected jitter on healthy
     * hardware (typically <2%) and below the 49% ratio seen on the 486
     * DX-2 bench box where the defensive increment fires spuriously. */
    if (pit_us > bios_us && (pit_us - bios_us) > (bios_us >> 2)) return 1;
    if (bios_us > pit_us && (bios_us - pit_us) > (bios_us >> 2)) return 1;

    if (out_pit_us)  *out_pit_us  = pit_us;
    if (out_bios_us) *out_bios_us = bios_us;
    return 0;
}

/* ----------------------------------------------------------------------- */
/* Self-check emit helper — pure, host-testable                            */
/* ----------------------------------------------------------------------- */

/* Single-call contract: timing_self_check is invoked exactly ONCE per
 * cerberus run (from main, during startup). report_add_* stores the
 * pointer we hand it, so the display storage must outlive the call —
 * hence file-scope static rather than stack-local. If this ever grows
 * a second call site, each call site MUST have its own dedicated
 * static buffer; reusing these would silently clobber the earlier
 * entry's displayed value. The same contract applies to any future
 * per-subsystem timing analyzers added to this file.
 *
 * Moved above the HW guard (alongside timing_emit_self_check) so host
 * tests can exercise the emit path without pulling in <conio.h>. */
static char self_check_pit_display[16];
static char self_check_bios_display[16];

void timing_emit_self_check(result_table_t *t,
                            int dual_measure_rc,
                            us_t pit_us,
                            us_t bios_us)
{
    /*
     * Pure emit helper for timing_self_check. Takes the result of a
     * dual_measure attempt (rc + the two us_t outputs) and writes the
     * appropriate rows into the result table:
     *
     *   failure path (rc != 0 || either us_t is zero):
     *     timing.cross_check.status = "measurement_failed" (WARN)
     *     consistency.timing_self_check WARN row (so the UI alert
     *       renderer — which filters on "consistency." — surfaces
     *       the problem to the user instead of burying it in the
     *       INI where only a post-mortem reader would find it)
     *
     *   success path:
     *     timing.cross_check.pit_us  (numeric)
     *     timing.cross_check.bios_us (numeric)
     *     timing.cross_check.status  = "ok"
     *
     * Note: string literals used for status + self-check message have
     * static lifetime, so they're safe to hand to report_add_str.
     * The numeric displays use self_check_*_display file-scope statics
     * per the single-call contract documented above.
     */
    if (dual_measure_rc != 0 || pit_us == 0 || bios_us == 0) {
        /* Status key is purely informational (VERDICT_UNKNOWN): no UI
         * renderer surfaces raw timing.cross_check.* rows. The
         * user-visible WARN lives on the consistency row below, which
         * the TUI alert renderer picks up by filtering on the
         * "consistency." prefix. Both branches of this function emit
         * the status key with VERDICT_UNKNOWN for symmetry — the WARN
         * semantic is carried exclusively by the consistency row on
         * the failure path. */
        report_add_str(t, "timing.cross_check.status",
                       "measurement_failed",
                       CONF_LOW, VERDICT_UNKNOWN);
        /* Emit as consistency row ONLY on the failure path — the
         * UI alert renderer filters on "consistency." prefix, so
         * without this line the WARN is invisible in the TUI
         * summary. On success, rule 4a's
         * consistency.timing_independence PASS/WARN row already
         * handles UI visibility, so we intentionally do NOT emit
         * a consistency row here in the "ok" branch. */
        report_add_str(t, "consistency.timing_self_check",
                       "WARN: PIT/BIOS dual measurement unreliable "
                       "- timing cross-check skipped",
                       CONF_LOW, VERDICT_WARN);
        return;
    }

    sprintf(self_check_pit_display,  "%lu", (unsigned long)pit_us);
    sprintf(self_check_bios_display, "%lu", (unsigned long)bios_us);
    report_add_u32(t, "timing.cross_check.pit_us",
                   (unsigned long)pit_us, self_check_pit_display,
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "timing.cross_check.bios_us",
                   (unsigned long)bios_us, self_check_bios_display,
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "timing.cross_check.status", "ok",
                   CONF_LOW, VERDICT_UNKNOWN);
}

/* ----------------------------------------------------------------------- */
/* Hardware layer — excluded from host tests                                */
/* ----------------------------------------------------------------------- */

#ifndef CERBERUS_HOST_TEST

#include <conio.h>
#include <dos.h>
#include <i86.h>

/* PIT 8254 register addresses */
#define PIT_CH0_DATA      0x40
#define PIT_CH2_DATA      0x42
#define PIT_CTRL          0x43
#define SPEAKER_CTRL      0x61

/* Control bytes (command register format: SC1 SC0 RW1 RW0 M2 M1 M0 BCD) */
#define CTRL_C0_LATCH     0x00  /* 00 00 0000 : channel 0 counter latch */
#define CTRL_C2_LATCH     0x80  /* 10 00 0000 : channel 2 counter latch */
#define CTRL_C2_MODE2     0xB4  /* 10 11 010 0 : C2, LSB/MSB access, mode 2, binary */

/* Port 61h bit map:
 *   bit 0 = PIT Channel 2 gate enable
 *   bit 1 = speaker data enable (we always want this 0)
 */
#define SPK_GATE          0x01
#define SPK_DATA          0x02
#define SPK_MASK          0x03

static int           emulator_flag         = 0;
static unsigned int  measurement_start_ct  = 0xFFFF;

/* --- Raw PIT I/O primitives --- */

/* CLI around the latch+read sequence. Without this, an interrupt
 * handler (INT 8 on BIOS tick, any TSR, mouse driver, etc.) that
 * issues its own PIT command between our latch and our read would
 * clobber the latched value — resulting in composite LSB/MSB reads
 * from different time points. Observed symptom: phantom "up"
 * transitions in the polling loop inflating the wrap count by ~50%
 * on the 486DX2-66 bench box's chipset-integrated 8254. */
static unsigned int pit_read_c2(void)
{
    unsigned int lo, hi;
    _disable();
    outp(PIT_CTRL, CTRL_C2_LATCH);
    lo = (unsigned int)(inp(PIT_CH2_DATA) & 0xFF);
    hi = (unsigned int)(inp(PIT_CH2_DATA) & 0xFF);
    _enable();
    return (hi << 8) | lo;
}

static unsigned int pit_read_c0(void)
{
    unsigned int lo, hi;
    _disable();
    outp(PIT_CTRL, CTRL_C0_LATCH);
    lo = (unsigned int)(inp(PIT_CH0_DATA) & 0xFF);
    hi = (unsigned int)(inp(PIT_CH0_DATA) & 0xFF);
    _enable();
    return (hi << 8) | lo;
}

static void c2_gate_off(void)
{
    outp(SPEAKER_CTRL, inp(SPEAKER_CTRL) & (unsigned char)~SPK_MASK);
}

static void c2_gate_on(void)
{
    unsigned char v = (unsigned char)(inp(SPEAKER_CTRL) & ~SPK_MASK);
    outp(SPEAKER_CTRL, v | SPK_GATE);
}

static void c2_program_mode2_max(void)
{
    outp(PIT_CTRL, CTRL_C2_MODE2);
    outp(PIT_CH2_DATA, 0xFF);   /* LSB */
    outp(PIT_CH2_DATA, 0xFF);   /* MSB */
}

/* --- Public API --- */

void timing_init(void)
{
    /*
     * XT-clone sanity probe: on some XT clones Channel 2 is wired oddly
     * to the speaker such that the gate bit doesn't actually enable the
     * counter. Verify that the counter advances between two reads with a
     * small delay; if it doesn't, flag timing as emulator/broken so all
     * downstream confidence is capped.
     */
    unsigned int c1, c2;
    volatile unsigned int i;

    emulator_flag = 0;

    c2_gate_off();
    c2_program_mode2_max();
    c2_gate_on();

    c1 = pit_read_c2();
    for (i = 0; i < 500; i++) { /* small delay loop */ }
    c2 = pit_read_c2();

    /* If counter didn't move at all, PIT C2 gate isn't working on this box */
    if (c1 == c2) {
        emulator_flag = 1;
    }

    c2_gate_off();
}

void timing_start(void)
{
    c2_gate_off();
    c2_program_mode2_max();
    measurement_start_ct = 0xFFFF;
    c2_gate_on();
}

us_t timing_stop(void)
{
    unsigned int stop_ct = pit_read_c2();
    unsigned long ticks;

    c2_gate_off();
    ticks = timing_elapsed_ticks(measurement_start_ct, stop_ct);
    return timing_ticks_to_us(ticks);
}

void timing_wait_us(us_t microseconds)
{
    /*
     * Uses Channel 0 (the BIOS system tick — already running, DO NOT
     * reprogram). Safe to call while Channel 2 is in use for measurement.
     * Cap target at ~0xF000 ticks (~55ms) to keep the single-wrap math
     * valid; longer waits can be chained by the caller if needed.
     */
    unsigned long target_ticks;
    unsigned int start, now;
    unsigned long elapsed;

    if (microseconds == 0) return;

    /* target_ticks = microseconds * 1193 / 1000, rounded */
    target_ticks = (microseconds * 1193UL + 500UL) / 1000UL;
    if (target_ticks >= 0xF000UL) target_ticks = 0xF000UL;
    if (target_ticks == 0) target_ticks = 1;

    start = pit_read_c0();
    do {
        now = pit_read_c0();
        elapsed = timing_elapsed_ticks(start, now);
    } while (elapsed < target_ticks);
}

int timing_emulator_hint(void)
{
    return emulator_flag;
}

/* --- BIOS tick (PIT Channel 0) --------------------------------------- */

static unsigned long read_bios_tick(void)
{
    /*
     * BIOS tick is a 32-bit count at 0040:006Ch, incremented by INT 8
     * (the timer IRQ) roughly every 54.925 ms. The interrupt CAN fire
     * between our LSB and MSB reads, so the standard idiom is: read
     * MSB, LSB, MSB again — if MSBs match, the word is coherent; if
     * they differ, retry. A single retry is always sufficient because
     * INT 8 fires far slower than the time it takes to re-read.
     */
    volatile unsigned long __far *bt =
        (volatile unsigned long __far *)MK_FP(0x0040, 0x006C);
    unsigned long v1, v2;

    v1 = *bt;
    v2 = *bt;
    if ((v1 >> 16) != (v2 >> 16)) {
        v2 = *bt;  /* one retry — INT 8 can't fire twice this fast */
    }
    return v2;
}

int timing_dual_measure(unsigned int target_bios_ticks,
                        us_t *out_pit_us,
                        us_t *out_bios_us)
{
    /* Watcom C89: hoist all locals to the function top (no mid-block
     * declarations allowed). */
    unsigned long bt_start, bt_now;
    unsigned int  initial_c2, c2_prev, c2_now, c2_verify;
    unsigned long c2_wraps = 0;

    if (target_bios_ticks == 0) target_bios_ticks = 1;

    /* Program C2 to a known state and start it fresh from 0xFFFF. */
    c2_gate_off();
    c2_program_mode2_max();
    c2_gate_on();

    /* Sync to the NEXT BIOS tick edge, so we don't start partway
     * through an interval and lose precision on the bios_us side. */
    bt_start = read_bios_tick();
    do {
        bt_now = read_bios_tick();
    } while (bt_now == bt_start);
    bt_start = bt_now;

    /* Capture the ACTUAL starting C2 value AFTER sync-to-edge. The
     * counter has been running since c2_gate_on() — which may have
     * been up to ~55 ms ago because of the sync-to-edge wait — so we
     * cannot assume the start-state is 0xFFFF. initial_c2 is the true
     * reference and is never overwritten. c2_prev is the sliding
     * predecessor used only for wrap-edge detection. */
    initial_c2 = pit_read_c2();
    c2_prev    = initial_c2;

    /* Poll both counters. We MUST poll C2 fast enough to observe every
     * wrap (one wrap per ~55ms). Reading the BIOS tick and comparing
     * c2_prev->c2_now on every loop iteration takes well under 55ms
     * even on an 8088, so single-wrap-between-samples is a safe
     * assumption. The wrap test: C2 counts DOWN; if this reading is
     * GREATER than the previous, the counter reloaded (passed zero).
     *
     * Phantom-wrap defense: some chipsets (observed on the 486DX2-66
     * bench box's integrated 8254) produce composite LSB/MSB misreads
     * that are BIASED toward consistent mid-range high values — a
     * simple "verify re-read is <= first read" passes through them
     * because the verify sees the same biased value.
     *
     * Stronger defense: require the physical SHAPE of a real wrap.
     * A genuine wrap only happens as the counter crosses zero, so
     * c2_prev must have been near zero (below 0x4000) AND c2_now must
     * be near the reload value (above 0xC000). Any mid-range "wrap"
     * (e.g. c2_prev=0x7000, c2_now=0x8500) is physically impossible —
     * the counter does not leap upward mid-range. We add a verify
     * re-read as belt-and-suspenders for chipsets whose misreads ARE
     * random, not biased.
     *
     * Tuning: 0x4000 / 0xC000 gives a 16,384-tick (~14 ms) band on
     * each end of the counter range where a wrap crossing is
     * plausible. Any poll loop iterating faster than 14 ms between
     * consecutive reads (every CERBERUS-target CPU qualifies) can't
     * miss a real wrap. Slower poll loops can miss it — and the
     * sanity check in timing_compute_dual catches gross undercount.
     *
     * Limitation: if a wrap occurs between two consecutive samples
     * (requires an interrupt handler running >55ms, implausible on
     * clean DOS but possible on a pathological TSR stack), the wrap
     * is silently lost. */
    while (1) {
        c2_now = pit_read_c2();
        if (c2_now > c2_prev) {
            /* Apparent wrap — range-check the shape first. */
            if (c2_prev < 0x4000U && c2_now > 0xC000U) {
                /* Plausible wrap — belt-and-suspenders verify. */
                c2_verify = pit_read_c2();
                if (c2_verify <= c2_now) {
                    c2_wraps++;
                    if (c2_wraps > 8UL) {
                        c2_gate_off();
                        return 1;
                    }
                    c2_now = c2_verify;
                } else {
                    /* Verify inconsistent (c2_verify > c2_now is physically
                     * impossible — the counter cannot increase between reads
                     * unless it reloaded, but we're inside the apparent-wrap
                     * branch which already treats c2_now as post-reload).
                     * Both reads are suspect; adopting c2_verify as the new
                     * reference would plant a biased-high baseline that
                     * masks the next real wrap. Reject entirely and keep
                     * the last known-good sample. */
                    c2_now = c2_prev;
                }
            } else {
                /* Mid-range jump — physically impossible. Phantom.
                 * Keep c2_prev as-is so next iteration compares
                 * against the last good sample. */
                c2_now = c2_prev;
            }
        }
        c2_prev = c2_now;
        bt_now  = read_bios_tick();
        if (bt_now - bt_start >= (unsigned long)target_bios_ticks) break;
    }

    c2_gate_off();

    /* Defer all post-loop math to the pure kernel so host tests can
     * reach the sanity check and defensive-wrap branches without
     * having to simulate PIT hardware. */
    return timing_compute_dual(initial_c2, c2_now, c2_wraps,
                               bt_now - bt_start, target_bios_ticks,
                               out_pit_us, out_bios_us);
}

/* --- self-check entry point ------------------------------------------ */

void timing_self_check(result_table_t *t)
{
    /*
     * Target interval: 4 BIOS ticks. One tick would give ~3% resolution
     * on the bios_us side (one tick of noise out of four, on average,
     * because sync-to-edge already trims one). Four ticks ~ 220 ms —
     * long enough that both clocks report ~220000 us with better than
     * 1% discretization noise, short enough not to stall startup.
     *
     * Status emission gives the INI three distinct states so the user
     * can tell "self-check skipped entirely" from "attempted but
     * unreliable" from "attempted and succeeded":
     *   absent               -> skipped (e.g. /SKIP:TIMING)
     *   "measurement_failed" -> ran but wrap counter unreliable
     *   "ok"                 -> ran and produced usable data
     * The measurement_failed path intentionally does NOT emit the
     * pit_us/bios_us keys, so rule 4a still correctly no-ops rather
     * than flagging a spurious inconsistency on biased data.
     *
     * All emit logic lives in timing_emit_self_check (above the HW
     * guard) so host tests can exercise both paths directly without
     * having to simulate PIT hardware.
     */
    us_t pit_us = 0, bios_us = 0;
    int rc = timing_dual_measure(4, &pit_us, &bios_us);
    timing_emit_self_check(t, rc, pit_us, bios_us);
}

#endif  /* CERBERUS_HOST_TEST */
