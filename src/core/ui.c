/*
 * Post-detection three-pane summary renderer.
 *
 * Replaces the minimum-viable v0.2 text-panel output with a screenshot-
 * quality 80×25 text-mode layout:
 *
 *   Row 0       Title bar (full width, bright attribute)
 *   Row 1       Double-line separator
 *   Rows 2-14   DETECTION pane (left 40 cols) │ BENCHMARK pane (right 40 cols)
 *   Row 15      Double-line separator
 *   Rows 16-24  CONSISTENCY VERDICTS pane (full width)
 *
 * Color scheme on VGA / EGA color / MCGA:
 *   Title bar          yellow-on-blue
 *   Pane headers       bright cyan
 *   Labels             gray
 *   Values             bright white
 *   Verdict PASS       bright green
 *   Verdict WARN       bright yellow
 *   Verdict FAIL       bright red
 *   CONF_LOW tag       dim
 *
 * On MDA / Hercules / EGA_MONO / VGA_MONO the attribute byte still
 * applies (intensity + underline + inverse) — ATTR_BOLD for values,
 * ATTR_INVERSE for verdict WARN/FAIL. Color-class attributes degrade
 * to something visually distinguishable on monochrome.
 *
 * Direct VRAM writes are used (B800:0000 color / B000:0000 mono) rather
 * than BIOS INT 10h teletype, because teletype does not set the cell's
 * attribute byte in text mode — colors require AH=09h or direct VRAM.
 * Direct VRAM is faster and simpler for a single-frame render.
 *
 * v0.3 scope: /NOUI bypass still works — main.c guards the render call
 * and this module never touches VRAM when the flag is set. UI hang
 * investigation (issue #3) is not reproducing on current state; the
 * stash remains preserved should a future session need to re-instrument.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include "ui.h"
#include "display.h"
#include "../core/report.h"

/* ----------------------------------------------------------------------- */
/* VRAM helpers                                                             */
/* ----------------------------------------------------------------------- */

#define VRAM_COLS   80
#define VRAM_ROWS   25

/* Returns the correct VRAM base segment for the detected adapter. Called
 * once per render function; cheap enough to not cache. */
static unsigned char __far *vram_base(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

/* Set the terminal cursor position via BIOS INT 10h AH=02h. Called after
 * rendering so the DOS prompt lands below the UI, leaving rows 0-23
 * intact for screenshot capture. */
static void vram_cursor(int row, int col)
{
    union REGS r;
    r.h.ah = 0x02;
    r.h.bh = 0;
    r.h.dh = (unsigned char)row;
    r.h.dl = (unsigned char)col;
    int86(0x10, &r, &r);
}

/* Write a single character+attribute pair to the given cell. No bounds
 * check — caller ensures 0<=row<25, 0<=col<80. */
static void vram_putc(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v = vram_base();
    unsigned int off = (unsigned int)((row * VRAM_COLS + col) * 2);
    v[off]     = ch;
    v[off + 1] = attr;
}

/* Write a null-terminated string at (row, col). Truncates at col 80
 * silently — the caller is responsible for pane-boundary discipline. */
static void vram_puts(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < VRAM_COLS) {
        vram_putc(row, col++, (unsigned char)*s++, attr);
    }
}

/* Fill a row-range with a single character and attribute. Used to clear
 * the screen at the start of render before drawing, so leftover scroll
 * text from [benchmark] running... output doesn't show through empty
 * cells in the layout. */
static void vram_fill(int row_start, int row_end,
                      unsigned char ch, unsigned char attr)
{
    int r, c;
    for (r = row_start; r <= row_end; r++) {
        for (c = 0; c < VRAM_COLS; c++) {
            vram_putc(r, c, ch, attr);
        }
    }
}

/* Draw a horizontal double-line run from col_start to col_end inclusive
 * on the given row. Used for pane separators. */
static void vram_hline_double(int row, int col_start, int col_end,
                              unsigned char attr)
{
    int c;
    for (c = col_start; c <= col_end; c++) {
        vram_putc(row, c, CP437_DBL_HORIZ, attr);
    }
}

/* ----------------------------------------------------------------------- */
/* Result-table helpers                                                     */
/* ----------------------------------------------------------------------- */

