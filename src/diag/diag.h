#ifndef CERBERUS_DIAG_H
#define CERBERUS_DIAG_H

#include "../cerberus.h"

void diag_all(result_table_t *t, const opts_t *o);

/* Per-subsystem diagnostic entry points. Each reads existing detect
 * results from the table and attaches verdicts to those entries (via
 * report_set_verdict) while optionally adding diagnose.<subsys>.<test>
 * rows for fine-grained pass/fail reporting. */
void diag_cpu(result_table_t *t);
void diag_mem(result_table_t *t);
void diag_fpu(result_table_t *t);

#endif
