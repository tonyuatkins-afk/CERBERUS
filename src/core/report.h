#ifndef CERBERUS_REPORT_H
#define CERBERUS_REPORT_H

#include "../cerberus.h"

void report_add_str(result_table_t *t, const char *key, const char *value,
                    confidence_t conf, verdict_t verdict);
void report_add_u32(result_table_t *t, const char *key, unsigned long value,
                    const char *display, confidence_t conf, verdict_t verdict);
void report_add_q16(result_table_t *t, const char *key, long fixed,
                    const char *display, confidence_t conf, verdict_t verdict);

int  report_write_ini(const result_table_t *t, const opts_t *o, const char *path);
void report_hardware_signature(const result_table_t *t, char out_hex[9]);

#endif
