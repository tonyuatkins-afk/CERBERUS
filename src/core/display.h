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

/* Text-mode attributes (CGA/EGA/VGA). On MDA/Hercules these map to
 * intensity/underline/inverse via BIOS. */
#define ATTR_NORMAL    0x07  /* gray on black */
#define ATTR_BOLD      0x0F  /* white on black */
#define ATTR_DIM       0x08
#define ATTR_INVERSE   0x70
#define ATTR_UNDERLINE 0x01  /* MDA/Hercules only */
#define ATTR_RED       0x04
#define ATTR_GREEN     0x02
#define ATTR_YELLOW    0x0E
#define ATTR_CYAN      0x0B

/* CP437 line-drawing glyphs — emit as single bytes (CP437 encoding).
 * Source files must be ASCII-only for portability; use these defines. */
#define CP437_HORIZ        0xC4  /* — */
#define CP437_VERT         0xB3  /* | */
#define CP437_TL           0xDA
#define CP437_TR           0xBF
#define CP437_BL           0xC0
#define CP437_BR           0xD9
#define CP437_DBL_HORIZ    0xCD
#define CP437_DBL_VERT     0xBA
#define CP437_DBL_TL       0xC9
#define CP437_DBL_TR       0xBB
#define CP437_DBL_BL       0xC8
#define CP437_DBL_BR       0xBC
#define CP437_DBL_T_DOWN   0xCB
#define CP437_DBL_T_UP     0xCA
#define CP437_DBL_T_LEFT   0xB9
#define CP437_DBL_T_RIGHT  0xCC
#define CP437_DBL_CROSS    0xCE
#define CP437_SHADE_LIGHT  0xB0
#define CP437_SHADE_MED    0xB1
#define CP437_SHADE_DARK   0xB2
#define CP437_BLOCK        0xDB

void      display_init(void);
void      display_shutdown(void);
void      display_banner(void);
adapter_t display_adapter(void);
const char *display_adapter_name(adapter_t a);
int       display_has_color(void);

/* Basic primitives (BIOS INT 10h teletype path — works on every adapter) */
void display_putc(char c);
void display_puts(const char *s);
void display_goto(int row, int col);
void display_set_attr(unsigned char attr);

/* Box drawing — single line */
void display_box(int row, int col, int width, int height);
/* Box drawing — double line (used for the three-pane frame and alert panel) */
void display_box_double(int row, int col, int width, int height);

/* CGA retrace-sync helper — returns after one vertical retrace has started.
 * Call before a direct-VRAM burst write to avoid CGA "snow." No-op for
 * non-CGA adapters. */
void display_wait_retrace(void);

#endif
