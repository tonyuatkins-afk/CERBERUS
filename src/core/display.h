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

/* M3.5 /MONO support. display_set_force_mono(1) before display_init()
 * forces display_is_mono() to return 1 regardless of detected adapter.
 * Rendering paths that branch on mono-vs-color SHOULD query
 * display_is_mono() rather than comparing adapter tokens directly, so
 * /MONO propagates cleanly. Query after display_init(). */
void      display_set_force_mono(int force);
int       display_is_mono(void);

/* M3.6: on color adapters, issue INT 10h AX=1003h BL=00h to switch the
 * attribute high bit from blink-enable to background-intensity, giving
 * 16 background colors. Zero-cost quality win per MS-DOS UI-UX research
 * Part B. No-op on mono adapters. Called from display_init(). */
void      display_enable_16bg_colors(void);

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

/* v0.8.1 M3.3: Hercules sub-variant discrimination.
 *
 * Three documented Hercules adapter variants share the ADAPTER_HERCULES
 * generic token (we don't split the adapter_t enum because that token
 * is part of the hardware signature). The variant returned below is a
 * best-effort classification based on 3BAh status-register bits 6:4,
 * which the HGC+ and InColor boards use to expose a small identification
 * code and which the original HGC left unspecified.
 *
 * Returns HERCULES_VARIANT_NA when the current adapter is not Hercules.
 * display_init() must have run first.
 *
 * Real-iron signature validation on actual HGC / HGC+ / InColor cards
 * is required to confirm the bit assignments this probe uses; documented
 * references disagree on some of the finer details. Pure classification
 * is host-testable via display_classify_hercules_id(). */
typedef enum {
    HERCULES_VARIANT_NA       = 0,   /* not a Hercules adapter */
    HERCULES_VARIANT_HGC      = 1,   /* original 1982 Hercules Graphics Card */
    HERCULES_VARIANT_HGCPLUS  = 2,   /* 1986 Plus (adds softfont RAM) */
    HERCULES_VARIANT_INCOLOR  = 3,   /* 1987 In Color (16-color) */
    HERCULES_VARIANT_UNKNOWN  = 4    /* Hercules-family but ID bits unmapped */
} hercules_variant_t;

hercules_variant_t display_hercules_variant(void);
const char        *display_hercules_variant_token(hercules_variant_t v);

/* Pure classifier — given the 3 ID bits (bits 6:4 of port 3BAh, right-
 * shifted into 2:0 of a byte), return the variant. Host-testable. */
hercules_variant_t display_classify_hercules_id(unsigned char id_bits);

#endif
