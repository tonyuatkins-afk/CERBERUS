#include <stdio.h>
#include "display.h"
#include "../cerberus.h"

static adapter_t current_adapter = ADAPTER_UNKNOWN;

void display_init(void)
{
    /* Phase 0 stub — full adapter detection in Task 0.3 */
    current_adapter = ADAPTER_UNKNOWN;
}

void display_shutdown(void)
{
}

void display_banner(void)
{
    puts("CERBERUS " CERBERUS_VERSION " - Retro PC System Intelligence");
    puts("(c) 2026 Tony Atkins / Barely Booting - MIT License");
    puts("");
}

adapter_t display_adapter(void)
{
    return current_adapter;
}

int display_has_color(void)
{
    switch (current_adapter) {
        case ADAPTER_CGA:
        case ADAPTER_EGA_COLOR:
        case ADAPTER_VGA_COLOR:
        case ADAPTER_MCGA:
            return 1;
        default:
            return 0;
    }
}
