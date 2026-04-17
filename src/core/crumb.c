/*
 * CERBERUS crash-recovery breadcrumb.
 *
 * Before each load-bearing test, write the test's name to a breadcrumb
 * file. If the test hangs the machine, the user reboots and the next
 * CERBERUS invocation finds the breadcrumb, reports what was running, and
 * suggests /SKIP:<name> to bypass the offending probe.
 *
 * Flaky vintage hardware is not hypothetical — every 8088/286/386 in the
 * fleet has at least one unusual INT handler, marginal cap, or dusty slot
 * that can hang a probe. The breadcrumb lets the user make progress
 * around a single bad subsystem rather than abandoning the whole run.
 *
 * Storage location falls through a preference chain:
 *   1. Current working directory (ideal for fixed HDD boots)
 *   2. %TEMP% env var (handles read-only media like floppies / CD-ROMs)
 *   3. %TMP% env var (same, different convention)
 *   4. Disabled — no storage available; print a one-line notice and move on
 *
 * DOS file buffering is the subtle trap: the directory entry and FAT are
 * not guaranteed flushed to disk until close(). INT 21h AH=68h ("commit
 * file") is the explicit flush. Watcom's `_dos_commit()` wraps it.
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
#include "crumb.h"

#define CRUMB_FILENAME "CERBERUS.LAST"
#define PATH_CAP       96
#define SKIPLIST_CAP   16
#define SKIP_NAME_CAP  32

static char crumb_path[PATH_CAP]   = "";
static int  crumb_disabled         = 0;

/* Skiplist: simple flat array of test names the user asked to bypass.
 * Population happens during CLI parse via /SKIP:<name>. Lookup is O(n×m)
 * but n is small (<16) and lookups are once per probe, so this is fine. */
static char skiplist[SKIPLIST_CAP][SKIP_NAME_CAP];
static unsigned int skiplist_count = 0;

/* ----------------------------------------------------------------------- */
/* Path probing                                                             */
/* ----------------------------------------------------------------------- */

static int try_write_test_file(const char *full_path)
{
    int fd = open(full_path, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, S_IWRITE);
    if (fd < 0) return 0;
    if (write(fd, "P", 1) != 1) { close(fd); return 0; }
    close(fd);
    unlink(full_path);
    return 1;
}

static int try_candidate(const char *dir)
{
    char probe[PATH_CAP];
    if (dir && *dir) {
        unsigned int dlen = (unsigned int)strlen(dir);
        char sep = dir[dlen - 1];
        sprintf(probe, (sep == '\\' || sep == '/') ? "%s%s" : "%s\\%s",
                dir, CRUMB_FILENAME);
    } else {
        strcpy(probe, CRUMB_FILENAME);
    }
    if (!try_write_test_file(probe)) return 0;
    strncpy(crumb_path, probe, PATH_CAP - 1);
    crumb_path[PATH_CAP - 1] = '\0';
    return 1;
}

void crumb_init(void)
{
    const char *e;

    /* 1st choice: CWD */
    if (try_candidate(NULL)) return;

    /* 2nd choice: %TEMP% */
    e = getenv("TEMP");
    if (e && *e && try_candidate(e)) return;

    /* 3rd choice: %TMP% */
    e = getenv("TMP");
    if (e && *e && try_candidate(e)) return;

    /* Give up — writable storage isn't available */
    crumb_disabled = 1;
    fputs("crash-recovery disabled: media appears read-only and no TEMP/TMP set\n",
          stderr);
}

/* ----------------------------------------------------------------------- */
/* Enter / exit                                                             */
/* ----------------------------------------------------------------------- */

void crumb_enter(const char *test_name)
{
    int fd;
    unsigned int n;
    if (crumb_disabled) return;
    if (!test_name) test_name = "<unknown>";

    fd = open(crumb_path, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, S_IWRITE);
    if (fd < 0) {
        /* Media may have gone read-only mid-run (floppy swap, etc.).
         * Stop trying after first mid-run failure to avoid chatter. */
        crumb_disabled = 1;
        return;
    }
    n = (unsigned int)strlen(test_name);
    write(fd, test_name, n);
    write(fd, "\n", 1);
    /* Flush DOS FS buffers so the file is recoverable after a hard hang.
     * INT 21h AH=68h is DOS 3.3+; Watcom's _dos_commit() wraps it and
     * silently no-ops on older DOS where buffering is less aggressive. */
#ifndef CERBERUS_HOST_TEST
    _dos_commit(fd);
#endif
    close(fd);
}

void crumb_exit(void)
{
    if (crumb_disabled) return;
    if (crumb_path[0]) unlink(crumb_path);
}

void crumb_check_previous(void)
{
    FILE *f;
    char line[SKIP_NAME_CAP];
    unsigned int len;
    if (crumb_disabled) return;
    if (!crumb_path[0]) return;

    f = fopen(crumb_path, "rt");
    if (!f) return;
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    fclose(f);

    /* Strip trailing newline / whitespace */
    len = (unsigned int)strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' ||
                       line[len-1] == ' '  || line[len-1] == '\t')) {
        line[--len] = '\0';
    }
    if (len == 0) {
        unlink(crumb_path);
        return;
    }

    printf("NOTICE: previous run hung during test: %s\n", line);
    printf("        Pass /SKIP:%s to bypass that probe on this run.\n", line);
    unlink(crumb_path);
}

/* ----------------------------------------------------------------------- */
/* Skiplist                                                                 */
/* ----------------------------------------------------------------------- */

void crumb_skiplist_add(const char *name)
{
    if (!name || !*name) return;
    if (skiplist_count >= SKIPLIST_CAP) return;
    strncpy(skiplist[skiplist_count], name, SKIP_NAME_CAP - 1);
    skiplist[skiplist_count][SKIP_NAME_CAP - 1] = '\0';
    skiplist_count++;
}

int crumb_skiplist_has(const char *name)
{
    unsigned int i;
    if (!name) return 0;
    for (i = 0; i < skiplist_count; i++) {
        if (strcmp(skiplist[i], name) == 0) return 1;
    }
    return 0;
}
