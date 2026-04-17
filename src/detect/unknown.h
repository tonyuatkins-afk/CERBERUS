/*
 * Unknown-hardware submission path.
 *
 * Any detect module that encounters hardware not in its DB calls
 * unknown_record() with (a) a subsystem tag, (b) a one-line summary, and
 * (c) optional raw probe data. CERBERUS accumulates these in memory,
 * writes them to CERBERUS.UNK at end-of-run (same location rules as the
 * crash-recovery breadcrumb — CWD, %TEMP%, %TMP%, or skip), and prints
 * a summary card inviting the user to submit a GitHub issue using the
 * hardware-submission template.
 *
 * This is what turns CERBERUS from a closed identification tool into
 * a community-grown database — every VOGONS contributor with a weird
 * Cyrix or no-name VGA card becomes a CSV PR.
 */
#ifndef CERBERUS_UNKNOWN_H
#define CERBERUS_UNKNOWN_H

void unknown_init(void);

/* Record a capture. `subsystem` is one of "cpu", "fpu", "video", "audio",
 * "bios". `summary` is one line describing what probed data is
 * interesting. `detail` may be NULL or a printf-style value already
 * formatted. All pointers may be literal strings; they are not copied —
 * caller must ensure lifetime until unknown_finalize(). */
void unknown_record(const char *subsystem, const char *summary,
                    const char *detail);

/* Write CERBERUS.UNK and render end-of-run card if any captures. */
void unknown_finalize(void);

/* Returns number of captures accumulated so far (for test + reporting) */
unsigned int unknown_count(void);

#endif
