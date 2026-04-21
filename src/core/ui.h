#ifndef CERBERUS_UI_H
#define CERBERUS_UI_H

#include "../cerberus.h"

/* Render the post-run summary in the v0.5.0 scrollable three-heads UI.
 * Builds a virtual row table covering Detection, Benchmarks, and System
 * Verdicts sections (each headed by a Cerberus dog head in CP437 block
 * art) and enters a BIOS INT 16h navigation loop. Exits on Q or Esc.
 * Called from main() after report_write_ini + unknown_finalize.
 */
void ui_render_summary(const result_table_t *t, const opts_t *o);

/* v0.4.x compatibility stub. The consistency verdicts are rendered
 * inline as the third section of ui_render_summary in v0.5.0; this
 * function is a no-op, preserved so external callers or plug-ins that
 * invoke both in sequence continue to link. */
void ui_render_consistency_alerts(const result_table_t *t);

/* /NOUI batch mode: print the same content as ui_render_summary but
 * as plain text to stdout, no VRAM writes, no interactive scroll.
 * Preserves the escape hatch for real-iron UI-render hangs (issue #3)
 * while still surfacing the run results. */
void ui_render_batch(const result_table_t *t);

#endif