static const result_t *find_key(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) return &t->results[i];
    }
    return (const result_t *)0;
}

static const char *value_str(const result_t *r)
{
    if (!r) return "";
    if (r->display) return r->display;
    if (r->type == V_STR && r->v.s) return r->v.s;
    return "";
}

/* Render a "label   value" row with value right-truncated to fit. Labels
 * are gray; values bright white. Used for detection + benchmark panes. */
static void render_kv_row(int row, int col, int pane_width,
                          const char *label, const result_t *r)
{
    const char *val = value_str(r);
    int label_width = 12;            /* fixed-width label column */
    int value_col = col + label_width + 1;
    int value_width = pane_width - label_width - 2;
    int vlen;
    int i;

    vram_puts(row, col, label, ATTR_NORMAL);
    /* value */
    vlen = (int)strlen(val);
    if (vlen > value_width) {
        /* truncate with trailing '.' indicator */
        for (i = 0; i < value_width - 1; i++) {
            vram_putc(row, value_col + i, (unsigned char)val[i], ATTR_BOLD);
        }
        vram_putc(row, value_col + value_width - 1, '.', ATTR_DIM);
    } else {
        for (i = 0; i < vlen; i++) {
            vram_putc(row, value_col + i, (unsigned char)val[i], ATTR_BOLD);
        }
    }
}

/* Like render_kv_row but the value gets CONF_LOW dim formatting. For
 * rows the DB warned on (e.g., whetstone k-whet). */
static void render_kv_row_dim_value(int row, int col, int pane_width,
                                    const char *label, const result_t *r)
{
    const char *val = value_str(r);
    int label_width = 12;
    int value_col = col + label_width + 1;
    int vlen = (int)strlen(val);
    int i;
    vram_puts(row, col, label, ATTR_NORMAL);
    for (i = 0; i < vlen && i < pane_width - label_width - 2; i++) {
        vram_putc(row, value_col + i, (unsigned char)val[i], ATTR_DIM);
    }
}

/* ----------------------------------------------------------------------- */
/* Title bar                                                                */
/* ----------------------------------------------------------------------- */

/* Safely append `src` to `dst[]` without overrunning `cap`. Treats
 * `*dst_len` as the current length (not including the trailing NUL).
 * Truncates silently at cap-1 so the buffer always stays NUL-terminated.
 * Written as a separate helper because the title bar is the ONE place
 * in the UI that concatenates multiple variable-length strings into a
 * fixed-size buffer, and sprintf can silently overflow past its format-
 * width guards (F1 of the UI review — was the most likely cause of the
 * historical UI hang in issue #3 on machines with verbose cpu.detected
 * / video.chipset strings). Belt-and-suspenders bounded concat.
 */
static void bounded_append(char *dst, int *dst_len, int cap, const char *src)
{
    int room = cap - 1 - *dst_len;
    if (room <= 0 || src == NULL) return;
    while (*src && room > 0) {
        dst[*dst_len] = *src++;
        (*dst_len)++;
        room--;
    }
    dst[*dst_len] = '\0';
}

static void render_title(const result_table_t *t)
{
    const result_t *cpu  = find_key(t, "cpu.detected");
    const result_t *vid  = find_key(t, "video.chipset");
    const result_t *bios = find_key(t, "bios.date");
    /* 81 bytes = 80 display columns + trailing NUL. We use bounded_append
     * throughout rather than sprintf to guarantee no overrun regardless
     * of detected-value length. */
    char line[VRAM_COLS + 1];
    int  len = 0;
    char sep[4];

    /* Compose: " CERBERUS <ver> = <cpu> / <video> / <bios>"  */
    line[0] = '\0';
    bounded_append(line, &len, VRAM_COLS + 1, " CERBERUS ");
#ifdef CERBERUS_VERSION
    bounded_append(line, &len, VRAM_COLS + 1, CERBERUS_VERSION);
#else
    bounded_append(line, &len, VRAM_COLS + 1, "dev");
#endif
    /* Use a literal ' = ' as the separator rather than CP437_DBL_HORIZ —
     * the inline byte shows up cleanly on any adapter without the %c
     * cast-width game the sprintf path played. */
    bounded_append(line, &len, VRAM_COLS + 1, " ");
    sep[0] = (char)CP437_DBL_HORIZ;
    sep[1] = ' ';
    sep[2] = '\0';
    bounded_append(line, &len, VRAM_COLS + 1, sep);
    bounded_append(line, &len, VRAM_COLS + 1, cpu  ? value_str(cpu)  : "(cpu ?)");
    bounded_append(line, &len, VRAM_COLS + 1, " / ");
    bounded_append(line, &len, VRAM_COLS + 1, vid  ? value_str(vid)  : "(video ?)");
    bounded_append(line, &len, VRAM_COLS + 1, " / ");
    bounded_append(line, &len, VRAM_COLS + 1, bios ? value_str(bios) : "(bios ?)");

    /* Fill full row with title-bar attribute (yellow on blue), then
     * overlay the composed string starting at col 0. */
    {
        int c;
        unsigned char title_attr = 0x1E;  /* yellow on blue */
        for (c = 0; c < VRAM_COLS; c++) {
            vram_putc(0, c, ' ', title_attr);
        }
        vram_puts(0, 0, line, title_attr);
    }

    /* Row 1: full-width double-line separator */
    vram_hline_double(1, 0, VRAM_COLS - 1, ATTR_NORMAL);
}

