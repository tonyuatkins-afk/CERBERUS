/*
 * AUTO-GENERATED — DO NOT EDIT.
 *
 * Regenerate with: python hw_db/build_cpu_db.py
 * Source: hw_db/cpus.csv (34 entries)
 */

#include "cpu_db.h"
#include <string.h>

const cpu_db_entry_t cpu_db[] = {
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 0, 0, 15, "", "Intel i486DX (A-step)", "early i486DX with FDIV-like microcode quirks" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 1, 0, 15, "", "Intel i486DX (B/C-step)", "mainstream i486DX-25/33/50" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 2, 0, 15, "", "Intel i486SX", "no integrated FPU" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 3, 0, 15, "", "Intel i486DX2", "clock-doubled i486DX" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 4, 0, 15, "", "Intel i486SL", "SL-enhanced mobile variant" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 5, 0, 15, "", "Intel i486SX2", "clock-doubled i486SX (rare)" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 7, 0, 15, "", "Intel i486DX2 Write-Back Enhanced", "" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 8, 0, 15, "", "Intel i486DX4", "clock-tripled i486DX" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 4, 9, 0, 15, "", "Intel i486DX4 Write-Back Enhanced", "" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 5, 1, 0, 15, "", "Intel Pentium 60/66", "FDIV bug on stepping 0-3" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 5, 2, 0, 15, "", "Intel Pentium P54C", "" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 5, 4, 0, 15, "", "Intel Pentium MMX", "" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 6, 1, 0, 15, "", "Intel Pentium Pro", "" },
    { CPU_DB_MATCH_CPUID, "GenuineIntel", 6, 3, 0, 15, "", "Intel Pentium II", "" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 4, 3, 0, 15, "", "AMD Am486DX2", "" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 4, 7, 0, 15, "", "AMD Am486DX2 Write-Back", "" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 4, 8, 0, 15, "", "AMD Am486DX4", "" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 4, 9, 0, 15, "", "AMD Am486DX4 Write-Back", "" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 4, 14, 0, 15, "", "AMD Am5x86-P75", "486 platform 133MHz" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 4, 15, 0, 15, "", "AMD Am5x86 Write-Back", "" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 5, 0, 0, 15, "", "AMD K5 SSA/5", "" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 5, 1, 0, 15, "", "AMD K5 PR90/100/120/133", "" },
    { CPU_DB_MATCH_CPUID, "AuthenticAMD", 5, 6, 0, 15, "", "AMD K6", "" },
    { CPU_DB_MATCH_CPUID, "CyrixInstead", 4, 4, 0, 15, "", "Cyrix MediaGX", "" },
    { CPU_DB_MATCH_CPUID, "CyrixInstead", 5, 2, 0, 15, "", "Cyrix 6x86 (M1)", "" },
    { CPU_DB_MATCH_CPUID, "CyrixInstead", 5, 4, 0, 15, "", "Cyrix GXm", "" },
    { CPU_DB_MATCH_CPUID, "CyrixInstead", 6, 0, 0, 15, "", "Cyrix 6x86MX (M2)", "" },
    { CPU_DB_MATCH_LEGACY, "", 0, 0, 0, 0, "8086", "Intel 8086 or compatible", "" },
    { CPU_DB_MATCH_LEGACY, "", 0, 0, 0, 0, "8088", "IBM PC-class 8088 or compatible", "" },
    { CPU_DB_MATCH_LEGACY, "", 0, 0, 0, 0, "v20", "NEC V20 (8088-compatible with 80186 superset)", "" },
    { CPU_DB_MATCH_LEGACY, "", 0, 0, 0, 0, "v30", "NEC V30 (8086-compatible with 80186 superset)", "" },
    { CPU_DB_MATCH_LEGACY, "", 0, 0, 0, 0, "286", "Intel or AMD 80286", "" },
    { CPU_DB_MATCH_LEGACY, "", 0, 0, 0, 0, "386", "Intel/AMD/Cyrix 80386 class (CPUID absent)", "" },
    { CPU_DB_MATCH_LEGACY, "", 0, 0, 0, 0, "486-no-cpuid", "Early i486DX without CPUID support (SX327/SX328)", "" },
};

const unsigned int cpu_db_count = 34;

const cpu_db_entry_t *cpu_db_lookup_cpuid(const char *vendor,
                                          unsigned char family,
                                          unsigned char model,
                                          unsigned char stepping)
{
    unsigned int i;
    for (i = 0; i < cpu_db_count; i++) {
        const cpu_db_entry_t *e = &cpu_db[i];
        if (e->match_kind != CPU_DB_MATCH_CPUID)   continue;
        if (strcmp(e->vendor, vendor) != 0)        continue;
        if (e->family != family)                   continue;
        if (e->model  != model)                    continue;
        if (stepping < e->stepping_min)            continue;
        if (stepping > e->stepping_max)            continue;
        return e;
    }
    return (const cpu_db_entry_t *)0;
}

const cpu_db_entry_t *cpu_db_lookup_legacy(const char *legacy_class)
{
    unsigned int i;
    if (!legacy_class) return (const cpu_db_entry_t *)0;
    for (i = 0; i < cpu_db_count; i++) {
        const cpu_db_entry_t *e = &cpu_db[i];
        if (e->match_kind != CPU_DB_MATCH_LEGACY)  continue;
        if (strcmp(e->legacy_class, legacy_class) == 0) return e;
    }
    return (const cpu_db_entry_t *)0;
}
