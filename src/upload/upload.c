/*
 * CERBERUS INI upload client.
 *
 * v0.8.0 — RUNTIME UPLOAD COMPILED OUT OF STOCK BUILDS.
 *
 * Per the 0.8.0 plan (§8 Upload decision), stock binaries contain no
 * HTTP client code path, no HTGET shell-out, no transmission logic.
 * The entire upload_execute() inner implementation plus all HTGET
 * helpers are gated on CERBERUS_UPLOAD_ENABLED (set via
 * `wmake UPLOAD=1`). Stock upload_execute is a stub that emits
 * upload.status=not_built and returns UPLOAD_DISABLED.
 *
 * Why: the upload path had an unfixed failure mode (stack overflow
 * when the endpoint is unreachable), and barelybooting.com is not
 * yet deployed, so the default condition is "endpoint unreachable."
 * Shipping a default-flag run one argv away from a crash on a
 * vintage 486 is the worst-possible first impression for a trust-
 * first release. Compiling the code out eliminates the crash as a
 * code-presence fact, not a configuration fact.
 *
 * What stays in stock builds:
 *   - upload_populate_metadata(): copies /NICK and /NOTE into the INI
 *     locally. The local annotation has value independent of upload.
 *   - src/detect/network.c: transport detection. Useful on its own.
 *   - [upload] section in INI, populated with status=not_built.
 *   - docs/ini-upload-contract.md: forward-looking design.
 *
 * What is compiled out in stock builds:
 *   - HTGET shell-out, UPLOAD.TMP handling, HTTP response parsing.
 *   - upload_execute()'s full body including the network-reachable
 *     failure modes (including the unfixed stack overflow).
 *   - /UPLOAD auto-yes wiring (flag is rejected at parse-time in main.c).
 *
 * Research builds (`wmake UPLOAD=1`, defines CERBERUS_UPLOAD_ENABLED):
 *
 * Flow:
 *   1. upload_populate_metadata() copies opts.nickname / opts.note into
 *      upload.nickname / upload.notes rows so they land in the INI on
 *      the initial report_write_ini pass.
 *   2. upload_execute() runs AFTER that initial INI write:
 *        a. Read network.transport. If "none": emit upload.status=offline
 *           and return UPLOAD_OFFLINE.
 *        b. If opts.no_upload: emit status=skipped, return UPLOAD_SKIPPED.
 *        c. If not opts.do_upload (no auto-yes flag): prompt user.
 *           Default Y; N or Esc = skipped.
 *        d. Invoke the upload transport (HTGET shell-out today).
 *        e. Parse response. Emit status + submission_id + url.
 *        f. Re-write INI with the updated [upload] block.
 *
 * Transport selection: HTGET shell-out only. Raw TCP over packet
 * driver is a multi-week project (TCP state machine, retransmit,
 * windowing) and stays filed for v0.9.0+.
 *
 * HTGET command line is encapsulated in a single #define below so the
 * exact flag syntax can be updated after on-hardware testing without
 * touching the upload flow itself.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>   /* _nmalloc for issue #9 narrow fix */
#include <dos.h>
#include <conio.h>
#include "upload.h"
#include "../core/report.h"
#include "../core/tui_util.h"
#include "../detect/network.h"

/* Target endpoint. Part B (the server) binds to this URL. Until Part B
 * is deployed the POST will fail with "connection refused" — that's
 * UPLOAD_NETWORK, not a crash. Clean fallback to "results saved
 * locally." */
#define UPLOAD_URL       "http://barelybooting.com/api/v1/submit"

/* HTGET command template. %s = INI path. The -P flag requests POST
 * with the named file as body. The -m flag sets the MIME type.
 * Response lands in UPLOAD.TMP for us to parse.
 *
 * Exact HTGET flag syntax verified on-hardware during v0.7.0
 * validation. If your HTGET version uses different flags, update this
 * line; upload_execute() is otherwise transport-agnostic. */