/* ----------------------------------------------------------------------- */
/* Detection pane (left 40 cols)                                            */
/* ----------------------------------------------------------------------- */

typedef struct {
    const char *key;
    const char *label;
} display_row_t;

static const display_row_t detect_rows[] = {
    { "environment.emulator",      "env" },
    { "cpu.detected",              "cpu" },
    { "cpu.family_model_stepping", "  fam/mdl/stp" },
    { "fpu.friendly",              "fpu" },
    { "memory.conventional_kb",    "mem conv" },
    { "memory.extended_kb",        "mem ext" },
    { "cache.present",             "cache" },
    { "bus.class",                 "bus" },
    { "video.adapter",             "video" },
    { "video.chipset",             "  chipset" },
    { "audio.detected",            "audio" },
    { "bios.family",               "bios" },
    { "bios.date",                 "  date" }
};
#define DETECT_ROWS_COUNT (sizeof(detect_rows) / sizeof(detect_rows[0]))

static void render_detection_pane(const result_table_t *t)
{
    unsigned int i;
    int r = 3;

    /* Pane header row 2: "DETECTION" in cyan, centered-ish with shade bars */
    vram_putc(2, 1, CP437_SHADE_DARK, ATTR_CYAN);
    vram_putc(2, 2, ' ', ATTR_CYAN);
    vram_puts(2, 3, "DETECTION", ATTR_BOLD);
    vram_putc(2, 13, ' ', ATTR_CYAN);

    for (i = 0; i < DETECT_ROWS_COUNT && r <= 14; i++) {
        const result_t *row = find_key(t, detect_rows[i].key);
        if (!row) continue;
        render_kv_row(r, 1, 38, detect_rows[i].label, row);
        r++;
    }
}

/* ----------------------------------------------------------------------- */
/* Benchmark pane (right 40 cols)                                           */
/* ----------------------------------------------------------------------- */

static const display_row_t bench_rows[] = {
    { "bench.cpu.int_iters_per_sec", "cpu int/s" },
    { "bench.cpu.dhrystones",        "dhrystones" },
    { "bench.fpu.ops_per_sec",       "fpu ops/s" },
    { "bench.memory.write_kbps",     "mem write" },
    { "bench.memory.read_kbps",      "mem read" },
    { "bench.memory.copy_kbps",      "mem copy" },
    { "bench.cpu_xt_factor",         "x PC-XT cpu" },
    { "bench.mem_xt_factor",         "x PC-XT mem" }
};
#define BENCH_ROWS_COUNT (sizeof(bench_rows) / sizeof(bench_rows[0]))

/* Separate keys for rows that should render in DIM (CONF_LOW) style. */
static const display_row_t bench_rows_dim[] = {
    { "bench.fpu.k_whetstones", "k-whet (LOW)" },
    { "bench.fpu_xt_factor",    "x PC-XT fpu" }
};
#define BENCH_ROWS_DIM_COUNT (sizeof(bench_rows_dim) / sizeof(bench_rows_dim[0]))

