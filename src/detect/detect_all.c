#include <stdio.h>
#include "detect.h"

void detect_all(result_table_t *t, const opts_t *o)
{
    puts("[detect] running...");
    detect_env(t);
    detect_cpu(t, o);
    detect_fpu(t);
    detect_mem(t);
    detect_cache(t);
    detect_bus(t);
    detect_video(t);
    detect_audio(t);
    detect_bios(t);
}
