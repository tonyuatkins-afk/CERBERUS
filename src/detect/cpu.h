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

#endif
