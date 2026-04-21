/*
 * Shared Cerberus head-art tables. See head_art.h for usage.
 * v0.6.0 T0c — directional variants (left/center/right) replace
 * the single forward-facing head used through v0.5.0.
 */

#include "head_art.h"

/* CP437 glyph codes. Named here for readability; same codes as
 * the v0.5.0 draw_head() function. */
#define GB 0xB2   /* body — dark shade */
#define HD 0xDC   /* lower half block */
#define HU 0xDF   /* upper half block */
#define HL 0xDD   /* right half block (anchors left edge) */
#define HR 0xDE   /* left half block (anchors right edge) */
#define EY 0x09   /* hollow bullet — eye */
#define FG 0x1F   /* down triangle — fang */
#define SP 0x20   /* space */

#define B(g) { (g), HEAD_CELL_BODY }
#define E(g) { (g), HEAD_CELL_EYE }
#define F(g) { (g), HEAD_CELL_FANG }

const head_cell_t head_art[HEAD_VARIANT_COUNT][HEAD_ROWS][HEAD_COLS] = {
    /* ==== HEAD_LEFT (faces left) ========================================
     * Snout tip at cols 0-1 (fang at col 1). Single eye on the right
     * side of the face (col 6) — looking away from the viewer, toward
     * where the head is facing. Ear bump on the top-right (col 8).
     * The animation-era "left head" in intro.c had two eyes; in v0.6.0
     * the directional profile drops it to one on this side. */
    {
        /* row 0 — top arc, flat on the snout side, ear bump right */
        { B(SP), B(SP), B(HD), B(GB), B(GB), B(GB), B(GB), B(HD), B(HU) },
        /* row 1 — skull shoulders, snout extends left */
        { B(SP), B(HD), B(GB), B(GB), B(GB), B(GB), B(GB), B(GB), B(HL) },
        /* row 2 — eye line, eye at col 6 */
        { B(HR), B(GB), B(GB), B(GB), B(GB), B(GB), E(EY), B(GB), B(HL) },
        /* row 3 — snout tip with fang at col 1 */
        { B(HU), F(FG), B(GB), B(GB), B(GB), B(GB), B(GB), B(HU), B(SP) }
    },

    /* ==== HEAD_CENTER (faces forward, dominant) =========================
     * Two eyes (cols 3, 5), two fangs (cols 3, 5). Symmetric silhouette
     * matches the v0.5.0 head so the animation's eye-cascade rhythm
     * (phases 2 + 3 on this head) stays intact. */
    {
        /* row 0 */
        { B(SP), B(HD), B(GB), B(GB), B(GB), B(GB), B(GB), B(HD), B(SP) },
        /* row 1 */
        { B(HR), B(GB), B(GB), B(GB), B(GB), B(GB), B(GB), B(GB), B(HL) },
        /* row 2 — eyes at cols 3 and 5 */
        { B(HR), B(GB), B(GB), E(EY), B(GB), E(EY), B(GB), B(GB), B(HL) },
        /* row 3 — fangs at cols 3 and 5 */
        { B(SP), B(HU), B(GB), F(FG), B(GB), F(FG), B(GB), B(HU), B(SP) }
    },

    /* ==== HEAD_RIGHT (faces right, mirror of HEAD_LEFT) =================
     * Snout tip at cols 7-8 (fang at col 7). Single eye at col 2 — on
     * the cheek side, facing away. Ear bump on the top-left (col 0). */
    {
        /* row 0 */
        { B(HU), B(HD), B(GB), B(GB), B(GB), B(GB), B(HD), B(SP), B(SP) },
        /* row 1 */
        { B(HR), B(GB), B(GB), B(GB), B(GB), B(GB), B(GB), B(HD), B(SP) },
        /* row 2 — eye at col 2 */
        { B(HR), B(GB), E(EY), B(GB), B(GB), B(GB), B(GB), B(GB), B(HL) },
        /* row 3 — snout tip + fang at col 7 */
        { B(SP), B(HU), B(GB), B(GB), B(GB), B(GB), B(GB), F(FG), B(HU) }
    }
};

const signed char head_eye_col[HEAD_VARIANT_COUNT][2] = {
    { 6, -1 },   /* LEFT   — one eye at col 6 */
    { 3,  5 },   /* CENTER — eyes at cols 3 and 5 */
    { 2, -1 }    /* RIGHT  — one eye at col 2 */
};

const unsigned char head_eye_count[HEAD_VARIANT_COUNT] = { 1, 2, 1 };
