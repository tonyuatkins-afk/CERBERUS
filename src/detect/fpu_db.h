/*
 * FPU identification database.
 *
 * Generated from hw_db/fpus.csv via hw_db/build_fpu_db.py — edit the CSV,
 * not fpu_db.c. Lookup is by tag (a unique string produced by the FPU
 * probe + CPU-class logic in fpu.c).
 */
#ifndef CERBERUS_FPU_DB_H
#define CERBERUS_FPU_DB_H

typedef struct {
    const char *tag;        /* e.g. "integrated-486", "387", "none" */
    const char *friendly;   /* Human-readable name */
    const char *vendor;     /* Empty string when unknown */
    const char *notes;      /* Empty string when none */
} fpu_db_entry_t;

extern const fpu_db_entry_t fpu_db[];
extern const unsigned int   fpu_db_count;

const fpu_db_entry_t *fpu_db_lookup(const char *tag);

#endif
