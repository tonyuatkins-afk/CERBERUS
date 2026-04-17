/*
 * Consistency engine — cross-checks detect / diagnose / benchmark
 * results for internal coherence. This is CERBERUS's signature
 * feature, the thing that distinguishes it from every other DOS
 * detection tool: "what your hardware claims to be" versus "what it
 * actually behaves as," surfaced explicitly when they disagree.
 *
 * Called after detect_all / diag_all / bench_all have populated the
 * result table. Each rule reads existing keys, compares, and emits
 * consistency.<rule_name> entries with verdicts of PASS / WARN / FAIL.
 *
 * Every rule documents (in its function comment) exactly what failure
 * modes it can catch AND what it CANNOT catch. The plan's §4 design
 * principle: "transparency over black boxes."
 */
#ifndef CERBERUS_CONSIST_H
#define CERBERUS_CONSIST_H

#include "../cerberus.h"

/* Run every landed consistency rule against the populated result
 * table. Emits consistency.<rule_name> rows for each rule; PASS rows
 * are emitted for successful rules so v0.5 output has positive
 * confirmation, not just absence of flags. */
void consist_check(result_table_t *t);

#endif
