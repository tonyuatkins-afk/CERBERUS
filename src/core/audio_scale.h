#ifndef CERBERUS_AUDIO_SCALE_H
#define CERBERUS_AUDIO_SCALE_H

#include "../cerberus.h"

/* v0.6.0 T7: 8-note C major scale visual via PC speaker. Fires near
 * end of journey, before the summary screen. Skipped under /NOUI,
 * /QUICK, or skip-all latch. */
void audio_scale_visual(const opts_t *o);

#endif
