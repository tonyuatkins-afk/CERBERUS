/*
 * Host-side test of the diagnose modules. The ALU vectors and memory
 * patterns are pure-logic — no emulator, no hardware — so the full
 * diag_cpu / diag_mem functions can run on the host and we assert the
 * result table comes back with PASS verdicts.
 *
 * This catches:
 *   - Typos in the hand-computed expected values (the authoring hazard)
 *   - Accidental table size/count mismatches after CSV/DB edits
 *   - Regressions in report_set_verdict
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"
#include "../../src/diag/diag_cpu.c"
#include "../../src/diag/diag_mem.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

int main(void)
{
    result_table_t t;
    const result_t *r;

    printf("=== CERBERUS host unit test: diag (cpu + memory) ===\n");

    /* Seed the table with a detect entry so diag can attach its verdict */
    memset(&t, 0, sizeof(t));
    report_add_str(&t, "cpu.detected",           "Host i486DX",  CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(&t, "memory.conventional_kb", 640UL, "640",   CONF_HIGH, VERDICT_UNKNOWN);

    diag_cpu(&t);
    diag_mem(&t);

    /* All ALU/MUL/shift tests should pass on a healthy host. */
    r = find_key(&t, "diagnose.cpu.alu");
    CHECK(r && r->verdict == VERDICT_PASS, "ALU verdict PASS");
    r = find_key(&t, "diagnose.cpu.mul");
    CHECK(r && r->verdict == VERDICT_PASS, "MUL verdict PASS");
    r = find_key(&t, "diagnose.cpu.shift");
    CHECK(r && r->verdict == VERDICT_PASS, "shift verdict PASS");

    /* cpu.detected should have VERDICT_PASS set by diag_cpu */
    r = find_key(&t, "cpu.detected");
    CHECK(r && r->verdict == VERDICT_PASS, "cpu.detected verdict upgraded to PASS");

    /* Memory patterns */
    r = find_key(&t, "diagnose.memory.walking_1s");
    CHECK(r && r->verdict == VERDICT_PASS, "walking_1s verdict PASS");
    r = find_key(&t, "diagnose.memory.walking_0s");
    CHECK(r && r->verdict == VERDICT_PASS, "walking_0s verdict PASS");
    r = find_key(&t, "diagnose.memory.addr_in_addr");
    CHECK(r && r->verdict == VERDICT_PASS, "addr_in_addr verdict PASS");

    /* memory.conventional_kb should now have VERDICT_PASS */
    r = find_key(&t, "memory.conventional_kb");
    CHECK(r && r->verdict == VERDICT_PASS, "memory.conventional_kb verdict upgraded to PASS");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
