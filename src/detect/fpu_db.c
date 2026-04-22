/*
 * AUTO-GENERATED — DO NOT EDIT.
 *
 * Regenerate with: python hw_db/build_fpu_db.py
 * Source: hw_db/fpus.csv (15 entries)
 */

#include "fpu_db.h"
#include <string.h>

const fpu_db_entry_t fpu_db[] = {
    { "none", "No FPU present", "", "8086/88/286/386SX or 486SX with no external coprocessor" },
    { "8087", "Intel 8087", "Intel", "Original 8086/8088 coprocessor (pre-1985)" },
    { "287", "Intel 80287", "Intel", "286-era coprocessor (also AMD Am287 and 80C287A)" },
    { "387", "Intel 80387 (DX/SX)", "Intel", "386-era coprocessor with affine infinity" },
    { "287-compat", "287-compatible coprocessor", "various", "IIT 2C87 / ULSI 83C287 / Cyrix FasMath 287+" },
    { "387-compat", "387-compatible coprocessor", "various", "ULSI 83C387 / Cyrix FasMath 387+ (IIT 3C87 discriminated separately via iit-3c87 tag)" },
    { "iit-3c87", "IIT 3C87", "IIT", "386-era 387-compatible coprocessor with extended matrix-math registers" },
    { "487sx", "Intel i487SX", "Intel", "i486SX companion coprocessor \x2014 actually a full i486DX that disables the host SX" },
    { "integrated-486", "Integrated i486DX FPU", "Intel", "On-die x87 for 486DX/DX2/DX4 family" },
    { "integrated-pentium", "Integrated Pentium FPU", "Intel", "On-die x87 for P5/P54C/P55C and later" },
    { "integrated-amd", "Integrated AMD FPU", "AMD", "On-die x87 for Am486DX/Am5x86/K5/K6" },
    { "integrated-cyrix", "Integrated Cyrix FPU", "Cyrix", "On-die x87 for Cx486DX/5x86/6x86 family" },
    { "rapidcad", "Intel RapidCAD", "Intel", "486-class CPU in 386 socket with 387-class FPU asynchronous clock" },
    { "weitek", "Weitek non-x87 coprocessor", "Weitek", "1167/3167/4167 \x2014 not x87-compatible; uses separate ISA" },
    { "external-unknown", "External FPU (unidentified)", "", "FPU present but class/vendor couldn't be refined" },
};

const unsigned int fpu_db_count = 15;

const fpu_db_entry_t *fpu_db_lookup(const char *tag)
{
    unsigned int i;
    if (!tag) return (const fpu_db_entry_t *)0;
    for (i = 0; i < fpu_db_count; i++) {
        if (strcmp(fpu_db[i].tag, tag) == 0) return &fpu_db[i];
    }
    return (const fpu_db_entry_t *)0;
}