#define HTGET_CMD_FMT    "HTGET -P %s -m text/plain " UPLOAD_URL " > UPLOAD.TMP"

#define UPLOAD_TMP_PATH  "UPLOAD.TMP"
#define SUBMISSION_ID_LEN 8

/* ----------------------------------------------------------------------- */
/* Static storage (kept file-scope so report_add_str's stored pointers     */
/* stay valid for the lifetime of the run).                                 */
/*                                                                          */
/* upload_status_buf is always compiled (stock builds still emit           */
/* status=not_built via set_status). upload_submission_id_buf and          */
/* upload_url_buf are only populated on successful POST, so they're gated  */
/* to research builds.                                                      */
/*                                                                          */
/* upload_nickname_ptr and upload_notes_ptr (issue #9 narrow fix): the     */
/* old BSS arrays upload_nickname_buf[40] and upload_notes_buf[136] were  */
/* observed on real iron (v0.7.1 on BEK-V409) to have their trailing      */
/* bytes progressively overwritten during emit_section by tails of other  */
/* string values, producing output like "nickname=e inconclusive" where   */
/* the captured user value was "BEK-V409". Root cause of the overwriter   */
/* is not yet identified. Narrow fix: _nmalloc the buffers into the near  */
/* heap at first-call time, moving them out of BSS and away from whatever */
/* static-data neighbor is stomping them. Buffers live until program exit */
/* (DOS reclaims). If the leak recurs on post-0.8.0 captures, escalate to */
/* the architectural fix (report_add_str ownership refactor). See 0.8.0   */
/* plan §6 M1.3 and the v0.7.x session reports.                           */
/* ----------------------------------------------------------------------- */

#define UPLOAD_NICKNAME_CAP 40
#define UPLOAD_NOTES_CAP    136

static char upload_status_buf[16];
static char *upload_nickname_ptr;   /* _nmalloc'd on first call */
static char *upload_notes_ptr;      /* _nmalloc'd on first call */
#ifdef CERBERUS_UPLOAD_ENABLED
static char upload_submission_id_buf[SUBMISSION_ID_LEN + 1];
static char upload_url_buf[96];
#endif

/* Fallback statics for the _nmalloc-returns-NULL path. Kept as a belt-
 * and-braces guarantee the INI always gets a valid pointer even if the
 * near heap is exhausted (very unlikely in practice, but CERBERUS runs
 * on 256 KB 8088 machines where DGROUP + free heap can be tight). */
static char upload_nickname_fallback[UPLOAD_NICKNAME_CAP];
static char upload_notes_fallback[UPLOAD_NOTES_CAP];

/* ----------------------------------------------------------------------- */
/* Metadata population                                                      */
/* ----------------------------------------------------------------------- */

/* Forward declaration — defined below. upload_populate_metadata seeds
 * upload.status in the first INI pass so offline/skipped/not_built
 * rows always appear on disk. */
static void set_status(result_table_t *t, const char *status);

