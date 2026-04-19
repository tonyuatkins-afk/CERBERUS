/*
 * Consistency engine — Phase 4 Task 4.1.
 *
 * Seed rules for v0.5. More rules land as Phase 3 calibrated mode
 * populates per-pass bench data (rule 4a/4b MIPS vs class_ipc), as
 * Task 1.4 cache stride work lands (rule 8), and as Phase 2 deferred
 * diagnostics add coverage.
 *
 * Each rule is structured as a small function that:
 *   1. Reads the specific keys it depends on via find_key()
 *   2. Returns early with no emit if a prerequisite key is absent
 *      (the rule doesn't apply to this run)
 *   3. Emits exactly one consistency.<name> row with PASS / WARN /
 *      FAIL and a human-readable explanation
 *
 * Rules NEVER emit multiple rows, NEVER modify existing entries, and
 * NEVER crash the orchestrator if a detect module skipped something.
 * Absence of a required key is always "rule not applicable" — never
 * a fault in itself.
 */

#include <stdio.h>
#include <string.h>
#include "consist.h"
#include "report.h"

/* Static message buffers — report_add_str stores the pointer we pass it
 * verbatim (see report.c:55 and the lifetime note in report.h). The
 * result table is read both by report_write_ini and by ui_render_* long
 * after each rule function returns, so stack-local sprintf scratch
 * buffers dangle. Every rule that formats a dynamic message lives on
 * one of these file-scope statics instead.
 *
 * Single-call contract: consist_check runs each rule exactly ONCE per
 * cerberus run, and each listed rule emits at most one row using its
 * dedicated buffer. Naming is per-rule + per-branch so two branches in
 * the same rule never share storage. Future rules that format dynamic
 * strings MUST declare their own dedicated statics here rather than
 * reuse one of these — sharing would silently clobber the earlier
 * rule's stored pointer in the result table. String-literal message
 * arms are static-lifetime for free and do not need a buffer. */
static char msg_rule5_warn[96];
static char msg_rule6_fail[96];
static char msg_rule9_fail[96];
static char msg_rule4a_warn[120];
static char msg_rule4b_warn[160];
static char msg_rule4b_pass[120];
static char msg_rule7_warn[120];
static char msg_rule7_warn_unknown[140];
static char msg_rule7_fail[120];
static char msg_rule10_fail[120];

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

static const char *key_value(const result_t *r)
{
    if (!r) return (const char *)0;
    if (r->display) return r->display;
    if (r->type == V_STR && r->v.s) return r->v.s;
    return (const char *)0;
}

static int value_contains(const result_t *r, const char *substring)
{
    const char *v = key_value(r);
    return v && strstr(v, substring) != (char *)0;
}

/* ----------------------------------------------------------------------- */
/* Rule 1: CPU reported as 486DX (or similar DX variant) → FPU must be
 *         integrated.
 *
 * Detects: counterfeit "486DX" that is really an SX with no FPU, or a
 *          486SX with the ID silkscreen mislabeled.
 * Does NOT detect: an actual 486DX with a broken FPU (that's the
 *          diag_fpu domain).                                              */
/* ----------------------------------------------------------------------- */

