#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cerberus.h"
#include "core/display.h"
#include "core/report.h"
#include "core/crumb.h"
#include "core/ui.h"
#include "core/intro.h"
#include "core/journey.h"
#include "core/consist.h"
#include "core/thermal.h"
#include "core/timing.h"
#include "detect/detect.h"
#include "detect/unknown.h"
#include "diag/diag.h"
#include "bench/bench.h"
#include "upload/upload.h"

static void print_help(void)
{
    puts("CERBERUS " CERBERUS_VERSION " - Retro PC System Intelligence");
    puts("");
    puts("Usage: CERBERUS [/Q] [/C[:n]] [/ONLY:<h>] [/SKIP:<h>] [/O:file]");
    puts("                [/U] [/NOCYRIX] [/NOINTRO] [/QUICK] [/NOUI] [/?]");
    puts("  /Q              Quick mode (default)");
    puts("  /C[:n]          Calibrated mode, n runs (default 7)");
    puts("  /ONLY:DET|DIAG|BENCH   Run only that head");
    puts("  /SKIP:DET|DIAG|BENCH   Skip that head");
    puts("  /SKIP:TIMING    Skip PIT/BIOS timing self-check (rule 4a)");
    puts("  /O:file         Output INI (default CERBERUS.INI)");
    puts("  /U              Upload via NetISA (non-zero exit if disabled)");
    puts("  /NOCYRIX        Skip Cyrix DIR probe (port 22h safety)");
    puts("  /NOINTRO        Skip VGA splash");
    puts("  /NOUI           Skip summary + consistency UI render (INI still written)");
    puts("  /QUICK          Skip visual demonstrations (title cards + visuals); run tests + summary");
    puts("  /?              This help");
}

static int str_starts_with(const char *s, const char *p)
{
    while (*p)
        if (*s++ != *p++) return 0;
    return 1;
}