void upload_populate_metadata(result_table_t *t, const opts_t *o)
{
    /* Allocate nickname + notes buffers out of BSS on first call.
     * See issue-#9 note above the storage declarations. _nmalloc
     * returns near-heap memory separate from the static BSS pool,
     * isolating these strings from whatever BSS-neighbor overwrite
     * was corrupting the v0.7.1 captures. Fallback to file-scope
     * statics if _nmalloc fails (near-heap exhausted). */
    if (!upload_nickname_ptr) {
        upload_nickname_ptr = (char *)_nmalloc(UPLOAD_NICKNAME_CAP);
        if (!upload_nickname_ptr) upload_nickname_ptr = upload_nickname_fallback;
    }
    if (!upload_notes_ptr) {
        upload_notes_ptr = (char *)_nmalloc(UPLOAD_NOTES_CAP);
        if (!upload_notes_ptr) upload_notes_ptr = upload_notes_fallback;
    }

    /* /NICK: populate upload.nickname. Always emit the row (empty if
     * no /NICK) so the server parser sees a stable schema. */
    if (o && o->nickname[0]) {
        strncpy(upload_nickname_ptr, o->nickname, UPLOAD_NICKNAME_CAP - 1);
        upload_nickname_ptr[UPLOAD_NICKNAME_CAP - 1] = '\0';
    } else {
        upload_nickname_ptr[0] = '\0';
    }
    report_add_str(t, "upload.nickname", upload_nickname_ptr,
                   CONF_HIGH, VERDICT_UNKNOWN);

    if (o && o->note[0]) {
        strncpy(upload_notes_ptr, o->note, UPLOAD_NOTES_CAP - 1);
        upload_notes_ptr[UPLOAD_NOTES_CAP - 1] = '\0';
    } else {
        upload_notes_ptr[0] = '\0';
    }
    report_add_str(t, "upload.notes", upload_notes_ptr,
                   CONF_HIGH, VERDICT_UNKNOWN);

    /* Seed the upload.status row in the first INI pass.
     *
     * Stock builds: set_status("not_built") here is the final value;
     * the stub upload_execute later redundantly writes the same value,
     * but the row is guaranteed to appear in the INI on disk.
     *
     * Research builds: set_status here seeds an initial "not_built"
     * that upload_execute overwrites in place (via report_update_str)
     * to offline / skipped / uploaded / etc. during the post-INI pass.
     *
     * This fixes a pre-existing v0.7.x issue where upload paths that
     * did not reach their second report_write_ini (offline, skipped,
     * no_client) left the INI on disk without any upload.status row. */
    set_status(t, "not_built");
}

/* ----------------------------------------------------------------------- */
/* Status emit helpers                                                      */
/* ----------------------------------------------------------------------- */

static void set_status(result_table_t *t, const char *status)
{
    strncpy(upload_status_buf, status, sizeof(upload_status_buf) - 1);
    upload_status_buf[sizeof(upload_status_buf) - 1] = '\0';
    /* v0.7.0-rc2: update_str instead of add_str. upload_execute() runs
     * AFTER the first report_write_ini pass, so the [upload] section
     * already has nickname/notes rows in the table — plain add_str here
     * would have created duplicate upload.status rows across the two
     * report_write_ini calls. update_str writes in place. */
    report_update_str(t, "upload.status", upload_status_buf,
                      CONF_HIGH, VERDICT_UNKNOWN);
}

#ifdef CERBERUS_UPLOAD_ENABLED
static void set_submission(result_table_t *t,
                           const char *id, const char *url)
{
    strncpy(upload_submission_id_buf, id,
            sizeof(upload_submission_id_buf) - 1);
    upload_submission_id_buf[sizeof(upload_submission_id_buf) - 1] = '\0';
    report_update_str(t, "upload.submission_id", upload_submission_id_buf,
                      CONF_HIGH, VERDICT_UNKNOWN);

    strncpy(upload_url_buf, url, sizeof(upload_url_buf) - 1);
    upload_url_buf[sizeof(upload_url_buf) - 1] = '\0';
    report_update_str(t, "upload.url", upload_url_buf,
                      CONF_HIGH, VERDICT_UNKNOWN);
}
#endif /* CERBERUS_UPLOAD_ENABLED */

/* ----------------------------------------------------------------------- */
/* HTGET invocation + response parse                                        */
/*                                                                          */
/* Everything below this point is gated on CERBERUS_UPLOAD_ENABLED. Stock  */
/* builds provide a stub upload_execute() in the #else branch at the end.  */
/* ----------------------------------------------------------------------- */

#ifdef CERBERUS_UPLOAD_ENABLED

/* Best-effort "is HTGET on PATH?" check. DOS has no `which`; we
 * attempt a dry-run via system() and rely on nonzero exit to mean
 * absent. This WILL also return absent if HTGET runs but fails at a
 * non-POST URL — false-negatives err safe (we'll just emit
 * UPLOAD_NO_TSR with a helpful message). */
