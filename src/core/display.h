#ifndef CERBERUS_DISPLAY_H
#define CERBERUS_DISPLAY_H

typedef enum {
    ADAPTER_UNKNOWN   = 0,
    ADAPTER_MDA       = 1,
    ADAPTER_CGA       = 2,
    ADAPTER_HERCULES  = 3,
    ADAPTER_EGA_MONO  = 4,
    ADAPTER_EGA_COLOR = 5,
    ADAPTER_VGA_MONO  = 6,
    ADAPTER_VGA_COLOR = 7,
    ADAPTER_MCGA      = 8
} adapter_t;

void      display_init(void);
void      display_shutdown(void);
void      display_banner(void);
adapter_t display_adapter(void);
int       display_has_color(void);

#endif