static void render_benchmark_pane(const result_table_t *t)
{
    unsigned int i;
    int r = 3;

    /* Pane header at row 2, right half */
    vram_putc(2, 41, CP437_SHADE_DARK, ATTR_CYAN);
    vram_putc(2, 42, ' ', ATTR_CYAN);
    vram_puts(2, 43, "BENCHMARKS", ATTR_BOLD);
    vram_putc(2, 54, ' ', ATTR_CYAN);

    for (i = 0; i < BENCH_ROWS_COUNT && r <= 14; i++) {
        const result_t *row = find_key(t, bench_rows[i].key);
        if (!row) continue;
        render_kv_row(r, 41, 38, bench_rows[i].label, row);
        r++;
    }
    for (i = 0; i < BENCH_ROWS_DIM_COUNT && r <= 14; i++) {
        const result_t *row = find_key(t, bench_rows_dim[i].key);
        if (!row) continue;
        render_kv_row_dim_value(r, 41, 38, bench_rows_dim[i].label, row);
        r++;
    }
}

/* Vertical divider at col 40 between detection and benchmark panes. */
static void render_vertical_divider(void)
{
    int r;
    for (r = 2; r <= 14; r++) {
        vram_putc(r, 40, CP437_DBL_VERT, ATTR_NORMAL);
    }
}

/* ----------------------------------------------------------------------- */
/* Public: summary = title + detection pane + benchmark pane                */
/* ----------------------------------------------------------------------- */

void ui_render_summary(const result_table_t *t, const opts_t *o)
{
    (void)o;

    /* Clear the UI region (rows 0-24) so scrolled stdout from earlier
     * detect/diag/bench output doesn't bleed through empty cells. */
    vram_fill(0, 24, ' ', ATTR_NORMAL);

    render_title(t);
    render_vertical_divider();
    render_detection_pane(t);
    render_benchmark_pane(t);

    /* Row 15 separator between top half and consistency panel */
    vram_hline_double(15, 0, VRAM_COLS - 1, ATTR_NORMAL);
}

/* ----------------------------------------------------------------------- */
/* Consistency-verdict pane (rows 16-24, full width)                        */
/* ----------------------------------------------------------------------- */

/* Classify a verdict into a color attribute + 4-char tag. */
static unsigned char verdict_attr(verdict_t v)
{
    switch (v) {
        case VERDICT_PASS: return 0x0A;   /* bright green */
        case VERDICT_WARN: return 0x0E;   /* bright yellow */
        case VERDICT_FAIL: return 0x0C;   /* bright red */
        default:           return 0x07;   /* gray */
    }
}

static const char *verdict_tag(verdict_t v)
{
    switch (v) {
        case VERDICT_PASS: return "PASS";
        case VERDICT_WARN: return "WARN";
        case VERDICT_FAIL: return "FAIL";
        default:           return "????";
    }
}

/* Render one verdict row:  [TAG] rule_name    explanation...
 * The TAG bracket and explanation are full-color; the rule name in bold.
 */
