#ifndef CERBERUS_REPORT_H
#define CERBERUS_REPORT_H

#include "../cerberus.h"

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

#endif
