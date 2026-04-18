#include <stdio.h>
#include "detect.h"
#include "../core/crumb.h"

/* Each subsystem is wrapped in a crumb enter/exit pair so that if a
 * probe hangs the machine, a reboot + rerun finds the orphaned crumb
 * and tells the user "/SKIP:detect.<name>" to bypass. The user's
 * skiplist is also honored here — a second run with /SKIP:detect.cpu
 * skips the probe entirely. */
#define WRAP_DETECT(name, call) do {                                    \
    if (!crumb_skiplist_has("detect." name)) {                          \
        crumb_enter("detect." name);                                    \
        call;                                                            \
        crumb_exit();                                                   \
    }                                                                    \
} while (0)

void detect_all(result_table_t *t, const opts_t *o)
{
    puts("[detect] running...");
    WRAP_DETECT("env",   detect_env(t));
    WRAP_DETECT("cpu",   detect_cpu(t, o));
    WRAP_DETECT("fpu",   detect_fpu(t));
    WRAP_DETECT("mem",   detect_mem(t));
    WRAP_DETECT("cache", detect_cache(t));
    WRAP_DETECT("bus",   detect_bus(t));
    WRAP_DETECT("video", detect_video(t));
    WRAP_DETECT("audio", detect_audio(t));
    WRAP_DETECT("bios",  detect_bios(t));
}
