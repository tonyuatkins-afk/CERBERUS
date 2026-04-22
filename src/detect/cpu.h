#ifndef CERBERUS_DETECT_CPU_H
#define CERBERUS_DETECT_CPU_H

typedef enum {
    CPU_CLASS_UNKNOWN      = 0,
    CPU_CLASS_8086         = 1,
    CPU_CLASS_286          = 2,
    CPU_CLASS_386          = 3,
    CPU_CLASS_486_NOCPUID  = 4,
    CPU_CLASS_CPUID        = 5
} cpu_class_t;

/* Set by detect_cpu; read by downstream detect modules (FPU in particular
 * needs to know the CPU class to decide integrated-vs-external). */
cpu_class_t cpu_get_class(void);

/* v0.7.1: Time Stamp Counter (RDTSC) availability. Returns 1 only when
 * this CPU reports CPUID leaf 1 EDX bit 4 set AND detect_cpu() has run.
 * Depends on cpu.c's cached leaf1_regs — zero-initialized until the
 * CPUID-capable branch of detect_cpu populates it, so the result is
 * safely 0 on pre-CPUID CPUs or before Phase 1 runs. Used by timing.c
 * to gate the RDTSC backend. */
int         cpu_has_tsc(void);

#endif
