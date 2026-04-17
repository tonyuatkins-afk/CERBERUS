#ifndef CERBERUS_VIDEO_DB_H
#define CERBERUS_VIDEO_DB_H

typedef struct {
    const char *bios_signature;
    const char *vendor;
    const char *chipset;
    const char *family;
    const char *notes;
} video_db_entry_t;

extern const video_db_entry_t video_db[];
extern const unsigned int     video_db_count;

#endif
