#ifndef CERBERUS_UPLOAD_H
#define CERBERUS_UPLOAD_H

#include "../cerberus.h"

/* Return codes from upload_execute. UI layer uses these to phrase the
 * UPLOAD STATUS row in the scrollable summary. */
#define UPLOAD_OK        0   /* 200 from server, submission_id received */
#define UPLOAD_OFFLINE   1   /* transport=none; never attempted */
#define UPLOAD_SKIPPED   2   /* user declined prompt, or /NOUPLOAD set */
#define UPLOAD_NO_TSR    3   /* no HTGET.EXE or mTCP equivalent found */
#define UPLOAD_NETWORK   4   /* DNS fail, connection refused, timeout */
#define UPLOAD_SERVER    5   /* non-200 HTTP response */
#define UPLOAD_DISABLED  6   /* build-time disabled (unused in v0.7.0) */

/* Populate [upload] nickname/notes into the result table from opts.
 * Call before report_write_ini so they land in the INI. */
void upload_populate_metadata(result_table_t *t, const opts_t *o);

/* The full upload flow — runs after the initial report_write_ini:
 *   1. Read network transport from table
 *   2. If offline or /NOUPLOAD: emit upload.status=offline/skipped
 *   3. If online and not /UPLOAD: prompt user (default Y)
 *   4. If user consents: shell out to HTGET with CERBERUS.INI as POST body
 *   5. Parse response: line 1 = submission id, line 2 = URL
 *   6. Emit upload.status, upload.submission_id, upload.url to table
 *   7. Re-write INI with the new rows
 *
 * Returns one of the UPLOAD_* codes above. Never crashes on failure.
 * ini_path is the path to CERBERUS.INI (for re-write + HTGET source). */
int  upload_execute(result_table_t *t, const opts_t *o, const char *ini_path);

#endif