static void render_verdict_row(int row, const result_t *r)
{
    /* Strip the sub-tree prefix so the rule-name column shows the
     * distinguishing tail, not the redundant category. Both tails the
     * pane renders today are handled; any future verdict-source prefix
     * would need its own branch. */
    const char *key;
    const char *val = value_str(r);
    unsigned char va = verdict_attr(r->verdict);
    const char *tag = verdict_tag(r->verdict);

    if (strncmp(r->key, "consistency.", 12) == 0) {
        key = r->key + 12;
    } else if (strncmp(r->key, "diagnose.", 9) == 0) {
        key = r->key + 9;
    } else {
        key = r->key;
    }

    /* "[TAG]" at col 2-7 */
    vram_putc(row, 2, '[',    ATTR_NORMAL);
    vram_putc(row, 3, (unsigned char)tag[0], va);
    vram_putc(row, 4, (unsigned char)tag[1], va);
    vram_putc(row, 5, (unsigned char)tag[2], va);
    vram_putc(row, 6, (unsigned char)tag[3], va);
    vram_putc(row, 7, ']',    ATTR_NORMAL);

    /* rule name at col 9-28 (20 wide, bold) */
    {
        int c = 9;
        int i;
        int klen = (int)strlen(key);
        if (klen > 20) klen = 20;
        for (i = 0; i < klen; i++) {
            vram_putc(row, c++, (unsigned char)key[i], ATTR_BOLD);
        }
    }

    /* explanation (condensed — strip leading "pass (" / "WARN: " / "FAIL:
     * " tokens since the tag already conveys verdict). Starts col 30,
     * runs to col 78. */
    {
        const char *v = val;
        /* Skip any well-known leading verdict tokens to avoid duplication
         * with the [TAG] column. Matches on "pass (", "WARN: ", "FAIL: ",
         * "WARN ", "FAIL ", case-insensitive variations. */
        if (strncmp(v, "pass (", 6) == 0) {
            v += 6;
            /* also trim trailing ')' to match */
        } else if (strncmp(v, "WARN: ", 6) == 0) v += 6;
        else if (strncmp(v, "FAIL: ", 6) == 0) v += 6;
        else if (strncmp(v, "WARN (", 6) == 0) v += 6;
        else if (strncmp(v, "pass ",  5) == 0) v += 5;

        {
            int c = 30;
            int maxcol = 78;
            /* Reserve the last cell (col 78) for a truncation indicator
             * IF we run out of payload. Write payload chars up to col 77,
             * then if still-more-payload, place indicator at col 78. Fixes
             * S3 of the UI review (the old logic overwrote a real payload
             * char AND used 0xAE = « instead of 0xAF = »). */
            while (*v && c < maxcol) {
                /* skip trailing ')' on the "pass (...)" variant */
                if (*v == ')' && v[1] == '\0') break;
                vram_putc(row, c++, (unsigned char)*v++, va);
            }
            if (*v && *v != ')') {
                /* Payload remaining — mark truncation at col 78 with the
                 * right-pointing double-guillemet (CP437 0xAF = »). */
                vram_putc(row, maxcol, (unsigned char)0xAF, ATTR_DIM);
            }
        }
    }
}

/* True if this result row contributes a meaningful verdict to the pane.
 * Includes: consistency.* (the dedicated rule engine) AND diagnose.cache.status
 * + diagnose.dma.summary (the two v0.3 diagnostics whose summary carries a
 * real PASS/WARN/FAIL verdict, the rest of diagnose.* is either already
 * aggregated under those two or shows up as detail rows that don't need
 * their own line). S5 of the UI review — without this, the v0.3 diag_cache
 * and diag_dma results are screenshot-invisible despite being the
 * headlining new v0.3 features. */
static int is_verdict_row(const result_t *r)
{
    if (r->verdict == VERDICT_UNKNOWN) return 0;
    if (strncmp(r->key, "consistency.", 12) == 0) return 1;
    if (strcmp(r->key, "diagnose.cache.status") == 0) return 1;
    if (strcmp(r->key, "diagnose.dma.summary") == 0) return 1;
    return 0;
}

void ui_render_consistency_alerts(const result_table_t *t)
{
    unsigned int i;
    int r = 17;
    int n_verdicts = 0;

    /* Pane header at row 16 — renamed to "SYSTEM VERDICTS" to reflect
     * that diagnose-head cache + DMA verdicts now appear alongside the
     * consistency-rule verdicts. */
    vram_putc(16, 1, CP437_SHADE_DARK, ATTR_CYAN);
    vram_putc(16, 2, ' ', ATTR_CYAN);
    vram_puts(16, 3, "SYSTEM VERDICTS", ATTR_BOLD);
    vram_putc(16, 19, ' ', ATTR_CYAN);

    /* Render each qualifying row — consistency.* and the two diagnose.*
     * summaries with explicit verdicts. */
    for (i = 0; i < t->count && r <= 24; i++) {
        const result_t *row = &t->results[i];
        if (!is_verdict_row(row)) continue;
        render_verdict_row(r, row);
        r++;
        n_verdicts++;
    }

    if (n_verdicts == 0) {
        vram_puts(17, 2, "(no verdicts applicable to this run)", ATTR_DIM);
    }

    /* Position cursor at row 24 col 0 so when CERBERUS exits to DOS the
     * prompt lands below the UI region. Rows 0-23 remain intact for
     * screenshot capture. Row 24 will be overwritten by the DOS prompt
     * but the rest of the frame stays visible. */
    vram_cursor(24, 0);
}
