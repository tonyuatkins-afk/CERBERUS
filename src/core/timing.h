#ifndef CERBERUS_TIMING_H
#define CERBERUS_TIMING_H

typedef unsigned long us_t;
typedef unsigned int  ticks_t;

void  timing_init(void);
void  timing_start(void);
us_t  timing_stop(void);
us_t  timing_ticks_to_us(unsigned long ticks);
void  timing_wait_us(us_t microseconds);
int   timing_emulator_hint(void);

#endif
