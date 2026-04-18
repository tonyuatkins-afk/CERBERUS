/*
 * Memory pattern diagnostic — Phase 2 Task 2.2.
 *
 * Runs three classical RAM integrity patterns on a 4KB static buffer:
 *
 *   1. Walking-1s — write 2^n to every cell, verify. Catches stuck-at-0
 *      bit faults.
 *   2. Walking-0s — write ~(2^n) to every cell, verify. Catches stuck-
 *      at-1 bit faults.
 *   3. Address-in-address — write the LOW byte of each offset into that
 *      offset, verify. Catches address-line faults and decode errors.
 *
 * Scope is deliberately limited to our own 4KB DGROUP buffer — this is
 * a plausibility check, not an exhaustive memory sweep. DOS data
 * structures fill conventional memory at unpredictable offsets, and
 * damaging a TSR's workspace or the DOS kernel during a probe would be
 * worse than reporting an incomplete test. A comprehensive memtest86-
 * style walk over free conventional RAM belongs in a future pass that
 * negotiates with DOS via INT 21h to allocate the largest free block.
 *
 * Each pattern that fails triggers VERDICT_FAIL on memory.conventional_kb
 * and emits a diagnose.memory.<pattern>=fail row with the first bad
 * offset. VERDICT_PASS otherwise.
 */

#include <stdio.h>
#include <string.h>
#include "diag.h"
#include "../core/report.h"

#define DIAG_MEM_BUF_SIZE 4096

static unsigned char diag_mem_buf[DIAG_MEM_BUF_SIZE];

/* FAIL-path detail buffer. report_add_str stores the value pointer
 * verbatim (report.c:55), so a stack-local detail[] would dangle after
 * diag_mem returns and the INI writer + UI renderer would read garbage.
 * The three FAIL paths (walking_1s / walking_0s / addr_in_addr) each
 * early-return, so at most one sprintf writes this per call — one
 * shared static is safe here. */
static char diag_mem_detail[64];

/* Walking-1s: for each bit position n in 0..7, fill the buffer with the
 * value 1<<n and verify. Catches a bit stuck at 0. */
static int pattern_walking_1s(unsigned int *out_first_bad_offset)
{
    unsigned int bit;
    unsigned int i;
    for (bit = 0; bit < 8; bit++) {
        unsigned char val = (unsigned char)(1u << bit);
        memset(diag_mem_buf, val, DIAG_MEM_BUF_SIZE);
        for (i = 0; i < DIAG_MEM_BUF_SIZE; i++) {
            if (diag_mem_buf[i] != val) {
                *out_first_bad_offset = i;
                return 0;
            }
        }
    }
    return 1;
}

/* Walking-0s: complementary — for each bit n, fill with ~(1<<n) and
 * verify. Catches a bit stuck at 1. */
static int pattern_walking_0s(unsigned int *out_first_bad_offset)
{
    unsigned int bit;
    unsigned int i;
    for (bit = 0; bit < 8; bit++) {
        unsigned char val = (unsigned char)~(1u << bit);
        memset(diag_mem_buf, val, DIAG_MEM_BUF_SIZE);
        for (i = 0; i < DIAG_MEM_BUF_SIZE; i++) {
            if (diag_mem_buf[i] != val) {
                *out_first_bad_offset = i;
                return 0;
            }
        }
    }
    return 1;
}

/* Address-in-address: write the LOW byte of each offset into that
 * offset. A faulty address line that maps offset X to offset Y will
 * cause the value at X to read back as Y's low byte. */
static int pattern_addr_in_addr(unsigned int *out_first_bad_offset)
{
    unsigned int i;
    for (i = 0; i < DIAG_MEM_BUF_SIZE; i++) {
        diag_mem_buf[i] = (unsigned char)(i & 0xFF);
    }
    for (i = 0; i < DIAG_MEM_BUF_SIZE; i++) {
        if (diag_mem_buf[i] != (unsigned char)(i & 0xFF)) {
            *out_first_bad_offset = i;
            return 0;
        }
    }
    return 1;
}

void diag_mem(result_table_t *t)
{
    unsigned int bad_off = 0;

    if (!pattern_walking_1s(&bad_off)) {
        sprintf(diag_mem_detail, "walking-1s failed at offset %u", bad_off);
        report_add_str(t, "diagnose.memory.walking_1s", diag_mem_detail,
                       CONF_HIGH, VERDICT_FAIL);
        report_set_verdict(t, "memory.conventional_kb", VERDICT_FAIL);
        return;
    }
    if (!pattern_walking_0s(&bad_off)) {
        sprintf(diag_mem_detail, "walking-0s failed at offset %u", bad_off);
        report_add_str(t, "diagnose.memory.walking_0s", diag_mem_detail,
                       CONF_HIGH, VERDICT_FAIL);
        report_set_verdict(t, "memory.conventional_kb", VERDICT_FAIL);
        return;
    }
    if (!pattern_addr_in_addr(&bad_off)) {
        sprintf(diag_mem_detail, "address-in-address failed at offset %u", bad_off);
        report_add_str(t, "diagnose.memory.addr_in_addr", diag_mem_detail,
                       CONF_HIGH, VERDICT_FAIL);
        report_set_verdict(t, "memory.conventional_kb", VERDICT_FAIL);
        return;
    }

    report_add_str(t, "diagnose.memory.walking_1s",    "pass", CONF_HIGH, VERDICT_PASS);
    report_add_str(t, "diagnose.memory.walking_0s",    "pass", CONF_HIGH, VERDICT_PASS);
    report_add_str(t, "diagnose.memory.addr_in_addr",  "pass", CONF_HIGH, VERDICT_PASS);
    report_set_verdict(t, "memory.conventional_kb", VERDICT_PASS);
}
