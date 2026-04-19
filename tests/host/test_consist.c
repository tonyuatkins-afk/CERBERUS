/*
 * Host-side test for the consistency engine. Synthesize result tables
 * representing realistic + pathological machine configurations and
 * verify each rule flags the right outcomes.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/core/consist.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

static const result_t *k(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

static verdict_t v_of(const result_t *r) { return r ? r->verdict : VERDICT_UNKNOWN; }

int main(void)
{
    result_table_t t;
    printf("=== CERBERUS host unit test: consistency engine ===\n");

    /* Scenario A: normal 486DX with integrated FPU → rule 1 PASS */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.detected", "Intel i486DX2-66", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "fpu.detected", "integrated-486",   CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.486dx_fpu")) == VERDICT_PASS,
          "Scenario A: 486DX + integrated → rule 1 PASS");

    /* Scenario B: 486DX but FPU reports external (counterfeit!) → rule 1 FAIL */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.detected", "Intel i486DX-33", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "fpu.detected", "387",             CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.486dx_fpu")) == VERDICT_FAIL,
          "Scenario B: 486DX + non-integrated FPU → rule 1 FAIL");

    /* Scenario C: 486SX without integrated FPU → rule 2 PASS */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.detected", "Intel i486SX",    CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "fpu.detected", "none",            CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.486sx_fpu")) == VERDICT_PASS,
          "Scenario C: 486SX + no FPU → rule 2 PASS");

    /* Scenario D: 486SX with integrated FPU (paradox!) → rule 2 FAIL */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.detected", "Intel i486SX",    CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "fpu.detected", "integrated-486",  CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.486sx_fpu")) == VERDICT_FAIL,
          "Scenario D: 486SX + integrated FPU → rule 2 FAIL");

    /* Scenario E: 8088 with NO extended memory → rule 6 not applicable */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class",           "8088",  CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "memory.extended_kb",  0UL, "0", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.extmem_cpu") == NULL,
          "Scenario E: 8088 + 0 extended → rule 6 no-op");

    /* Scenario F: 8088 WITH extended memory (impossible!) → rule 6 FAIL */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class",           "8088",   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "memory.extended_kb",  1024UL, "1024", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.extmem_cpu")) == VERDICT_FAIL,
          "Scenario F: 8088 + 1024KB extended → rule 6 FAIL");

    /* Scenario G: 286 with 512KB extended → rule 6 PASS */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class",           "286",   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "memory.extended_kb",  512UL, "512", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.extmem_cpu")) == VERDICT_PASS,
          "Scenario G: 286 + 512KB extended → rule 6 PASS");

    /* Scenario H: diag fpu pass + bench fpu has result → rule 5 PASS */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "diagnose.fpu.compound",  "pass",  CONF_HIGH, VERDICT_PASS);
    report_add_u32(&t, "bench.fpu.ops_per_sec",  50000UL, "50000", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.fpu_diag_bench")) == VERDICT_PASS,
          "Scenario H: FPU diag pass + bench has result → rule 5 PASS");

    /* Scenario I: diag pass but bench result is 0 → rule 5 WARN */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "diagnose.fpu.compound",  "pass",  CONF_HIGH, VERDICT_PASS);
    report_add_u32(&t, "bench.fpu.ops_per_sec",  0UL, "0", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.fpu_diag_bench")) == VERDICT_WARN,
          "Scenario I: diag pass but bench=0 → rule 5 WARN");

    /* Scenario I2: diag present, bench absent (/ONLY:DIAG or /SKIP:BENCH)
     * → rule 5 no-op. Absence of either head is not a fault; the rule
     * can't cross-check with only one side. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "diagnose.fpu.compound",  "pass",  CONF_HIGH, VERDICT_PASS);
    consist_check(&t);
    CHECK(k(&t, "consistency.fpu_diag_bench") == NULL,
          "Scenario I2: diag present, bench absent → rule 5 no-op");

    /* Scenario I3: bench present, diag absent (/ONLY:BENCH or /SKIP:DIAG)
     * → rule 5 no-op. Symmetric with I2. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "bench.fpu.ops_per_sec",  50000UL, "50000", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.fpu_diag_bench") == NULL,
          "Scenario I3: bench present, diag absent → rule 5 no-op");

    /* Scenario J: 386SX on ISA-8 → rule 3 FAIL */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.detected", "Intel i386SX-16", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bus.class",    "isa8",            CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.386sx_bus")) == VERDICT_FAIL,
          "Scenario J: 386SX + isa8 → rule 3 FAIL");

    /* Scenario K: 386SX on ISA-16 → rule 3 PASS */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.detected", "Intel i386SX", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bus.class",    "isa16",        CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.386sx_bus")) == VERDICT_PASS,
          "Scenario K: 386SX + isa16 → rule 3 PASS");

    /* Scenario L: 386DX on ISA-8 → rule 3 not applicable (no 386SX match) */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.detected", "Intel i386DX", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bus.class",    "isa8",        CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.386sx_bus") == NULL,
          "Scenario L: 386DX + isa8 → rule 3 no-op");

    /* Scenario M: 8088 on ISA-8 → rule 9 PASS */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class",  "8088", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bus.class",  "isa8", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.8086_bus")) == VERDICT_PASS,
          "Scenario M: 8088 + isa8 → rule 9 PASS");

    /* Scenario N: 8086 on PCI (impossible!) → rule 9 FAIL */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class",  "8086", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bus.class",  "pci",  CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.8086_bus")) == VERDICT_FAIL,
          "Scenario N: 8086 + pci → rule 9 FAIL");

    /* Scenario O: V20 on unknown bus → rule 9 WARN (softer) */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class",  "v20",     CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bus.class",  "unknown", CONF_LOW,  VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.8086_bus")) == VERDICT_WARN,
          "Scenario O: V20 + unknown bus → rule 9 WARN");

    /* Scenario P: 486DX on PCI → rule 9 not applicable (CPU class !=8086) */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class",  "GenuineIntel", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bus.class",  "pci",          CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.8086_bus") == NULL,
          "Scenario P: 486-class + pci → rule 9 no-op");

    /* Scenario Q: self-check keys absent → rule 4a no-op (nothing to judge) */
    memset(&t, 0, sizeof(t));
    consist_check(&t);
    CHECK(k(&t, "consistency.timing_independence") == NULL,
          "Scenario Q: no self-check → rule 4a no-op");

    /* Scenario R: PIT and BIOS agree within 15% → rule 4a PASS.
     * Target was 4 BIOS ticks = 219700 us. A 486 with working timing.c
     * would come back with pit_us ≈ 219700 +/- quantization noise
     * (1 PIT wrap is 54925 us, so +/-1 wrap across 4 equals ±25%; in
     * practice we'll observe far less because the wrap counter is
     * exact — the noise is only in the sub-wrap residue). */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "timing.cross_check.pit_us",  220500UL, "220500", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "timing.cross_check.bios_us", 219700UL, "219700", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.timing_independence")) == VERDICT_PASS,
          "Scenario R: PIT 220500us vs BIOS 219700us (0.4% diff) → rule 4a PASS");

    /* Scenario S: PIT reads HALF of BIOS (classic 16-bit overflow
     * fingerprint — ticks * 838 truncated at 65535 us every 78 ticks).
     * 50% divergence is well above the 15% threshold → WARN. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "timing.cross_check.pit_us",  110000UL, "110000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "timing.cross_check.bios_us", 219700UL, "219700", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.timing_independence")) == VERDICT_WARN,
          "Scenario S: PIT 110000us vs BIOS 219700us (50% diff) → rule 4a WARN");

    /* Scenario T: exactly at the 15% boundary on the PASS side. bios=100000,
     * delta allowed up to 15000 → pit=114999 should still PASS. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "timing.cross_check.pit_us",  114999UL, "114999", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "timing.cross_check.bios_us", 100000UL, "100000", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.timing_independence")) == VERDICT_PASS,
          "Scenario T: 15% boundary (14.999% diff) → rule 4a PASS");

    /* Scenario U: just past the boundary → WARN. bios=100000, pit=115001. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "timing.cross_check.pit_us",  115001UL, "115001", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "timing.cross_check.bios_us", 100000UL, "100000", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.timing_independence")) == VERDICT_WARN,
          "Scenario U: 15.001% diff → rule 4a WARN");

    /* Scenario V: PIT reads materially lower than BIOS. BIOS is the
     * reference denominator, so the threshold is 15% of bios_us = 18750;
     * the observed delta of 25000 clears it. This verifies the rule
     * fires when PIT reads materially lower than BIOS, mirroring
     * Scenario S in the opposite direction. (Note: not a "symmetric"
     * check — absolute delta is compared against 15% of BIOS, never
     * against the mean, so PIT-low and PIT-high of equal magnitude
     * are not equivalent tests.) */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "timing.cross_check.pit_us",  100000UL, "100000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "timing.cross_check.bios_us", 125000UL, "125000", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.timing_independence")) == VERDICT_WARN,
          "Scenario V: PIT 100000us vs BIOS 125000us (20% diff) → rule 4a WARN");

    /* Scenario W: pit_us=0 (self-check sentinel) → rule 4a no-op. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "timing.cross_check.pit_us",  0UL, "0", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "timing.cross_check.bios_us", 219700UL, "219700", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.timing_independence") == NULL,
          "Scenario W: pit_us=0 → rule 4a no-op (self-check bailed)");

    /* Scenario X: mixer DB says CT1745, probe confirms CT1745 → rule 7 PASS.
     * Models the healthy Vibra 16S / SB16 CT2230+ case on real hardware
     * where the DB's seeded value and the Interrupt Setup Register read
     * agree. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "audio.mixer_chip_expected", "CT1745", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "audio.mixer_chip_observed", "CT1745", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.audio_mixer_chip")) == VERDICT_PASS,
          "Scenario X: mixer expected=CT1745 + observed=CT1745 → rule 7 PASS");

    /* Scenario Y: DB expects CT1745 but probe returned "none" (mixer absent
     * or open-bus). Models a counterfeit card advertising SB16 DSP version
     * while physically missing the CT1745 mixer, or a hardware fault where
     * the mixer port doesn't respond. Rule 7 FAILs. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "audio.mixer_chip_expected", "CT1745", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "audio.mixer_chip_observed", "none",   CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.audio_mixer_chip")) == VERDICT_FAIL,
          "Scenario Y: mixer expected=CT1745 + observed=none → rule 7 FAIL");

    /* Scenario Z: DB has mixer_chip=unknown (most rows today), probe saw a
     * CT1745 on hardware → rule 7 WARN inviting the user to contribute a
     * DB entry. The specific text is not asserted; the verdict + presence
     * is sufficient. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "audio.mixer_chip_expected", "unknown", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "audio.mixer_chip_observed", "CT1745",  CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.audio_mixer_chip")) == VERDICT_WARN,
          "Scenario Z: mixer expected=unknown + observed=CT1745 → rule 7 WARN");

    /* Scenario AA: DB unknown, probe also saw no mixer (e.g. a card without
     * a CT1745 where the DB correctly doesn't claim one either). Rule 7
     * should stay silent — no useful signal. Prevents a wall of WARNs on
     * every run with a non-SB16 card. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "audio.mixer_chip_expected", "unknown", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "audio.mixer_chip_observed", "none",    CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.audio_mixer_chip") == NULL,
          "Scenario AA: mixer expected=unknown + observed=none → rule 7 no-op");

    /* Scenario BB: neither expected nor observed populated (e.g. /ONLY:DIAG
     * or a run without an SB detected) → rule 7 no-op. */
    memset(&t, 0, sizeof(t));
    consist_check(&t);
    CHECK(k(&t, "consistency.audio_mixer_chip") == NULL,
          "Scenario BB: mixer keys absent → rule 7 no-op");

    /* Scenario CC: FPU reported present + Whetstone ran with nonzero
     * result → rule 10 PASS. Models the healthy 486DX or 8088+8087 path. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "fpu.detected",               "integrated-486", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bench.fpu.whetstone_status", "ok",             CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "bench.fpu.k_whetstones",     11420UL, "11420", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.whetstone_fpu")) == VERDICT_PASS,
          "Scenario CC: FPU=integrated-486 + Whetstone=ok/11420 → rule 10 PASS");

    /* Scenario DD: FPU reported absent + Whetstone skipped → rule 10 PASS.
     * The normal no-8087 8088 path. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "fpu.detected",               "none",           CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bench.fpu.whetstone_status", "skipped_no_fpu", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.whetstone_fpu")) == VERDICT_PASS,
          "Scenario DD: FPU=none + Whetstone=skipped → rule 10 PASS");

    /* Scenario EE: FPU reported present but Whetstone skipped — detection
     * disagreement → rule 10 FAIL. Would indicate fpu.detected changed
     * between detect and bench (memory-corruption class of bug). */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "fpu.detected",               "387",            CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bench.fpu.whetstone_status", "skipped_no_fpu", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.whetstone_fpu")) == VERDICT_FAIL,
          "Scenario EE: FPU=387 + Whetstone=skipped → rule 10 FAIL");

    /* Scenario FF: FPU reported absent but Whetstone produced a number —
     * detect under-reported. Classic socketed-8087-missed-by-probe case.
     * → rule 10 FAIL. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "fpu.detected",               "none",       CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bench.fpu.whetstone_status", "ok",         CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "bench.fpu.k_whetstones",     500UL, "500", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.whetstone_fpu")) == VERDICT_FAIL,
          "Scenario FF: FPU=none + Whetstone=ok/500 → rule 10 FAIL (detect under-reported)");

    /* Scenario GG: fpu.detected present, bench keys absent → rule 10 no-op. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "fpu.detected", "integrated-486", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.whetstone_fpu") == NULL,
          "Scenario GG: fpu.detected present, bench keys absent → rule 10 no-op");

    /* Scenario HH: FPU present + Whetstone reported inconclusive_elapsed_zero
     * (measurement loop finished in under a PIT tick — emulator-artifact
     * territory, not a detection disagreement). Rule 10 must no-op on
     * any "inconclusive*" status; the WARN verdict already attached to
     * the status row by bench_whetstone surfaces the measurement issue
     * without implicating consistency. Pre-fix, strcmp(st,"ok") returned
     * false and the else branch FAILed the rule. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "fpu.detected", "integrated-486", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "bench.fpu.whetstone_status", "inconclusive_elapsed_zero",
                   CONF_LOW, VERDICT_WARN);
    consist_check(&t);
    CHECK(k(&t, "consistency.whetstone_fpu") == NULL,
          "Scenario HH: FPU=integrated-486 + Whetstone=inconclusive_elapsed_zero → rule 10 no-op");

    /* Scenario II: DB expects CT1745 but probe returned "unknown" (Interrupt
     * Setup byte read neither CT1745-shaped nor clearly-absent — the weird-
     * mixer bucket that audio.c's probe_mixer_chip documents as "needs
     * human triage via Rule 7 WARN"). Pre-fix, rule 7's trailing else
     * branch emitted FAIL here because ev != ov, contradicting audio.c's
     * classifier contract and crying wolf on legitimate CT1745 cards whose
     * register snapshot fell in the inconclusive range. Post-fix rule 7
     * must WARN, not FAIL, on observed=="unknown". */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "audio.mixer_chip_expected", "CT1745",  CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "audio.mixer_chip_observed", "unknown", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.audio_mixer_chip")) == VERDICT_WARN,
          "Scenario II: mixer expected=CT1745 + observed=unknown → rule 7 WARN (not FAIL)");

    /* Scenario JJ: Rule 4b bench in the expected range → PASS. Smoke-test of
     * the path that was uncovered before the v0.4 narration-extension work. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "bench.cpu.int_iters_per_sec",
                   7000000UL, "7000000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_low",
                   4700000UL, "4700000",  CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_high",
                   10500000UL, "10500000", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.cpu_ipc_bench")) == VERDICT_PASS,
          "Scenario JJ: bench 7M/s in 4.7M-10.5M range → rule 4b PASS");

    /* Scenario KK: bench under expected AND diagnose.cache.status reports the
     * cache is live ("cache_working" substring from diag_cache's verdict
     * string). v0.4 narration extension must finger TSR/thermal throttle,
     * NOT cache-disabled, because the cache diagnostic proves the cache is
     * working. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "bench.cpu.int_iters_per_sec",
                   1800000UL, "1800000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_low",
                   4700000UL, "4700000",  CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_high",
                   10500000UL, "10500000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.cache.status",
                   "pass (ratio=1.70 × — cache_working)",
                   CONF_HIGH, VERDICT_PASS);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.cpu_ipc_bench")) == VERDICT_WARN,
          "Scenario KK: bench under, cache working → rule 4b WARN");
    {
        const result_t *r = k(&t, "consistency.cpu_ipc_bench");
        CHECK(r && r->v.s && strstr(r->v.s, "TSR stealing") != NULL,
              "Scenario KK: narration names TSR/thermal (cache exonerated)");
        CHECK(r && r->v.s && strstr(r->v.s, "cache diag PASS") != NULL,
              "Scenario KK: narration cites cache diag PASS");
    }

    /* Scenario LL: bench under expected AND diagnose.cache.status reports
     * "no_cache_effect" — cache is provably dead (BIOS-disabled or absent).
     * v0.4 narration extension must finger the cache, NOT TSR. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "bench.cpu.int_iters_per_sec",
                   1200000UL, "1200000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_low",
                   4700000UL, "4700000",  CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_high",
                   10500000UL, "10500000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.cache.status",
                   "fail (ratio=1.01 × — no_cache_effect)",
                   CONF_HIGH, VERDICT_FAIL);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.cpu_ipc_bench")) == VERDICT_WARN,
          "Scenario LL: bench under, cache dead → rule 4b WARN");
    {
        const result_t *r = k(&t, "consistency.cpu_ipc_bench");
        CHECK(r && r->v.s && strstr(r->v.s, "cache disabled in BIOS or absent") != NULL,
              "Scenario LL: narration names cache-BIOS-disabled (TSR exonerated)");
    }

    /* Scenario MM: bench under expected, diagnose.cache.status absent
     * (e.g. skipped on an 8088-class floor box where cache.present=no). Must
     * fall back to the original three-cause ambiguous narration — the rule
     * has no evidence to blame any one cause. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "bench.cpu.int_iters_per_sec",
                   1800000UL, "1800000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_low",
                   4700000UL, "4700000",  CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_high",
                   10500000UL, "10500000", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    {
        const result_t *r = k(&t, "consistency.cpu_ipc_bench");
        CHECK(v_of(r) == VERDICT_WARN,
              "Scenario MM: bench under, no cache status → rule 4b WARN");
        CHECK(r && r->v.s && strstr(r->v.s,
              "(throttle, cache disabled, or TSR stealing cycles)") != NULL,
              "Scenario MM: narration falls back to ambiguous three-cause form");
    }

    /* Scenario NN: bench ABOVE expected (overclock / CPU misidentified). The
     * cache.status field is irrelevant to this branch and must not affect
     * narration — over-range uses its own message. */
    memset(&t, 0, sizeof(t));
    report_add_u32(&t, "bench.cpu.int_iters_per_sec",
                   15000000UL, "15000000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_low",
                   4700000UL, "4700000",  CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "cpu.bench_iters_high",
                   10500000UL, "10500000", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.cache.status",
                   "pass (ratio=1.70 × — cache_working)",
                   CONF_HIGH, VERDICT_PASS);
    consist_check(&t);
    {
        const result_t *r = k(&t, "consistency.cpu_ipc_bench");
        CHECK(v_of(r) == VERDICT_WARN,
              "Scenario NN: bench above → rule 4b WARN (overclock)");
        CHECK(r && r->v.s && strstr(r->v.s,
              "overclock or CPU misidentified") != NULL,
              "Scenario NN: narration uses over-range message (cache status ignored)");
    }

    /* ---- Rule 11: dma_class_coherence ---------------------------------- */

    /* Scenario OO: XT-class CPU (8088) + slave correctly skipped by diag_dma
     * → PASS (coherent). */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class", "8088", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.dma.ch5_status",
                   "skipped_no_slave (XT-class machine)",
                   CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.dma_class_coherence")) == VERDICT_PASS,
          "Scenario OO: 8088 + ch5 skipped → rule 11 PASS");

    /* Scenario PP: XT-class CPU but diag_dma reports ch5 responded with
     * the test pattern — contradiction between detection paths. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class", "8088", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.dma.ch5_status", "pass",
                   CONF_HIGH, VERDICT_PASS);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.dma_class_coherence")) == VERDICT_WARN,
          "Scenario PP: 8088 + ch5 pass → rule 11 WARN (contradiction)");

    /* Scenario QQ: NEC V20 is an 8088-compatible; treat as XT. Same WARN
     * shape as Scenario PP. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class", "v20", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.dma.ch5_status", "pass",
                   CONF_HIGH, VERDICT_PASS);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.dma_class_coherence")) == VERDICT_WARN,
          "Scenario QQ: V20 + ch5 pass → rule 11 WARN (contradiction)");

    /* Scenario RR: V30 (8086-compatible but still XT-class) + safely
     * skipped slave → PASS. Covers the V30 branch of the cpu.class match. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class", "v30", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.dma.ch5_status",
                   "skipped_no_slave (XT-class machine)",
                   CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(v_of(k(&t, "consistency.dma_class_coherence")) == VERDICT_PASS,
          "Scenario RR: V30 + ch5 skipped → rule 11 PASS");

    /* Scenario SS: AT-class CPU (486) with slave passing the probe. Rule
     * doesn't apply — no emit, no contradiction (this is the normal case
     * on modern AT hardware). */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class", "intel", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.dma.ch5_status", "pass",
                   CONF_HIGH, VERDICT_PASS);
    consist_check(&t);
    CHECK(k(&t, "consistency.dma_class_coherence") == NULL,
          "Scenario SS: AT-class CPU + ch5 pass → rule 11 no-op");

    /* Scenario TT: AT-class CPU with ch5 safety-skipped by some future
     * code path. Rule stays silent — AT-class doesn't invoke rule 11
     * regardless of what the slave channel says. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class", "486-no-cpuid",
                   CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.dma.ch5_status",
                   "skipped_no_slave (XT-class machine)",
                   CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.dma_class_coherence") == NULL,
          "Scenario TT: AT-class + ch5 skipped → rule 11 no-op");

    /* Scenario UU: cpu.class present but no ch5_status key (diag_dma
     * didn't run — /SKIP:DMA, or module dropped from build). Rule must
     * no-op rather than assume coherence. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class", "8088", CONF_HIGH, VERDICT_UNKNOWN);
    consist_check(&t);
    CHECK(k(&t, "consistency.dma_class_coherence") == NULL,
          "Scenario UU: XT-class + ch5_status absent → rule 11 no-op");

    /* Scenario VV: ch5_status present but cpu.class absent (detect_cpu
     * failed). Rule no-op — cannot judge coherence without the CPU
     * claim to check against. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "diagnose.dma.ch5_status", "pass",
                   CONF_HIGH, VERDICT_PASS);
    consist_check(&t);
    CHECK(k(&t, "consistency.dma_class_coherence") == NULL,
          "Scenario VV: ch5 present + cpu.class absent → rule 11 no-op");

    /* Scenario WW: XT-class CPU but ch5 reported FAIL (implausible —
     * slave port responded but with wrong pattern). Rule must stay
     * silent: the per-channel FAIL row already surfaces the anomaly;
     * emitting a second WARN here would be double-counting. */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.class", "8088", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(&t, "diagnose.dma.ch5_status", "fail",
                   CONF_HIGH, VERDICT_FAIL);
    consist_check(&t);
    CHECK(k(&t, "consistency.dma_class_coherence") == NULL,
          "Scenario WW: XT + ch5 fail → rule 11 no-op (avoid double-counting)");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
