/* Host-side structural test for video_db.
 * Real signature-scan validation happens on target; host test verifies
 * schema integrity (non-empty fields, valid family tokens). */

#include "../../src/detect/video_db.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

static int valid_family(const char *f)
{
    return strcmp(f, "mda")      == 0 ||
           strcmp(f, "cga")      == 0 ||
           strcmp(f, "hercules") == 0 ||
           strcmp(f, "ega")      == 0 ||
           strcmp(f, "vga")      == 0 ||
           strcmp(f, "svga")     == 0;
}

int main(void)
{
    unsigned int i;
    int all_non_empty = 1;
    int all_valid_fam = 1;
    int has_trident = 0, has_s3 = 0, has_cirrus = 0, has_ati = 0;

    printf("=== CERBERUS host unit test: video_db ===\n");
    CHECK(video_db_count > 0, "video_db has entries");

    for (i = 0; i < video_db_count; i++) {
        const video_db_entry_t *e = &video_db[i];
        if (!e->bios_signature || !*e->bios_signature) all_non_empty = 0;
        if (!e->chipset        || !*e->chipset)        all_non_empty = 0;
        if (!valid_family(e->family))                  all_valid_fam = 0;
        if (strstr(e->chipset, "Trident") || strstr(e->chipset, "TGUI") ||
            strstr(e->chipset, "TVGA"))                has_trident = 1;
        if (strstr(e->chipset, "S3") || strstr(e->chipset, "Trio") ||
            strstr(e->chipset, "ViRGE"))               has_s3 = 1;
        if (strstr(e->chipset, "Cirrus") || strstr(e->chipset, "CL-GD"))
                                                       has_cirrus = 1;
        if (strstr(e->chipset, "ATI") || strstr(e->chipset, "Mach"))
                                                       has_ati = 1;
    }

    CHECK(all_non_empty, "all entries have non-empty bios_signature + chipset");
    CHECK(all_valid_fam, "all entries have a valid family token");
    CHECK(has_trident,   "Trident family represented");
    CHECK(has_s3,        "S3 family represented");
    CHECK(has_cirrus,    "Cirrus Logic represented");
    CHECK(has_ati,       "ATI family represented");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
