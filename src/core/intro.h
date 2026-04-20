#ifndef CERBERUS_INTRO_H
#define CERBERUS_INTRO_H

#include "../cerberus.h"

/* CERBERUS boot splash.
 *
 * Adapter-aware three-headed-dog ANSI intro. Invoked once during
 * startup between display_init and display_banner. Skipped under
 * /NOUI, /NOINTRO, or when the caller has already determined the
 * environment is an emulator.
 *
 * VGA color path: DAC palette fade-in from black, three eye-pairs
 * illuminate in sequence, palette shimmer on the dog backdrop,
 * DAC fade-out to black. EGA and lower adapters run the same
 * choreography via attribute-byte cycling only. On MDA and
 * Hercules the dog is rendered in high-contrast block shading and
 * the three eyes pulse via the intensity bit.
 *
 * Keypress dismisses immediately; a ~1500 ms no-keypress timeout
 * dismisses automatically. On return, the screen is cleared to
 * black and the cursor is at (0, 0) so the subsequent
 * display_banner renders cleanly. */
void intro_splash(const opts_t *o);

#endif
