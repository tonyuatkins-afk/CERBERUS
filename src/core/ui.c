/*
 * Post-detection summary renderer. Phase 1 Task 1.9 minimum-viable.
 *
 * v0.2 scope: formatted text output with confidence meters. Full three-
 * pane box draw with live updates during detection is deferred to a
 * UI-polish follow-up once the detection backbone is exercised on real
 * hardware and we know where the visual pressure points actually are.
 *
 * Confidence meter rendering:
 *   CONF_HIGH   "[H]" or ▓▓▓ (CP437 0xB2 0xB2 0xB2)
 *   CONF_MEDIUM "[M]" or ▓▓░ (0xB2 0xB2 0xB0)
 *   CONF_LOW    "[L]" or ▓░░ (0xB2 0xB0 0xB0)
 *
 * Every adapter class CERBERUS supports renders CP437 correctly, so the
 * shading-block version is the default. Mono-safe text tags are a
 * fallback if we ever encounter a display that renders them as junk.
 */

#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "display.h"

/* Canonical section display order — mirrors the INI writer's
 * section_order. Keys beyond these are ignored by the summary but still
 * appear in CERBERUS.INI. */
typedef struct {
    const char *key;
    const char *label;
} display_row_t;

static const display_row_t rows[] = {
    { "environment.emulator",         "env" },
    { "cpu.detected",                 "cpu" },
    { "cpu.family_model_stepping",    "  f/m/s" },
    { "fpu.friendly",                 "fpu" },
    { "memory.conventional_kb",       "mem_conv" },
    { "memory.extended_kb",           "mem_ext" },
    { "memory.xms_present",           "  xms" },
    { "memory.ems_present",           "  ems" },
    { "cache.present",                "cache" },
    { "bus.class",                    "bus" },
    { "video.adapter",                "video" },
    { "video.chipset",                "  chipset" },
    { "audio.detected",               "audio" },
    { "bios.family",                  "bios" },
    { "bios.date",                    "  date" }
};
#define ROW_COUNT (sizeof(rows) / sizeof(rows[0]))

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

static void print_conf_bar(confidence_t c)
{
    /* CP437 shading blocks 0xB2 (dark), 0xB1 (medium), 0xB0 (light).
     * Using putchar so the bytes hit the console as-is. */
    switch (c) {
        case CONF_HIGH:   putchar(0xB2); putchar(0xB2); putchar(0xB2); break;
        case CONF_MEDIUM: putchar(0xB2); putchar(0xB2); putchar(0xB0); break;
        case CONF_LOW:    putchar(0xB2); putchar(0xB0); putchar(0xB0); break;
        default:          printf("   "); break;
    }
}

static void print_value(const result_t *r, unsigned int max_width)
{
    const char *txt = r->display ? r->display :
                      (r->type == V_STR ? r->v.s : "?");
    unsigned int n;
    if (!txt) txt = "";
    n = (unsigned int)strlen(txt);
    if (n > max_width) {
        /* Truncate with trailing '.' so the user sees it was clipped */
        unsigned int i;
        for (i = 0; i < max_width - 1; i++) putchar(txt[i]);
        putchar('.');
    } else {
        fputs(txt, stdout);
        while (n < max_width) { putchar(' '); n++; }
    }
}

void ui_render_summary(const result_table_t *t, const opts_t *o)
{
    unsigned int i;
    const result_t *sig_r = find_key(t, "environment.confidence_penalty");
    (void)o;

    printf("\n+-- DETECTION SUMMARY -----------------------------------+\n");
    for (i = 0; i < ROW_COUNT; i++) {
        const result_t *r = find_key(t, rows[i].key);
        if (!r) continue;
        printf("| %-12s ", rows[i].label);
        print_value(r, 32);
        printf(" ");
        print_conf_bar(r->confidence);
        printf(" |\n");
    }
    printf("+--------------------------------------------------------+\n");

    if (sig_r && strcmp(
            sig_r->display ? sig_r->display : sig_r->v.s, "none") != 0) {
        printf("Note: emulator detected — downstream confidence capped at MEDIUM.\n");
    }
}
