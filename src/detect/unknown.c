/*
 * Unknown-hardware submission path. See unknown.h for rationale.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifndef CERBERUS_HOST_TEST
#include <dos.h>
#endif
#include "unknown.h"
#include "../cerberus.h"

#define MAX_UNKNOWN_CAPTURES 32
#define UNK_PATH_CAP         96

typedef struct {
    const char *subsystem;
    const char *summary;
    const char *detail;
} unknown_capture_t;

static unknown_capture_t captures[MAX_UNKNOWN_CAPTURES];
static unsigned int      capture_count;
static char              unk_path[UNK_PATH_CAP] = "";
static int               unk_disabled         = 0;

/* ----------------------------------------------------------------------- */
/* Path probing (mirrors crumb.c)                                           */
/* ----------------------------------------------------------------------- */

static int try_probe_path(const char *dir)
{
    char probe[UNK_PATH_CAP];
    int fd;
    if (dir && *dir) {
        unsigned int n = (unsigned int)strlen(dir);
        char sep = dir[n - 1];
        sprintf(probe, (sep == '\\' || sep == '/') ? "%sCERBERUS.UNK" : "%s\\CERBERUS.UNK", dir);
    } else {
        strcpy(probe, "CERBERUS.UNK");
    }
    fd = open(probe, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, S_IWRITE);
    if (fd < 0) return 0;
    write(fd, "P", 1);
    close(fd);
    unlink(probe);
    strncpy(unk_path, probe, UNK_PATH_CAP - 1);
    unk_path[UNK_PATH_CAP - 1] = '\0';
    return 1;
}

void unknown_init(void)
{
    const char *e;
    capture_count = 0;
    unk_disabled = 0;
    unk_path[0] = '\0';

    if (try_probe_path(NULL)) return;
    e = getenv("TEMP");
    if (e && *e && try_probe_path(e)) return;
    e = getenv("TMP");
    if (e && *e && try_probe_path(e)) return;
    unk_disabled = 1;  /* silent: end-of-run card will skip without disk */
}

/* ----------------------------------------------------------------------- */
/* Record + finalize                                                        */
/* ----------------------------------------------------------------------- */

void unknown_record(const char *subsystem, const char *summary,
                    const char *detail)
{
    if (capture_count >= MAX_UNKNOWN_CAPTURES) return;
    captures[capture_count].subsystem = subsystem ? subsystem : "?";
    captures[capture_count].summary   = summary   ? summary   : "";
    captures[capture_count].detail    = detail;  /* may be NULL */
    capture_count++;
}

unsigned int unknown_count(void) { return capture_count; }

static void render_card(void)
{
    unsigned int i;
    printf("\n");
    printf("+-- UNKNOWN HARDWARE CAPTURED ------------------------------------+\n");
    printf("| %u %s not in the CERBERUS database.\n",
           capture_count, capture_count == 1 ? "item" : "items");
    for (i = 0; i < capture_count; i++) {
        printf("|   [%s] %s\n",
               captures[i].subsystem, captures[i].summary);
    }
    if (!unk_disabled) {
        printf("|\n");
        printf("| Dump written to: %s\n", unk_path);
    }
    printf("|\n");
    printf("| Help grow the DB - submit a GitHub issue:\n");
    printf("|   https://github.com/tonyuatkins-afk/CERBERUS/issues/new\n");
    printf("|   (use the 'hardware submission' template)\n");
    printf("+-----------------------------------------------------------------+\n");
}

void unknown_finalize(void)
{
    FILE *f;
    unsigned int i;

    if (capture_count == 0) return;

    if (!unk_disabled) {
        f = fopen(unk_path, "wt");
        if (f) {
            fprintf(f, "# CERBERUS.UNK: captured unknown hardware\n");
            fprintf(f, "# Tool version: %s\n", CERBERUS_VERSION);
            fprintf(f, "# Total captures: %u\n", capture_count);
            fprintf(f, "#\n");
            fprintf(f, "# Paste this file into a GitHub issue using the\n");
            fprintf(f, "# 'hardware submission' template at:\n");
            fprintf(f, "#   https://github.com/tonyuatkins-afk/CERBERUS/issues/new\n");
            fprintf(f, "#\n\n");
            for (i = 0; i < capture_count; i++) {
                fprintf(f, "[%s]\n", captures[i].subsystem);
                fprintf(f, "summary=%s\n", captures[i].summary);
                if (captures[i].detail) {
                    fprintf(f, "detail=%s\n", captures[i].detail);
                }
                fprintf(f, "\n");
            }
            fclose(f);
        }
    }

    render_card();
}
