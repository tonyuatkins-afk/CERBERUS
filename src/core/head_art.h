#ifndef CERBERUS_HEAD_ART_H
#define CERBERUS_HEAD_ART_H

/*
 * Shared Cerberus head-art tables — v0.6.0 T0c.
 *
 * Three directional variants of the 9-col x 4-row dog head used on
 * the boot splash and the scrollable summary section headers. Left
 * and right heads face outward (away from the viewer) — single eye
 * on the far side, snout tip toward their direction, ear bump on
 * the opposite side. Center head faces forward with two visible
 * eyes and symmetric fangs — the dominant head in mythology.
 *
 * Usage pattern:
 *   for (r = 0; r < HEAD_ROWS; r++)
 *     for (c = 0; c < HEAD_COLS; c++) {
 *       head_cell_t cell = head_art[HEAD_LEFT][r][c];
 *       put_cell(screen_row + r, screen_col + c, cell.glyph,
 *                cell.accent ? accent_attr : body_attr);
 *     }
 *
 * accent flag = 1 on eye and fang cells; caller supplies the
 * accent_attr (typically bright yellow or red on color, white on
 * mono). body cells use body_attr (typically cyan on color, bright
 * white on mono).
 *
 * Eye position metadata (head_eye_col[variant][slot]) exposes the
 * visible-eye columns per variant for the intro's eye-cascade
 * animation. A slot of -1 marks "no eye in this slot" — left and
 * right heads have one eye (slot 0); center has two (slots 0 + 1).
 */

typedef enum {
    HEAD_LEFT   = 0,
    HEAD_CENTER = 1,
    HEAD_RIGHT  = 2
} head_dir_t;

#define HEAD_VARIANT_COUNT 3
#define HEAD_COLS          9
#define HEAD_ROWS          4

#define HEAD_CELL_BODY 0
#define HEAD_CELL_EYE  1
#define HEAD_CELL_FANG 2

typedef struct {
    unsigned char glyph;
    unsigned char kind;   /* HEAD_CELL_BODY / _EYE / _FANG */
} head_cell_t;

extern const head_cell_t head_art[HEAD_VARIANT_COUNT][HEAD_ROWS][HEAD_COLS];

/* Eye-column metadata per variant. Two slots; unused slot is -1.
 * Row is always head row 2 (eye line). */
extern const signed char head_eye_col[HEAD_VARIANT_COUNT][2];

/* Count of visible eyes per variant (1 or 2). */
extern const unsigned char head_eye_count[HEAD_VARIANT_COUNT];

#endif
