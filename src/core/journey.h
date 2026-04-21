#ifndef CERBERUS_JOURNEY_H
#define CERBERUS_JOURNEY_H

#include "../cerberus.h"
#include "head_art.h"

/*
 * Journey framework — v0.6.0 T1.
 *
 * Glues the per-subsystem visual demonstrations into a coherent sequence:
 * title card → measurement → visual → result flash → transition. Each
 * visual module calls the journey primitives here to present itself
 * consistently.
 *
 * Skip semantics:
 *   /NOUI     — no visuals, no title cards, batch summary output
 *   /QUICK    — no visuals, no title cards; interactive summary still
 *                renders (the journey body is skipped; summary not)
 *   Esc      — during any title card or visual: set skip-all flag;
 *                all subsequent title cards and visuals silently bypass
 *                and control reaches the summary screen.
 *   S         — during a visual: skip this visual; proceed to result
 *                flash; next title card resumes normally.
 *
 * Result-flash and title-card rendering are text-mode on every adapter.
 * Visuals may use mode 13h on VGA; they are responsible for mode restore.
 */

/* Reset per-run journey state (skip flags). Call once at main start. */
void journey_init(void);

/* True if a visual should be skipped now. Checks /NOUI, /QUICK, and the
 * skip-all latch set by Esc during a previous title card or visual. */
int  journey_should_skip(const opts_t *o);

/* Render a title card and wait up to 2.5 s or until a key is pressed.
 * variant selects the directional head on the left; title is the
 * ALL-CAPS section name; desc is 1-2 sentence description.
 *
 * Returns:
 *   0 — normal: user watched or any-key-continued
 *   1 — skip-all: user pressed Esc. Caller should still continue but
 *       subsequent journey_* calls will no-op. */
int  journey_title_card(const opts_t *o,
                        head_dir_t variant,
                        const char *title,
                        const char *desc);

/* Brief single-line result banner (~1 second). Rendered on a separate
 * text-mode row after a visual completes. No-op under /NOUI, /QUICK,
 * or after skip-all. */
void journey_result_flash(const opts_t *o, const char *result);

/* During a visual: non-blocking keyboard poll. Returns:
 *   0 — continue rendering
 *   1 — user pressed S: skip this visual (caller should wrap up cleanly)
 *   2 — user pressed Esc: skip all remaining visuals
 *
 * Visuals should call this periodically (e.g., between scan lines or
 * iteration batches) and honor the return value. */
int  journey_poll_skip(void);

#endif