static int htget_available(void)
{
    /* Watcom's system() returns 0 on success, nonzero otherwise. The
     * "--help" output prints to stdout but exits 0 on any real
     * HTGET. If HTGET isn't in PATH, DOS returns the "Bad command"
     * status which system() surfaces as nonzero. */
    int rc = system("HTGET /? > NUL 2> NUL");
    return (rc == 0);
}

/* Shell out to HTGET and parse the response file. Returns UPLOAD_OK
 * on a 2-line response we could parse; UPLOAD_NETWORK / UPLOAD_SERVER
 * on any failure. On UPLOAD_OK, *out_id and *out_url are populated. */
static int htget_post(const char *ini_path,
                     char *out_id, unsigned int id_cap,
                     char *out_url, unsigned int url_cap)
{
    char cmd[192];
    int rc;
    FILE *f;
    char line1[32];
    char line2[96];

    sprintf(cmd, HTGET_CMD_FMT, ini_path);
    rc = system(cmd);
    if (rc != 0) {
        /* HTGET exited non-zero — typically connection refused or
         * DNS failure. */
        remove(UPLOAD_TMP_PATH);
        return UPLOAD_NETWORK;
    }

    f = fopen(UPLOAD_TMP_PATH, "r");
    if (!f) {
        /* v0.7.0-rc2: HTGET may have succeeded-and-created UPLOAD.TMP
         * but fopen then fails (I/O error, permission denial, or a
         * TSR interfering). Every OTHER failure path below calls
         * remove(); this one historically missed, leaving a stale
         * file that the NEXT run's htget_post would mis-read. Clean up. */
        remove(UPLOAD_TMP_PATH);
        return UPLOAD_NETWORK;
    }

    line1[0] = '\0';
    line2[0] = '\0';
    if (fgets(line1, sizeof(line1), f) == (char *)0) {
        fclose(f);
        remove(UPLOAD_TMP_PATH);
        return UPLOAD_SERVER;
    }
    if (fgets(line2, sizeof(line2), f) == (char *)0) {
        fclose(f);
        remove(UPLOAD_TMP_PATH);
        return UPLOAD_SERVER;
    }
    fclose(f);
    remove(UPLOAD_TMP_PATH);

    /* Strip trailing newline from both lines */
    {
        int n = (int)strlen(line1);
        while (n > 0 && (line1[n - 1] == '\n' || line1[n - 1] == '\r')) {
            line1[--n] = '\0';
        }
        n = (int)strlen(line2);
        while (n > 0 && (line2[n - 1] == '\n' || line2[n - 1] == '\r')) {
            line2[--n] = '\0';
        }
    }

    if (strlen(line1) == 0 || strlen(line2) == 0) return UPLOAD_SERVER;

    strncpy(out_id,  line1, id_cap - 1);
    out_id[id_cap - 1] = '\0';
    strncpy(out_url, line2, url_cap - 1);
    out_url[url_cap - 1] = '\0';
    return UPLOAD_OK;
}

/* ----------------------------------------------------------------------- */
/* User prompt                                                              */
/* ----------------------------------------------------------------------- */

static int prompt_user(const char *transport)
{
    unsigned char a, s;
    printf("\nNetwork detected (%s).\n", transport);
    printf("Upload results to barelybooting.com? (Y/n) ");
    fflush(stdout);
    for (;;) {
        tui_read_key(&a, &s);
        if (a == 'y' || a == 'Y' || a == '\r' || a == '\n') {
            printf("Y\n");
            return 1;
        }
        if (a == 'n' || a == 'N' || a == 0x1B) {
            printf("n\n");
            return 0;
        }
        /* ignore other keys */
    }
}

/* ----------------------------------------------------------------------- */
/* Public entry                                                             */
/* ----------------------------------------------------------------------- */

