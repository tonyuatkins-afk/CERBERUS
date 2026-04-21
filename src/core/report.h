#ifndef CERBERUS_REPORT_H
#define CERBERUS_REPORT_H

#include "../cerberus.h"

/*
 * NOTE: report_add_* has NO dedup. Each call appends a fresh row to the
 * result table — two calls with the same key produce two entries, and
 * the INI writer emits both in insertion order. Any UI renderer that
 * iterates the table will render both, which visually presents as a
 * duplicate row to the user.
 *
 * Responsibility for key uniqueness lives with callers: every detect
 * module, diag rule, bench rule, and consistency rule is expected to
 * own a disjoint set of keys. If two rules contend for the same key,
 * rename one (consistency rules keep a consistency.<rule_name> scheme
 * specifically to avoid collisions). See docs/consistency-rules.md's
 * "Adding a new rule" section for the convention.
 *
 * Future improvement: push_slot could check for an existing key and
 * update in place instead of appending. Deferred for Phase 4 to keep
 * the emit path allocation-free and predictable; revisit if a real
 * collision ever lands in a rule set.
 */

/* Result-builder helpers — each covers one value_type_t variant */
void report_add_str(result_table_t *t, const char *key, const char *value,
                    confidence_t conf, verdict_t verdict);
void report_add_u32(result_table_t *t, const char *key, unsigned long value,
                    const char *display, confidence_t conf, verdict_t verdict);
void report_add_q16(result_table_t *t, const char *key, long fixed,
                    const char *display, confidence_t conf, verdict_t verdict);

/* Write the full INI file, compute and embed both signatures.
 * Returns 0 on success, -1 on error. */
int  report_write_ini(const result_table_t *t, const opts_t *o, const char *path);

/* Compute the hardware-identity signature over the canonical 7-key subset
 * at CONF_HIGH only. MEDIUM/LOW values serialize as "unknown".
 * Writes a 9-byte hex string (8 chars + NUL) into out_hex. */
void report_hardware_signature(const result_table_t *t, char out_hex[9]);

/* Set the verdict on an existing result-table entry by key. Returns 1 on
 * success, 0 if the key wasn't found. Used by diagnose modules to attach
 * pass/warn/fail verdicts to detect entries without adding new rows. */
int report_set_verdict(result_table_t *t, const char *key, verdict_t v);

/* Update an existing V_STR row in place, or append a new one if the key
 * does not yet exist. Used by upload.c to overwrite upload.status /
 * upload.submission_id / upload.url after the POST completes, without
 * creating duplicate rows at the second report_write_ini pass.
 * Caller retains lifetime responsibility for `value` (see note above).
 * v0.7.0-rc2 quality-gate addition. */
void report_update_str(result_table_t *t, const char *key, const char *value,
                       confidence_t conf, verdict_t verdict);

#endif