static int parse_args(int argc, char *argv[], opts_t *o)
{
    int i;
    o->mode = MODE_QUICK;
    o->runs = 1;
    o->do_detect = 1;
    o->do_diagnose = 1;
    o->do_benchmark = 1;
    o->do_upload = 0;
    o->no_cyrix = 0;
    o->no_intro = 0;
    o->no_ui    = 0;
    o->do_quick = 0;
    strcpy(o->out_path, "CERBERUS.INI");

    for (i = 1; i < argc; i++) {
        char *a = argv[i];
        if (!strcmp(a, "/?")) { print_help(); return 0; }
        else if (!strcmp(a, "/Q")) { o->mode = MODE_QUICK; o->runs = 1; }
        else if (str_starts_with(a, "/C")) {
            int n;
            o->mode = MODE_CALIBRATED;
            n = (a[2] == ':' && a[3]) ? atoi(a + 3) : 7;
            if (n < 3) n = 3;
            if (n > 255) n = 255;
            o->runs = (unsigned char)n;
        }
        else if (str_starts_with(a, "/ONLY:")) {
            o->do_detect = 0;
            o->do_diagnose = 0;
            o->do_benchmark = 0;
            if      (!strcmp(a + 6, "DET"))   o->do_detect = 1;
            else if (!strcmp(a + 6, "DIAG"))  o->do_diagnose = 1;
            else if (!strcmp(a + 6, "BENCH")) o->do_benchmark = 1;
            else { fprintf(stderr, "bad /ONLY target: %s\n", a + 6); return -1; }
        }
        else if (str_starts_with(a, "/SKIP:")) {
            if      (!strcmp(a + 6, "DET"))   o->do_detect = 0;
            else if (!strcmp(a + 6, "DIAG"))  o->do_diagnose = 0;
            else if (!strcmp(a + 6, "BENCH")) o->do_benchmark = 0;
            else crumb_skiplist_add(a + 6);
        }
        else if (str_starts_with(a, "/O:")) {
            strncpy(o->out_path, a + 3, sizeof(o->out_path) - 1);
            o->out_path[sizeof(o->out_path) - 1] = 0;
        }
        else if (!strcmp(a, "/U"))       { o->do_upload = 1; }
        else if (!strcmp(a, "/NOCYRIX")) { o->no_cyrix = 1; }
        else if (!strcmp(a, "/NOINTRO")) { o->no_intro = 1; }
        else if (!strcmp(a, "/NOUI"))    { o->no_ui = 1; }
        else if (!strcmp(a, "/QUICK"))   { o->do_quick = 1; }
        else {
            fprintf(stderr, "unknown option: %s\n", a);
            return -1;
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    opts_t opts;
    result_table_t table;
    int parse_rc;
    int exit_code = EXIT_OK;
    int upload_rc;

    memset(&table, 0, sizeof(table));

    parse_rc = parse_args(argc, argv, &opts);
    if (parse_rc == 0)  return EXIT_OK;
    if (parse_rc == -1) return EXIT_USAGE;

    crumb_init();
    unknown_init();
    display_init();
    journey_init();
    crumb_check_previous();

    /* Boot splash runs before the text banner. Skipped under /NOUI,
     * /NOINTRO. Returns with screen cleared and cursor at (0,0) so
     * display_banner starts on a fresh frame. */
    intro_splash(&opts);

    display_banner();

    /* PIT Channel 2 sanity probe — flags the emulator-or-broken hint
     * that detect_env consumes as its fallback when signature scans
     * return ambiguous. Quick (<1ms) + safe to call before anything
     * else that touches C2.
     *
     * Gated by /SKIP:TIMING as a ship-hatch for motherboards where
     * touching PIT C2 hangs the box (a real failure mode on some
     * 386/486 boards with non-standard 8254-clone chips). The crumb
     * enter/exit pair means a hang during the probe leaves a
     * "timing.init" breadcrumb, so the next reboot of CERBERUS will
     * print the NOTICE telling the user to pass /SKIP:TIMING. */
    if (!crumb_skiplist_has("TIMING")) {
        crumb_enter("timing.init");
        timing_init();
        crumb_exit();
    }

    if (opts.do_detect)    detect_all(&table, &opts);
    if (opts.do_diagnose)  diag_all(&table, &opts);
    if (opts.do_benchmark) bench_all(&table, &opts);

    /* Timing self-check runs once per invocation, independent of which
     * heads ran. Writes the two cross-check values consist_check's rule
     * 4a reads. Skipped if /SKIP:TIMING was passed (ship-hatch for
     * buggy real-hardware scenarios). Wrapped in crumb enter/exit so
     * a mid-measurement hang is diagnosable — same rationale as
     * timing_init above. */
    if (!crumb_skiplist_has("TIMING")) {
        crumb_enter("timing.self_check");
        timing_self_check(&table);
        crumb_exit();
    }

    /* Consistency cross-check runs after all three heads so it sees
     * everything that got populated. Thermal follows, consuming the
     * per-pass bench series that calibrated mode produced. */
    consist_check(&table);
    thermal_check(&table);

    report_write_ini(&table, &opts, opts.out_path);

    /* Finalize the unknown-hardware dump BEFORE any UI rendering so the
     * CERBERUS.UNK file lands on disk even if the UI path hangs (observed
     * on the 486 DX-2 bench box: after the consistency alert boxes
     * rendered the program hung before returning to DOS, requiring
     * CTRL-ALT-DEL). INI was already written by report_write_ini above;
     * we just want the UNK preserved for the same resilience. */
    unknown_finalize();

    /* UI dispatch:
     *   /NOUI  → batch text to stdout, no interactive scroll
     *   normal → interactive scrollable three-heads summary */
    if (opts.no_ui) {
        ui_render_batch(&table);
    } else {
        ui_render_summary(&table, &opts);
    }

    if (opts.do_upload) {
        upload_rc = upload_ini(opts.out_path);
        if (upload_rc != UPLOAD_OK) exit_code = EXIT_UPLOAD_FAIL;
    }

    /* Flush stdio so any buffered UI output makes it to the console
     * before the exit-cleanup stage starts. Cheap insurance for the
     * real-iron UI hang. */
    fflush(stdout);
    fflush(stderr);

    display_shutdown();
    return exit_code;
}
