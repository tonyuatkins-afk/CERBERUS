/*
 * Host-side unit test for cpu_db.
 *
 * cpu_db.c is generated from hw_db/cpus.csv. These tests pin a handful of
 * well-known lookups so a regeneration that changes the lookup semantics
 * fails CI. Updating the CSV is expected to change friendly-name strings
 * but the key lookups (is there an entry for Intel family=5 model=2?)
 * should remain stable.
 */

#include "../../src/detect/cpu_db.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

int main(void)
{
    const cpu_db_entry_t *e;

    printf("=== CERBERUS host unit test: cpu_db ===\n");

    CHECK(cpu_db_count > 0, "cpu_db has at least one entry");

    /* Known CPUID triplets */
    e = cpu_db_lookup_cpuid("GenuineIntel", 4, 1, 5);
    CHECK(e != NULL,                                         "Intel family=4 model=1 stepping=5 -> match");
    CHECK(e != NULL && strstr(e->friendly, "i486DX") != NULL,
          "Intel 4/1 friendly name mentions i486DX");

    e = cpu_db_lookup_cpuid("GenuineIntel", 4, 2, 0);
    CHECK(e != NULL,                                         "Intel i486SX lookup");
    CHECK(e != NULL && strstr(e->friendly, "i486SX") != NULL,
          "i486SX friendly name correct");

    e = cpu_db_lookup_cpuid("GenuineIntel", 5, 2, 0);
    CHECK(e != NULL,                                         "Intel Pentium P54C lookup");

    e = cpu_db_lookup_cpuid("AuthenticAMD", 5, 6, 0);
    CHECK(e != NULL,                                         "AMD K6 lookup");

    e = cpu_db_lookup_cpuid("CyrixInstead", 5, 2, 0);
    CHECK(e != NULL,                                         "Cyrix 6x86 lookup");

    /* Negative cases */
    e = cpu_db_lookup_cpuid("GenuineIntel", 99, 0, 0);
    CHECK(e == NULL, "unknown family returns NULL");

    e = cpu_db_lookup_cpuid("UnknownVendor", 5, 2, 0);
    CHECK(e == NULL, "unknown vendor returns NULL");

    e = cpu_db_lookup_cpuid("GenuineIntel", 5, 2, 99);
    CHECK(e == NULL, "stepping out of DB range returns NULL");

    /* Legacy lookups */
    e = cpu_db_lookup_legacy("8088");
    CHECK(e != NULL,                                         "legacy 8088 match");
    CHECK(e != NULL && e->match_kind == CPU_DB_MATCH_LEGACY, "legacy match_kind is LEGACY");

    e = cpu_db_lookup_legacy("286");
    CHECK(e != NULL, "legacy 286 match");

    e = cpu_db_lookup_legacy("486-no-cpuid");
    CHECK(e != NULL, "legacy 486-no-cpuid match");

    e = cpu_db_lookup_legacy("unknown-class");
    CHECK(e == NULL, "unknown legacy class returns NULL");

    e = cpu_db_lookup_legacy(NULL);
    CHECK(e == NULL, "NULL legacy class returns NULL");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
