#ifndef CERBERUS_DETECT_H
#define CERBERUS_DETECT_H

#include "../cerberus.h"

void detect_all(result_table_t *t, const opts_t *o);

/* Per-subsystem entry points (Phase 1 tasks 1.0 through 1.8) */
void detect_env(result_table_t *t);
void detect_cpu(result_table_t *t, const opts_t *o);
void detect_fpu(result_table_t *t);
void detect_mem(result_table_t *t);
void detect_cache(result_table_t *t);
void detect_bus(result_table_t *t);
void detect_video(result_table_t *t);
void detect_audio(result_table_t *t);
void detect_bios(result_table_t *t);

#endif
