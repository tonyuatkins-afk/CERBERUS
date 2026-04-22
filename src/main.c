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
#include "core/audio_scale.h"
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
    puts("                [/U] [/NOCYRIX] [/NOINTRO] [/QUICK] [/NOUI]");
    puts("                [/NOUPLOAD] [/UPLOAD] [/NICK:<name>] [/NOTE:<text>] [/CSV] [/?]");
    puts("  /Q              Quick mode (default)");
    puts("  /C[:n]          Calibrated mode, n runs (default 7)");
    puts("  /ONLY:DET|DIAG|BENCH   Run only that head");
    puts("  /SKIP:DET|DIAG|BENCH   Skip that head");
    puts("  /SKIP:TIMING    Skip PIT/BIOS timing self-check (rule 4a)");
    puts("  /O:file         Output INI (default CERBERUS.INI)");
    puts("  /NOCYRIX        Skip Cyrix DIR probe (port 22h safety)");
    puts("  /NOINTRO        Skip VGA splash");
    puts("  /NOUI           Skip summary + consistency UI render (INI still written)");
    puts("  /QUICK          Skip visual demonstrations (title cards + visuals); run tests + summary");
    puts("  /MONO           Force monochrome rendering regardless of adapter");
    puts("  /CSV            Also write sibling <out>.CSV (RFC 4180 minimal quoting)");
#ifdef CERBERUS_UPLOAD_ENABLED
    puts("  /U, /UPLOAD     Upload without prompting (auto-yes)");
    puts("  /NOUPLOAD       Never prompt to upload, never upload");
#else
    puts("  /U, /UPLOAD     Upload: not built in this release. Rebuild with 'wmake UPLOAD=1'.");
    puts("  /NOUPLOAD       Accepted as no-op for script compat; upload not built.");
