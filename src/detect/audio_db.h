#ifndef CERBERUS_AUDIO_DB_H
#define CERBERUS_AUDIO_DB_H

typedef struct {
    const char *match_key;  /* composite key, e.g. "opl3:0400" */
    const char *friendly;
    const char *vendor;
    const char *notes;
    const char *mixer_chip; /* expected CT-number at base+4/+5, or
                             * "unknown" / "none" — fed to consist rule 7 */
} audio_db_entry_t;

extern const audio_db_entry_t audio_db[];
extern const unsigned int     audio_db_count;

const audio_db_entry_t *audio_db_lookup(const char *match_key);

#endif
