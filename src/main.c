#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cerberus.h"
#include "core/display.h"
#include "core/report.h"
#include "core/crumb.h"
#include "core/ui.h"
#include "core/consist.h"
#include "core/thermal.h"
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
    puts("                [/U] [/NOCYRIX] [/NOINTRO] [/?]");
    puts("  /Q              Quick mode (default)");
    puts("  /C[:n]          Calibrated mode, n runs (default 7)");
    puts("  /ONLY:DET|DIAG|BENCH   Run only that head");
    puts("  /SKIP:DET|DIAG|BENCH   Skip that head");
    puts("  /O:file         Output INI (default CERBERUS.INI)");
    puts("  /U              Upload via NetISA (non-zero exit if disabled)");
    puts("  /NOCYRIX        Skip Cyrix DIR probe (port 22h safety)");
    puts("  /NOINTRO        Skip VGA splash");
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
    crumb_check_previous();
    display_banner();

    if (opts.do_detect)    detect_all(&table, &opts);
    if (opts.do_diagnose)  diag_all(&table, &opts);
    if (opts.do_benchmark) bench_all(&table, &opts);

    /* Consistency cross-check runs after all three heads so it sees
     * everything that got populated. Thermal follows, consuming the
     * per-pass bench series that calibrated mode produced. */
    consist_check(&table);
    thermal_check(&table);

    report_write_ini(&table, &opts, opts.out_path);
    ui_render_summary(&table, &opts);
    ui_render_consistency_alerts(&table);

    if (opts.do_upload) {
        upload_rc = upload_ini(opts.out_path);
        if (upload_rc != UPLOAD_OK) exit_code = EXIT_UPLOAD_FAIL;
    }

    unknown_finalize();
    display_shutdown();
    return exit_code;
}
