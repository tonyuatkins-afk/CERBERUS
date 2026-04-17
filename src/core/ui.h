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

#endif
