#ifndef CERBERUS_UI_H
#define CERBERUS_UI_H

#include "../cerberus.h"

/* Render a post-detection summary view with confidence meters.
 *
 * v0.2 minimum: formatted text output (no full-screen box draw — that's
 * follow-up polish). Shows the key canonical signature fields from the
 * result_table plus confidence indicators. Called from main() after
 * report_write_ini + unknown_finalize.
 */
void ui_render_summary(const result_table_t *t, const opts_t *o);

/* Render consistency-flag alert boxes for every rule that failed or
 * warned. The signature visual moment — "HARDWARE CLAIMS X, MEASURES Y"
 * as a CP437 double-line-framed panel. Silent if every rule passed.
 * Called from main() after ui_render_summary. */
void ui_render_consistency_alerts(const result_table_t *t);

#endif