int upload_execute(result_table_t *t, const opts_t *o, const char *ini_path)
{
    const char *transport;
    int rc;

    if (!t || !o) return UPLOAD_DISABLED;

    transport = network_transport_str(t);

    /* Offline → status=offline, no prompt, no POST. */
    if (strcmp(transport, "none") == 0) {
        set_status(t, "offline");
        /* Flush the updated status to disk so the INI reflects reality.
         * Pre-M1 this path left the INI with the upload_populate_metadata
         * seed (empty or not_built) rather than the actual outcome.
         * Cost: one extra INI write on offline runs, cheap. */
        (void)report_write_ini(t, o, ini_path);
        return UPLOAD_OFFLINE;
    }

    /* /NOUPLOAD → skipped regardless of network state. */
    if (o->no_upload) {
        set_status(t, "skipped");
        (void)report_write_ini(t, o, ini_path);
        return UPLOAD_SKIPPED;
    }

    /* Prompt unless /UPLOAD provides auto-yes. */
    if (!o->do_upload) {
        if (!prompt_user(transport)) {
            set_status(t, "skipped");
            (void)report_write_ini(t, o, ini_path);
            return UPLOAD_SKIPPED;
        }
    }

    /* Have we got an HTTP client? */
    if (!htget_available()) {
        printf("\n");
        printf("No HTGET.EXE on PATH, install mTCP to enable uploads.\n");
        printf("See: brutman.com/mTCP/\n");
        printf("Results saved locally in %s\n", ini_path);
        set_status(t, "no_client");
        (void)report_write_ini(t, o, ini_path);
        return UPLOAD_NO_TSR;
    }

    printf("\nUploading %s to barelybooting.com ...\n", ini_path);
    fflush(stdout);

    {
        char sub_id[SUBMISSION_ID_LEN + 1];
        char url[96];
        sub_id[0] = '\0';
        url[0]    = '\0';
        rc = htget_post(ini_path, sub_id, sizeof(sub_id),
                        url, sizeof(url));
        if (rc == UPLOAD_OK) {
            set_status(t, "uploaded");
            set_submission(t, sub_id, url);
            printf("Uploaded successfully.\n");
            printf("Submission: %s\n", sub_id);
            printf("View at:    %s\n", url);
            /* Re-write INI so the file on disk has the post-upload
             * state. Ignore rc — we've already informed the user of
             * success; failure to rewrite is cosmetic. */
            (void)report_write_ini(t, o, ini_path);
            return UPLOAD_OK;
        } else if (rc == UPLOAD_SERVER) {
            set_status(t, "bad_response");
            printf("Upload response malformed. Results saved locally.\n");
            (void)report_write_ini(t, o, ini_path);
            return rc;
        } else {
            set_status(t, "failed");
            printf("Upload failed: network error.\n");
            printf("Results saved locally in %s\n", ini_path);
            (void)report_write_ini(t, o, ini_path);
            return rc;
        }
    }
}

#else /* CERBERUS_UPLOAD_ENABLED */

/* Stock-build stub. No HTGET, no network, no crash. Writes
 * upload.status=not_built so downstream tooling and the server's
 * permissive parser see a well-formed value. Returns UPLOAD_DISABLED
 * so the caller's log message ("upload disabled") fires correctly. */
int upload_execute(result_table_t *t, const opts_t *o, const char *ini_path)
{
    (void)o;
    (void)ini_path;
    if (t) set_status(t, "not_built");
    return UPLOAD_DISABLED;
}

#endif /* CERBERUS_UPLOAD_ENABLED */

/* Legacy ABI: old upload_ini() entry retained for backward-compat with
 * any caller still linking against it. Delegates to a no-op since the
 * new flow lives in upload_execute(). Always compiled (both build paths). */
int upload_ini(const char *path)
{
    (void)path;
    return UPLOAD_DISABLED;
}
