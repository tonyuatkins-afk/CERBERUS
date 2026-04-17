/*
 * Host-side unit test for the crash-recovery breadcrumb.
 *
 * Covers: skiplist add/lookup, path-probe fallthrough, enter/exit file
 * lifecycle, and crumb_check_previous. The _dos_commit() DOS-only flush
 * is guarded by CERBERUS_HOST_TEST so this test links cleanly on Win32.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif
#include "../../src/core/crumb.c"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

static int file_exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int main(void)
{
    printf("=== CERBERUS host unit test: crumb ===\n");

    /* Skiplist */
    CHECK(crumb_skiplist_has("never-added") == 0, "skiplist: empty lookup misses");
    crumb_skiplist_add("cpu.cyrix");
    crumb_skiplist_add("audio.opl3");
    CHECK(crumb_skiplist_has("cpu.cyrix")  == 1, "skiplist: cpu.cyrix found");
    CHECK(crumb_skiplist_has("audio.opl3") == 1, "skiplist: audio.opl3 found");
    CHECK(crumb_skiplist_has("cpu.CYRIX")  == 0, "skiplist: case-sensitive");
    CHECK(crumb_skiplist_has("")           == 0, "skiplist: empty string misses");
    crumb_skiplist_add(NULL);  /* should be no-op */
    crumb_skiplist_add("");    /* should be no-op */

    /* Path probing — CWD should be writable in tests/host */
    crumb_init();
    CHECK(crumb_disabled == 0, "crumb_init: CWD writable, not disabled");
    CHECK(crumb_path[0] != '\0', "crumb_init: path populated");

    /* enter → file appears; exit → file gone */
    crumb_enter("cpu.probe.pushfd");
    CHECK(file_exists(crumb_path) == 1, "crumb_enter: file exists");

    /* Read back and verify content */
    {
        FILE *f = fopen(crumb_path, "rt");
        char line[64];
        CHECK(f != NULL, "crumb file can be reopened");
        if (f) {
            fgets(line, sizeof(line), f);
            fclose(f);
            CHECK(strncmp(line, "cpu.probe.pushfd", 16) == 0,
                  "crumb file contains test name");
        }
    }

    crumb_exit();
    CHECK(file_exists(crumb_path) == 0, "crumb_exit: file removed");

    /* crumb_check_previous: write a stale breadcrumb, then verify it's
     * detected and cleaned up */
    {
        FILE *f = fopen(crumb_path, "wt");
        fprintf(f, "memory.moving-inversion\n");
        fclose(f);
    }
    CHECK(file_exists(crumb_path) == 1, "pre-check: stale crumb present");
    printf("  (expected output from crumb_check_previous follows)\n");
    crumb_check_previous();
    CHECK(file_exists(crumb_path) == 0, "crumb_check_previous: clears stale file");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