static void rule_486dx_implies_integrated_fpu(result_table_t *t)
{
    const result_t *cpu = find_key(t, "cpu.detected");
    const result_t *fpu = find_key(t, "fpu.detected");
    const char *cv = key_value(cpu);
    const char *fv = key_value(fpu);

    if (!cv || !fv) return;  /* rule not applicable */

    /* Applies to any 486 variant ending in "DX" (DX/DX2/DX4) but NOT
     * 486SX. The cpu_db friendly names follow the pattern "i486DX..." */
    if (strstr(cv, "486DX") == (char *)0) return;
    if (strstr(cv, "486SX") != (char *)0) return;  /* belt + suspenders */

    if (strstr(fv, "integrated") != (char *)0) {
        report_add_str(t, "consistency.486dx_fpu",
                       "pass (486DX reports integrated FPU)",
                       CONF_HIGH, VERDICT_PASS);
    } else {
        report_add_str(t, "consistency.486dx_fpu",
                       "FAIL: CPU reports 486DX-class but FPU is not integrated",
                       CONF_HIGH, VERDICT_FAIL);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 2: CPU reported as 486SX → FPU must NOT be integrated.
 *
 * Detects: a 486SX report that paradoxically shows integrated FPU
 *          (possible detection confusion or masked 487 scenario).
 * Does NOT detect: a 486SX + external 487 coprocessor combination
 *          (that's a valid state — "non-integrated FPU present" is
 *          the expected behavior and rule 2 passes).                      */
/* ----------------------------------------------------------------------- */

static void rule_486sx_no_integrated_fpu(result_table_t *t)
{
    const result_t *cpu = find_key(t, "cpu.detected");
    const result_t *fpu = find_key(t, "fpu.detected");
    const char *cv = key_value(cpu);
    const char *fv = key_value(fpu);

    if (!cv || !fv) return;
    if (strstr(cv, "486SX") == (char *)0) return;

    if (strstr(fv, "integrated") == (char *)0) {
        report_add_str(t, "consistency.486sx_fpu",
                       "pass (486SX reports non-integrated FPU as expected)",
                       CONF_HIGH, VERDICT_PASS);
    } else {
        report_add_str(t, "consistency.486sx_fpu",
                       "FAIL: CPU is 486SX but FPU reports integrated",
                       CONF_HIGH, VERDICT_FAIL);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 5: FPU diag PASS ↔ FPU bench produced a numeric result.
 *
 * Detects: internal inconsistency where one head claims the FPU works
 *          and another couldn't exercise it, or vice versa.
 * Does NOT detect: both heads failing the same way (correlated fault).   */
/* ----------------------------------------------------------------------- */

static void rule_fpu_diag_bench_agreement(result_table_t *t)
{
    const result_t *diag = find_key(t, "diagnose.fpu.compound");
    const result_t *bench = find_key(t, "bench.fpu.ops_per_sec");
    int diag_pass  = diag  && diag->verdict == VERDICT_PASS;
    int bench_ok   = bench && bench->v.u > 0UL;

    /* Rule only applies if BOTH paths ran. If either head was skipped
     * (/ONLY:*, /SKIP:DIAG, /SKIP:BENCH) the rule no-ops because we
     * can't compare. This is intentional — absence is not a fault. */
    if (!diag || !bench) return;

    if (diag_pass == bench_ok) {
        report_add_str(t, "consistency.fpu_diag_bench",
                       "pass (diag and bench agree on FPU liveness)",
                       CONF_HIGH, VERDICT_PASS);
    } else {
        sprintf(msg_rule5_warn, "WARN: diag.fpu:%s but bench.fpu:%s",
                diag_pass ? "pass" : "no-pass",
                bench_ok  ? "has-result" : "no-result");
        report_add_str(t, "consistency.fpu_diag_bench", msg_rule5_warn,
                       CONF_HIGH, VERDICT_WARN);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 6: memory.extended_kb > 0 → CPU class must be ≥ 286.
 *
 * Detects: a detect run that reports extended memory on an 8086-class
 *          CPU (physically impossible; would indicate a detection bug).
 * Does NOT detect: a correctly-detected 286+ with improperly-reported
 *          extended memory size.                                          */
/* ----------------------------------------------------------------------- */

static void rule_extmem_implies_286(result_table_t *t)
{
    const result_t *ext   = find_key(t, "memory.extended_kb");
    const result_t *class = find_key(t, "cpu.class");
    const char *cv = key_value(class);
    unsigned long ext_kb;

    if (!ext || !class) return;

    ext_kb = ext->v.u;
    if (ext_kb == 0) return;  /* rule not applicable */

    /* The CPU class tokens from cpu_db: 8086/8088/v20/v30 are the
     * legacy pre-286 class; anything else is 286 or later. */
    if (!cv) return;
    if (strcmp(cv, "8086") == 0 || strcmp(cv, "8088") == 0 ||
        strcmp(cv, "v20")  == 0 || strcmp(cv, "v30")  == 0) {
        sprintf(msg_rule6_fail, "FAIL: extended memory %luKB reported on %s",
                ext_kb, cv);
        report_add_str(t, "consistency.extmem_cpu", msg_rule6_fail,
                       CONF_HIGH, VERDICT_FAIL);
    } else {
        report_add_str(t, "consistency.extmem_cpu",
                       "pass (extended memory consistent with CPU class)",
                       CONF_HIGH, VERDICT_PASS);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 3: cpu.detected contains "386SX" → bus.class must not be "isa8".
 *
 * 386SX has a 16-bit external data bus despite being a 32-bit CPU
 * internally. Every system shipped with a 386SX used ISA 16-bit or
 * better; a 386SX on an ISA-8 (XT-class) bus is electrically
 * impossible.
 *
 * Detects: detection disagreement where the bus probe misclassified
 *          a 386SX system as XT-class ISA-8.
 * Does NOT detect: any 386SX variant (SL/CX/EX) specifics — the
 *          current cpu_db lacks per-variant SX/DX entries on the
 *          legacy CPUID-absent path. This rule activates only when
 *          the DB gains "386SX" friendly names, which is a follow-up.
 *          Until then it is correctly no-op.                             */
/* ----------------------------------------------------------------------- */

static void rule_386sx_implies_isa16(result_table_t *t)
{
    const result_t *cpu = find_key(t, "cpu.detected");
    const result_t *bus = find_key(t, "bus.class");
    const char *cv = key_value(cpu);
    const char *bv = key_value(bus);

    if (!cv || !bv) return;
    if (strstr(cv, "386SX") == (char *)0) return;  /* dormant today */

    if (strcmp(bv, "isa8") == 0) {
        report_add_str(t, "consistency.386sx_bus",
                       "FAIL: 386SX CPU reported but bus is ISA-8 (impossible)",
                       CONF_HIGH, VERDICT_FAIL);
    } else {
        report_add_str(t, "consistency.386sx_bus",
                       "pass (386SX bus is ISA-16 or better)",
                       CONF_HIGH, VERDICT_PASS);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 9: 8086-class CPU must be on an ISA-8 bus.
 *
 * 8086/8088/V20/V30 systems all shipped with ISA 8-bit. A VLB or PCI
 * bus on an 8086-class machine is physically impossible. Catches
 * detection bugs where a misidentified chipset's probe confuses bus
 * detection for floor-target machines.
 *
 * Fires today on current detect output — cpu_db legacy tokens
 * "8086/8088/v20/v30" are populated whenever the EFLAGS probe
 * determines the CPU is pre-286.                                          */
/* ----------------------------------------------------------------------- */

static void rule_8086_implies_isa8(result_table_t *t)
{
    const result_t *cls = find_key(t, "cpu.class");
    const result_t *bus = find_key(t, "bus.class");
    const char *cv = key_value(cls);
    const char *bv = key_value(bus);

    if (!cv || !bv) return;
    if (!(strcmp(cv, "8086") == 0 || strcmp(cv, "8088") == 0 ||
          strcmp(cv, "v20")  == 0 || strcmp(cv, "v30")  == 0)) return;

    if (strcmp(bv, "isa8") == 0) {
        report_add_str(t, "consistency.8086_bus",
                       "pass (8086-class CPU with ISA-8 bus, as expected)",
                       CONF_HIGH, VERDICT_PASS);
    } else if (strcmp(bv, "unknown") == 0) {
        /* Unknown bus on an 8086-class CPU is suspicious but not
         * definitely wrong — might just be a probe miss. WARN, not
         * FAIL. */
        report_add_str(t, "consistency.8086_bus",
                       "WARN: 8086-class CPU but bus class unknown",
                       CONF_HIGH, VERDICT_WARN);
    } else {
        sprintf(msg_rule9_fail, "FAIL: 8086-class CPU (%s) cannot be on a %s bus", cv, bv);
        report_add_str(t, "consistency.8086_bus", msg_rule9_fail,
                       CONF_HIGH, VERDICT_FAIL);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 4a: PIT Channel 2 (timing.c) vs Channel 0 (BIOS tick) agreement.
 *
 * Both PIT channels share the 1.193182 MHz crystal. timing_self_check
 * measured the same real-time interval with both and wrote the two
 * derived us totals as timing.cross_check.{pit_us,bios_us}. If they
 * disagree by more than 15%, flag timing_inconsistency — something in
 * timing.c's math (or the BIOS tick path) is biased.
 *
 * This rule is independent of every CPU/memory/bus detection result —
 * it validates the measurement infrastructure itself, the one thing all
 * other rules depend on.
 *
 * Detects: the 16-bit integer trap class in timing.c (ticks * 838
 *          overflows without unsigned long), PIT C2 wrap counting bugs,
 *          a BIOS tick that's been hooked by a TSR that drops ticks.
 * Does NOT detect: faults that bias BOTH paths identically (e.g. a
 *          crystal running off-spec — both channels report the same
 *          wrong answer).
 * Verdict: WARN, not FAIL. We can see the two paths disagree but can't
 *          tell which is the biased one; downstream timing-dependent
 *          measurements should be viewed skeptically, not discarded.  */
/* ----------------------------------------------------------------------- */

static void rule_timing_independence(result_table_t *t)
{
    const result_t *pit  = find_key(t, "timing.cross_check.pit_us");
    const result_t *bios = find_key(t, "timing.cross_check.bios_us");
    unsigned long pit_us, bios_us;
    unsigned long delta, threshold;

    if (!pit || !bios) return;  /* self-check didn't run / bailed */

    pit_us  = pit->v.u;
    bios_us = bios->v.u;
    if (pit_us == 0 || bios_us == 0) return;

    delta     = (pit_us > bios_us) ? (pit_us - bios_us) : (bios_us - pit_us);
    threshold = bios_us * 15UL / 100UL;

    if (delta <= threshold) {
        report_add_str(t, "consistency.timing_independence",
                       "pass (PIT C2 and BIOS tick agree within 15%)",
                       CONF_HIGH, VERDICT_PASS);
    } else {
        unsigned long pct = delta * 100UL / bios_us;
        sprintf(msg_rule4a_warn, "WARN: PIT %luus BIOS %luus diverge %lu%% (timing.c suspect)",
                pit_us, bios_us, pct);
        report_add_str(t, "consistency.timing_independence", msg_rule4a_warn,
                       CONF_HIGH, VERDICT_WARN);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 4b: bench_cpu iters_per_sec must lie within the CPU DB's empirical
 *          range for the detected family/model.
 *
 * Detects:
 *   - Thermal throttle / broken cooling (chip downclocking itself).
 *   - Cache disabled in BIOS (huge bench slowdown even at full clock).
 *   - TSR / ISR interrupt storms stealing cycles — observed on the 486
 *     DX-2 bench box where CTCM + mixer init cut the bench from ~8.4M
 *     iters/sec down to ~2.2M. The WARN is the signal that SOMETHING
 *     took a big bite out of compute, without claiming which.
 *   - Under-identified overclocks / counterfeit repinned parts whose
 *     bench sits ABOVE the expected range.
 *   - Clock-doubler chips that failed to enter double-clocked mode.
 *
 * Does NOT detect:
 *   - Biased-equally-across-both-paths issues (PIT miscalibration that
 *     scales bench_cpu AND the wall clock the same way — the bench
 *     reports its own normalized iters/sec so this cancels).
 *   - Workloads that genuinely ARE in the range but pathological for a
 *     specific app (the bench loop is a CPU-only integer micro, not
 *     representative of mixed-FPU/MMU code).
 *
 * Input keys:
 *   bench.cpu.int_iters_per_sec  — measured by bench_cpu (Phase 3)
 *   cpu.bench_iters_low          — emitted by detect_cpu when DB has data
 *   cpu.bench_iters_high         —       "         "        "        "
 *
 * Rule no-ops if any of the three keys is missing. This is intentional:
 * most DB rows currently seed 0/0 until real-hardware data arrives, so
 * most CPUs won't trigger the rule yet — a clean signal path rather than
 * a wall of MEDIUM-confidence WARNs on unknowns.
 *
 * Verdict: PASS when in range, WARN when outside. Under-range narration
 *          is three-way split with v0.4:
 *            - diagnose.cache.status contains "cache_working" → cache is
 *              provably live, so the deficit is TSR/thermal throttle.
 *            - diagnose.cache.status contains "no_cache_effect" → cache
 *              is provably dead, so the deficit is cache-BIOS-disabled
 *              (or cache silicon failure).
 *            - cache status missing or inconclusive (partial, anomaly,
 *              no_measurement) → fall back to the original ambiguous
 *              three-cause narration.
 *          Above-range is rarer and usually indicates overclock or
 *          counterfeit; same WARN, different message.                  */
/* ----------------------------------------------------------------------- */

static void rule_cpu_ipc_bench(result_table_t *t)
{
    const result_t *r_iters = find_key(t, "bench.cpu.int_iters_per_sec");
    const result_t *r_low   = find_key(t, "cpu.bench_iters_low");
    const result_t *r_high  = find_key(t, "cpu.bench_iters_high");
    const result_t *r_cache;
    unsigned long iters, lo, hi;

    if (!r_iters || !r_low || !r_high) return;  /* missing data — no-op */

    iters = r_iters->v.u;
    lo    = r_low->v.u;
    hi    = r_high->v.u;
    if (iters == 0 || lo == 0 || hi == 0 || lo > hi) return;

    if (iters >= lo && iters <= hi) {
        sprintf(msg_rule4b_pass,
                "pass (bench %lu/sec within expected %lu-%lu for this CPU)",
                iters, lo, hi);
        report_add_str(t, "consistency.cpu_ipc_bench", msg_rule4b_pass,
                       CONF_HIGH, VERDICT_PASS);
    } else if (iters < lo) {
        r_cache = find_key(t, "diagnose.cache.status");
        if (r_cache && r_cache->v.s && strstr(r_cache->v.s, "cache_working")) {
            sprintf(msg_rule4b_warn,
                    "WARN: bench %lu/sec below expected %lu (cache diag PASS — TSR stealing cycles or thermal throttle)",
                    iters, lo);
        } else if (r_cache && r_cache->v.s && strstr(r_cache->v.s, "no_cache_effect")) {
            sprintf(msg_rule4b_warn,
                    "WARN: bench %lu/sec below expected %lu (cache diag FAIL — cache disabled in BIOS or absent)",
                    iters, lo);
        } else {
            sprintf(msg_rule4b_warn,
                    "WARN: bench %lu/sec below expected %lu (throttle, cache disabled, or TSR stealing cycles)",
                    iters, lo);
        }
        report_add_str(t, "consistency.cpu_ipc_bench", msg_rule4b_warn,
                       CONF_HIGH, VERDICT_WARN);
    } else {
        sprintf(msg_rule4b_warn,
                "WARN: bench %lu/sec above expected %lu (overclock or CPU misidentified)",
                iters, hi);
        report_add_str(t, "consistency.cpu_ipc_bench", msg_rule4b_warn,
                       CONF_HIGH, VERDICT_WARN);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 7: audio DB mixer_chip column agrees with the hardware mixer probe.
 *
 * Detects:
 *   - A Sound Blaster family card that the DB identifies as CT1745-backed
 *     (SB16 CT2230+, Vibra 16S — today's seeded set) but whose mixer port
 *     doesn't answer like a CT1745. Classic fingerprint of a counterfeit
 *     or repackaged OEM card advertising an SB16 DSP version while
 *     actually carrying a cheaper mixer.
 *   - An audio entry whose DB row says "unknown" but where the probe sees
 *     a plausible CT1745 byte. Read as: "you have real data, please
 *     contribute it back to hw_db/audio.csv so the DB matures."
 *
 * Does NOT detect:
 *   - Mixer chips other than CT1745 (e.g., CT1345 on SB Pro). The probe
 *     returns "unknown" or "none" in that range today. Widening coverage
 *     is a follow-up — add discriminators in probe_mixer_chip and the
 *     CT1345 column value simultaneously.
 *   - Cards that faithfully clone a CT1745 to the point of returning
 *     plausible Interrupt Setup bytes. The rule is an additional
 *     consistency axis, not a counterfeit-proof credential check.
 *
 * Brief from session 2026-04-18 offered "Rule 8 or extend Rule 5". Rule
 * 5 (fpu_diag_bench) is a different subsystem and doesn't fit; Rule 8 is
 * reserved above for the future cache-stride cross-check. Rule 7 is the
 * free slot and used here.
 *
 * Verdict: PASS on match, WARN when DB is unknown-with-mixer-observed,
 *          FAIL when DB expects a specific chip and probe contradicts.   */
/* ----------------------------------------------------------------------- */

static void rule_audio_mixer_chip(result_table_t *t)
{
    const result_t *exp = find_key(t, "audio.mixer_chip_expected");
    const result_t *obs = find_key(t, "audio.mixer_chip_observed");
    const char *ev = key_value(exp);
    const char *ov = key_value(obs);

    if (!exp || !obs || !ev || !ov) return;  /* rule not applicable */

    /* DB has no mixer data, probe saw nothing either — no usable signal;
     * stay silent rather than emit a dozen WARNs on every non-SB card. */
    if (strcmp(ev, "unknown") == 0 && strcmp(ov, "none") == 0) return;

    /* DB is uncertain but probe saw a mixer — prompt human to seed DB. */
    if (strcmp(ev, "unknown") == 0) {
        sprintf(msg_rule7_warn,
                "WARN: audio DB mixer_chip=unknown for this card; probe observed '%s' (contribute to hw_db/audio.csv)",
                ov);
        report_add_str(t, "consistency.audio_mixer_chip", msg_rule7_warn,
                       CONF_MEDIUM, VERDICT_WARN);
        return;
    }

    /* DB expects a specific chip. Compare. */
    if (strcmp(ev, ov) == 0) {
        report_add_str(t, "consistency.audio_mixer_chip",
                       "pass (mixer chip matches DB expectation)",
                       CONF_HIGH, VERDICT_PASS);
    } else if (strcmp(ov, "unknown") == 0) {
        /* Probe-observed "unknown" means the Interrupt Setup byte read
         * neither a CT1745-shaped value nor a clearly-absent sentinel —
         * probe_mixer_chip explicitly reserves this bucket for "weird
         * mixer, needs human triage via Rule 7 WARN" (see audio.c). A
         * plain FAIL here would cry wolf on a legitimate CT1745 whose
         * register snapshot happened to fall in the inconclusive range,
         * contradicting the classifier's documented contract. Emit WARN
         * at MEDIUM confidence to mirror the unknown-DB WARN branch. */
        sprintf(msg_rule7_warn_unknown,
                "WARN: audio DB expects mixer '%s' but probe observed 'unknown' (mixer-probe inconclusive, human triage needed)",
                ev);
        report_add_str(t, "consistency.audio_mixer_chip",
                       msg_rule7_warn_unknown,
                       CONF_MEDIUM, VERDICT_WARN);
    } else {
        sprintf(msg_rule7_fail,
                "FAIL: audio DB expects mixer '%s' but probe observed '%s'",
                ev, ov);
        report_add_str(t, "consistency.audio_mixer_chip", msg_rule7_fail,
                       CONF_HIGH, VERDICT_FAIL);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 10: Whetstone completion state agrees with detect_fpu's report.
 *
 * Detects:
 *   - detect_fpu says "none" but Whetstone ran to completion and
 *     produced a number. x87 instructions executed successfully, so an
 *     FPU (or emulator TSR) is present and detect under-reported. Classic
 *     case: socketed 8087 the FNINIT/FNSTSW probe somehow missed.
 *   - detect names an FPU but Whetstone reported skipped_no_fpu.
 *     bench_whetstone consumes the same fpu.detected row, so the two
 *     sides should only disagree on a memory-corruption-class bug —
 *     still worth flagging if it ever happens.
 *
 * Does NOT detect:
 *   - Broken FPU that completes Whetstone with wrong numbers. Rule 10
 *     treats "completed" as PASS. Rule 5 (fpu_diag_bench) and diag_fpu's
 *     known-answer bit-exact tests cover that axis.
 *   - Software FPU emulator — classified as "present" (correctly, at the
 *     ISA level). Consumers wanting hardware-vs-emulator distinction
 *     consult fpu.vendor / fpu.friendly.
 *
 * Slot rationale: session brief wrote "Rule 9" but Rule 9 is already
 * rule_8086_implies_isa8, and Rule 8 is reserved in this footer for
 * the future cache-stride cross-check. Rule 10 is the next free slot.
 * Same off-by-one pattern as Rule 7. Noted here and in commit body.
 *
 * Input keys:
 *   fpu.detected                  — detect_fpu ("none" or a tag)
 *   bench.fpu.whetstone_status    — bench_whetstone ("ok" or skipped_*)
 *   bench.fpu.k_whetstones        — bench_whetstone (present when status=ok)
 */
/* ----------------------------------------------------------------------- */

static void rule_whetstone_fpu_consistency(result_table_t *t)
{
    const result_t *fpu    = find_key(t, "fpu.detected");
    const result_t *status = find_key(t, "bench.fpu.whetstone_status");
    const result_t *kwhet  = find_key(t, "bench.fpu.k_whetstones");

    const char *fpu_tag = key_value(fpu);
    const char *st      = key_value(status);
    int fpu_present;
    int whet_ran;

    /* Rule applies only when both heads produced a verdict. /ONLY:DET
     * or /ONLY:BENCH leaves one key absent; absence is not a fault. */
    if (!fpu_tag || !st) return;

    /* Whetstone reports "inconclusive_elapsed_zero" when the measurement
     * loop finished in under one PIT tick — emulator-artifact territory,
     * not a disagreement between detection heads. The status string is
     * defined in src/bench/bench_whetstone.c on the else branch of the
     * elapsed-us guard. Early-return here so Rule 10 is a no-op on that
     * axis; the WARN verdict already attached to the status row surfaces
     * the measurement issue without implicating detection consistency.
     * Strncmp on "inconclusive" covers any future inconclusive_* suffixes
     * bench_whetstone might add without needing a new gate here. */
    if (strncmp(st, "inconclusive", 12) == 0) return;

    fpu_present = (strcmp(fpu_tag, "none") != 0);
    whet_ran    = (strcmp(st, "ok") == 0);

    if (fpu_present && whet_ran) {
        if (kwhet && kwhet->type == V_U32 && kwhet->v.u == 0UL) {
            report_add_str(t, "consistency.whetstone_fpu",
                           "WARN: FPU reported present and Whetstone ran, but k_whetstones=0",
                           CONF_MEDIUM, VERDICT_WARN);
        } else {
            report_add_str(t, "consistency.whetstone_fpu",
                           "pass (FPU present and Whetstone produced a result)",
                           CONF_HIGH, VERDICT_PASS);
        }
    } else if (!fpu_present && !whet_ran) {
        /* Tightened in round-2 fix S4: only emit PASS for the exact
         * expected skip status token. If a future bench_whetstone adds a
         * new skip reason we don't recognize, no-op on this branch (rule
         * not applicable — unknown bench state) rather than claiming
         * correctness. The "inconclusive*" prefix has already been
         * filtered by the early-return above, so the only PASS-worthy
         * token here is the exact "skipped_no_fpu" bench emits on the
         * FPU-absent path. */
        if (strcmp(st, "skipped_no_fpu") == 0) {
            report_add_str(t, "consistency.whetstone_fpu",
                           "pass (no FPU, Whetstone correctly skipped)",
                           CONF_HIGH, VERDICT_PASS);
        }
        /* else: unknown skip token — fall through to no-op */
    } else {
        sprintf(msg_rule10_fail,
                "FAIL: detect says fpu=%s but Whetstone status=%s",
                fpu_tag, st);
        report_add_str(t, "consistency.whetstone_fpu", msg_rule10_fail,
                       CONF_HIGH, VERDICT_FAIL);
    }
}

/* ----------------------------------------------------------------------- */
/* Rule 11: dma_class_coherence.
 *
 * XT-class CPUs (8086 / 8088 / NEC V20 / V30) were shipped in systems
 * with only the master 8237 DMA controller at 0x00-0x1F. The slave
 * controller (channels 5-7 at 0xC0-0xDF) appeared with the PC/AT (286).
 * If detect_cpu reports XT-class AND diag_dma finds channel 5
 * responsive, the two detection paths contradict.
 *
 * Detects:
 *   - Misidentified CPU (real 286 being reported as 8088 by a broken
 *     CPUID-fallback path).
 *   - Misidentified bus/board (XT with a retrofit ISA-16 slot + slave
 *     DMA — rare but has been seen on collector frankenboards).
 *   - A diag_dma readback false-positive on ch5 (the pattern byte
 *     happened to match on a floating bus).
 *
 * Does NOT detect:
 *   - An AT-class CPU with a broken slave (diag_dma would report the
 *     slave channels as FAIL; that's caught by the dma.summary verdict,
 *     not here).
 *   - Legitimately XT hardware where diag_dma safety-skipped the
 *     slave (the skipped_no_slave status matches the XT claim — PASS).
 *
 * Input keys:
 *   cpu.class                  — "8086"/"8088"/"v20"/"v30" for XT-class
 *   diagnose.dma.ch5_status    — emitted by diag_dma in three forms:
 *                                 "pass" (controller responded)
 *                                 "fail" (write-readback mismatch)
 *                                 "skipped_no_slave (XT-class machine)"
 *
 * Rule no-ops if either key is absent (missing data, not a
 * contradiction). Rule also no-ops for AT-class CPUs.                 */
/* ----------------------------------------------------------------------- */

static void rule_dma_class_coherence(result_table_t *t)
{
    const result_t *r_cls = find_key(t, "cpu.class");
    const result_t *r_ch5 = find_key(t, "diagnose.dma.ch5_status");
    const char *cv, *c5;
    int is_xt;

    if (!r_cls || !r_ch5) return;
    cv = r_cls->v.s;
    c5 = r_ch5->v.s;
    if (!cv || !c5) return;

    is_xt = (strcmp(cv, "8088") == 0 || strcmp(cv, "8086") == 0 ||
             strcmp(cv, "v20")  == 0 || strcmp(cv, "v30")  == 0);
    if (!is_xt) return;  /* rule only applies to XT-class CPUs */

    /* Exact token match against diag_dma's emitted strings. The
     * skipped_no_slave form carries a parenthetical tail; match on
     * substring so a future message tweak doesn't silently break the
     * coherence path. */
    if (strstr(c5, "skipped_no_slave")) {
        report_add_str(t, "consistency.dma_class_coherence",
                       "pass (XT-class CPU with no DMA slave, as expected)",
                       CONF_HIGH, VERDICT_PASS);
    } else if (strcmp(c5, "pass") == 0) {
        report_add_str(t, "consistency.dma_class_coherence",
                       "WARN: XT-class CPU but DMA slave channel responsive — detection contradiction",
                       CONF_HIGH, VERDICT_WARN);
    }
    /* else: fail / unknown token — silent no-op, surface via
     * the per-channel status row rather than a second WARN. */
}

/* ----------------------------------------------------------------------- */

void consist_check(result_table_t *t)
{
    rule_486dx_implies_integrated_fpu(t);
    rule_486sx_no_integrated_fpu(t);
    rule_386sx_implies_isa16(t);
    rule_fpu_diag_bench_agreement(t);
    rule_extmem_implies_286(t);
    rule_8086_implies_isa8(t);
    rule_timing_independence(t);
    rule_cpu_ipc_bench(t);
    rule_audio_mixer_chip(t);
    rule_whetstone_fpu_consistency(t);
    rule_dma_class_coherence(t);
    /*
     * Rules landing as downstream phases complete:
     *
     *   rule_cache_stride_vs_cpuid_leaf2  — needs cache bench (Task 3.3)
     *                                        and CPUID leaf 2 decode
     *   rule_vga_bench_modes_available    — needs bench_video (Task 3.5)
     *
     * Each new rule should document the failure modes it catches AND
     * the ones it structurally cannot. Opacity is the anti-pattern.
     */
    (void)value_contains;  /* reserved for future string-matching rules */
}
