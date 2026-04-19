/*
 * CPU identification database.
 *
 * Generated C from hw_db/cpus.csv via hw_db/build_cpu_db.py — edit the CSV,
 * not cpu_db.c. Community contributions go in as CSV rows; the Python
 * generator regenerates the C.
 *
 * Two match kinds coexist:
 *
 *   CPU_DB_MATCH_CPUID  — vendor string + family + model + stepping range
 *                         Applies on CPUs where CPUID is available.
 *
 *   CPU_DB_MATCH_LEGACY — CPU class from EFLAGS/PUSHFD probes
 *                         Applies on pre-CPUID CPUs (8086 through 386 and
 *                         some early 486 parts).
 *
 * Lookup order: the table is walked top-to-bottom. More-specific stepping
 * ranges should precede generic catch-alls.
 */
#ifndef CERBERUS_CPU_DB_H
#define CERBERUS_CPU_DB_H

typedef enum {
    CPU_DB_MATCH_CPUID  = 0,
    CPU_DB_MATCH_LEGACY = 1
} cpu_db_match_kind_t;

typedef struct {
    cpu_db_match_kind_t match_kind;

    /* CPUID match fields (when match_kind == CPU_DB_MATCH_CPUID) */
    const char   *vendor;         /* CPUID vendor string */
    unsigned char family;
    unsigned char model;
    unsigned char stepping_min;
    unsigned char stepping_max;   /* inclusive */

    /* Legacy match (when match_kind == CPU_DB_MATCH_LEGACY) */
    const char   *legacy_class;   /* "8086"|"8088"|"v20"|"286"|"386"|"486-no-cpuid" */

    /* Output (common) */
    const char   *friendly;       /* Human-readable: "Intel i486DX2-66" */

    /* Rule 4b (cpu_ipc_bench) expected bench_cpu iters_per_sec range.
     * Zero on both means "no empirical data for this CPU yet" and rule
     * 4b no-ops on absence. detect_cpu emits these as
     * cpu.bench_iters_low/high so consist.c can compare against the
     * bench output without touching the DB. */
    unsigned long iters_low;
    unsigned long iters_high;

    const char   *notes;          /* Brief caveat / quirk reference */
} cpu_db_entry_t;

extern const cpu_db_entry_t cpu_db[];
extern const unsigned int   cpu_db_count;

/* Lookup by CPUID triplet — returns entry or NULL. stepping_min/max
 * ranges let one row cover multiple steppings. */
const cpu_db_entry_t *cpu_db_lookup_cpuid(const char *vendor,
                                          unsigned char family,
                                          unsigned char model,
                                          unsigned char stepping);

/* Lookup by legacy class token — returns entry or NULL. */
const cpu_db_entry_t *cpu_db_lookup_legacy(const char *legacy_class);

#endif