#endif
    puts("  /NICK:<name>    Nickname for INI annotation (alnum+space+hyphen, max 32)");
    puts("  /NOTE:<text>    Note for INI annotation (max 128 chars)");
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
    o->no_upload = 0;
    o->force_mono = 0;
    o->do_csv = 0;
    strcpy(o->out_path, "CERBERUS.INI");
    o->nickname[0] = '\0';
    o->note[0]     = '\0';

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
        else if (!strcmp(a, "/U"))       {
#ifdef CERBERUS_UPLOAD_ENABLED
            o->do_upload = 1;
#else
            fprintf(stderr, "upload not built into this binary; rebuild with 'wmake UPLOAD=1'\n");
            return -1;
#endif
        }
        else if (!strcmp(a, "/NOCYRIX")) { o->no_cyrix = 1; }
        else if (!strcmp(a, "/NOINTRO")) { o->no_intro = 1; }
        else if (!strcmp(a, "/NOUI"))    { o->no_ui = 1; }
        else if (!strcmp(a, "/QUICK"))   { o->do_quick = 1; }
        else if (!strcmp(a, "/MONO"))    { o->force_mono = 1; }
        else if (!strcmp(a, "/CSV"))     { o->do_csv = 1; }
        else if (!strcmp(a, "/NOUPLOAD")) { o->no_upload = 1; }
        else if (!strcmp(a, "/UPLOAD"))  {
#ifdef CERBERUS_UPLOAD_ENABLED
            o->do_upload = 1;
#else
            fprintf(stderr, "upload not built into this binary; rebuild with 'wmake UPLOAD=1'\n");
            return -1;
#endif
        }
        else if (str_starts_with(a, "/NICK:")) {
            /* Nickname: max 32 chars, sanitize to alnum + space + hyphen. */
            const char *src = a + 6;
            int dst = 0;
            while (*src && dst < (int)sizeof(o->nickname) - 1) {
                char c = *src++;
                if ((c >= '0' && c <= '9') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= 'a' && c <= 'z') ||
                    c == ' ' || c == '-' || c == '_') {
                    o->nickname[dst++] = c;
                }
                /* silently drop other chars */
            }
            o->nickname[dst] = '\0';
        }
        else if (str_starts_with(a, "/NOTE:")) {
            /* Note: max 128 chars, strip control chars. Quotes on the
             * command line are shell-eaten; Watcom gives us the content
             * without quotes. */
            const char *src = a + 6;
            int dst = 0;
            while (*src && dst < (int)sizeof(o->note) - 1) {
                unsigned char c = (unsigned char)*src++;
                if (c >= 0x20 && c < 0x7F) {
                    o->note[dst++] = (char)c;
                }
            }
            o->note[dst] = '\0';
        }
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
    /* M3.5: /MONO must be applied BEFORE display_init() so adapter
     * detection sees the force flag and display_enable_16bg_colors()
     * decides not to bump the palette. */
    display_set_force_mono(opts.force_mono);
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
        /* v0.7.1: emit timing.method and (if RDTSC) timing.cpu_mhz so
         * downstream tooling knows which clock produced the numbers.
         * Runs the TSC calibration via ~220ms BIOS-tick window; cached
         * internally, so subsequent callers incur no extra cost. */
        crumb_enter("timing.method_info");
        timing_emit_method_info(&table);
        crumb_exit();
        /* v0.6.0 T6: PIT metronome visual. */
        timing_metronome_visual(&opts);
    }

    /* v0.6.0 T7: Audio Hardware scale visual — last journey beat
     * before the summary screen. PC speaker only in v0.6.0; OPL2 +
     * SB16 PCM paths are v0.6.1 follow-up. */
    audio_scale_visual(&opts);

    /* Consistency cross-check runs after all three heads so it sees
     * everything that got populated. Thermal follows, consuming the
     * per-pass bench series that calibrated mode produced. */
    consist_check(&table);
    thermal_check(&table);

    /* ==================================================================
     * END-OF-RUN CRUMB CHAIN: LOAD-BEARING. DO NOT REMOVE.
     *
     * Before v0.7.2 the end-of-run path (upload_populate / report_write /
     * upload_execute / unknown_finalize / ui_* / flush / display_shutdown
     * / return) lacked crumb instrumentation and the program triggered
     * EMM386 error #05 "unrecoverable privileged operation" at program
     * exit on BEK-V409 (Intel i486DX-2-66, AMI BIOS 11/11/92, running
     * DOS 6.22 under HIMEM + EMM386). The crash was not reproducible in
     * DOSBox-X.
     *
     * Adding the crumb_enter / crumb_exit pairs below coincidentally
     * eliminated the crash. Each pair executes at minimum:
     *
     *   crumb_enter: INT 21h AH=3Dh (open), AH=40h (write), AH=68h
     *                (_dos_commit — forces FS buffers/FAT/dirent flush),
     *                AH=3Eh (close)
     *   crumb_exit:  INT 21h AH=41h (unlink)
     *
     * Working hypothesis: one or more of these calls (most likely
     * _dos_commit, which is DOS 3.3+ explicit buffer flush via INT 21h
     * AH=68h) resets V86-monitor state or flushes a pending real-mode
     * ↔ V86 transition that the end-of-run path otherwise accumulates
     * beyond EMM386's tolerance. The precise triggering instruction has
     * not been isolated, and so the pairs stay as ordered.
     *
     * Rule: any refactor of the end-of-run path MUST preserve at least
     * one crumb_enter / crumb_exit pair between each of the following
     * call sites until 0.8.0 M1.7 closes the root-cause investigation:
     *
     *   upload_populate_metadata
     *   report_write_ini
     *   upload_execute
     *   unknown_finalize
     *   ui_render_batch OR ui_render_summary
     *   fflush pair
     *   display_shutdown
     *   return
     *
     * Removing any single pair requires a fresh BEK-V409 capture proving
     * the EMM386 #05 crash does not return with that pair absent, run
     * from a cold boot and in the standard DOS 6.22 + HIMEM + EMM386
     * configuration. See 0.8.0 plan section 6 M1.7 and docs/methodology
     * for the removal-at-a-time investigation protocol.
     * ================================================================== */

    /* v0.7.0: populate [upload] metadata from /NICK /NOTE before initial
     * INI write so nickname + notes land in the file on first pass. */
    crumb_enter("main.upload_populate");
    upload_populate_metadata(&table, &opts);
    crumb_exit();

    crumb_enter("main.report_write");
    report_write_ini(&table, &opts, opts.out_path);
    crumb_exit();

    /* v0.8.1 M1.2: /CSV flag writes a sibling CSV file. Path is the INI
     * path with the extension replaced by .CSV (or appended if absent).
     * The INI remains the authoritative submission format; CSV is a
     * convenience for spreadsheet + automated analysis users. */
    if (opts.do_csv) {
        char csv_path[64];
        int dot = -1;
        int i_n;
        strncpy(csv_path, opts.out_path, sizeof(csv_path) - 1);
        csv_path[sizeof(csv_path) - 1] = '\0';
        for (i_n = 0; csv_path[i_n]; i_n++) {
            if (csv_path[i_n] == '.') dot = i_n;
            if (csv_path[i_n] == '\\' || csv_path[i_n] == '/') dot = -1;
        }
        if (dot >= 0 && dot < (int)sizeof(csv_path) - 4) {
            strcpy(csv_path + dot, ".CSV");
        } else if ((int)strlen(csv_path) < (int)sizeof(csv_path) - 4) {
            strcat(csv_path, ".CSV");
        }
        crumb_enter("main.report_write_csv");
        report_write_csv(&table, csv_path);
        crumb_exit();
    }

    /* v0.7.0: attempt upload after INI is on disk. upload_execute
     * decides internally based on network transport + /NOUPLOAD /UPLOAD
     * + user prompt. Updates [upload] status / submission_id / url in
     * the table and re-writes the INI on success. Never crashes on
     * failure; graceful degrade to "results saved locally." */
    crumb_enter("main.upload_execute");
    upload_rc = upload_execute(&table, &opts, opts.out_path);
    crumb_exit();
    /* upload_rc is not fatal — UPLOAD_OFFLINE / _SKIPPED / _NETWORK
     * are all expected outcomes on various host configurations. */
    (void)upload_rc;

    /* Finalize the unknown-hardware dump BEFORE any UI rendering so the
     * CERBERUS.UNK file lands on disk even if the UI path hangs (observed
     * on the 486 DX-2 bench box: after the consistency alert boxes
     * rendered the program hung before returning to DOS, requiring
     * CTRL-ALT-DEL). INI was already written by report_write_ini above;
     * we just want the UNK preserved for the same resilience. */
    crumb_enter("main.unknown_finalize");
    unknown_finalize();
    crumb_exit();

    /* UI dispatch:
     *   /NOUI  → batch text to stdout, no interactive scroll
     *   normal → interactive scrollable three-heads summary */
    if (opts.no_ui) {
        crumb_enter("main.ui_batch");
        ui_render_batch(&table);
        crumb_exit();
    } else {
        crumb_enter("main.ui_summary");
        ui_render_summary(&table, &opts);
        crumb_exit();
    }

    /* v0.7.0: the legacy /U → upload_ini path has been replaced by
     * upload_execute() which runs earlier (before the UI summary) so
     * the upload status can appear in the scrollable summary's
     * UPLOAD STATUS row. Kept this block empty as a landmark; remove
     * in a v0.8.0 cleanup. */

    /* Flush stdio so any buffered UI output makes it to the console
     * before the exit-cleanup stage starts. Cheap insurance for the
     * real-iron UI hang. */
    crumb_enter("main.flush");
    fflush(stdout);
    fflush(stderr);
    crumb_exit();

    crumb_enter("main.display_shutdown");
    display_shutdown();
    crumb_exit();

    /* 0.8.0 M1 fix: bypass Watcom libc exit cleanup via _exit().
     *
     * Real-iron capture on BEK-V409 (2026-04-21, CERBERUS.LAS retained
     * 'main.return' after a Q-exit hang) confirmed the v0.7.1 class of
     * end-of-run hang lives in Watcom's runtime teardown: atexit chain,
     * FPU state teardown, libc stdio close. All resources this program
     * owns are already explicitly released by the time we reach here:
     * stdio was fflush'd ('main.flush' crumb), video mode restored
     * ('main.display_shutdown'), CAPTURE.INI and CERBERUS.UNK closed by
     * their respective fopen/fclose pairs inside report_write_ini and
     * unknown_finalize. CERBERUS registers no atexit handlers. The
     * libc cleanup step that Watcom would run adds zero value and
     * empirically hangs under DOS 6.22 + HIMEM + EMM386 + AMI BIOS on
     * the 486 DX-2-66 bench box.
     *
     * _exit() in Watcom's stdlib calls INT 21h AH=4Ch directly — no
     * libc involvement, no FPU cleanup, no atexit. Clean return to
     * COMMAND.COM with the specified exit code.
     *
     * The crumb pair below unlinks CERBERUS.LAS on successful _exit so
     * the next run's crumb_check_previous sees a clean state and does
     * NOT emit a spurious "previous run hung during main.return" notice.
     * If _exit itself ever hangs (structurally implausible since it is
     * a bare INT 21h AH=4Ch), LAS stays with 'main.return' and the
     * next boot's notice is accurate. */
    crumb_enter("main.return");
    crumb_exit();
    _exit((int)exit_code);
    return exit_code;  /* unreachable — silences compiler */
}
