#ifndef CERBERUS_BIOS_DB_H
#define CERBERUS_BIOS_DB_H

typedef struct {
    const char *signature;
    const char *vendor;
    const char *family;
    const char *era;
    const char *notes;
} bios_db_entry_t;

extern const bios_db_entry_t bios_db[];
extern const unsigned int    bios_db_count;

#endif
