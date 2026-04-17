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

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
