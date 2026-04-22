/*
 * Scrollable three-heads summary UI — v0.5.0.
 *
 * Three full-width vertical sections (Detection, Benchmarks, System
 * Verdicts), each headed by a Cerberus dog head in CP437 block art
 * echoing the intro.c emblem at section-header scale. The mythology
 * is now literal: three heads, each guarding a different domain of
 * what the tool reports.
 *
 * Rendering model: a single virtual-row table (vrows[]) holds one
 * entry per logical content row (head-art row, horizontal rule, k/v
 * row, verdict row). The viewport displays 24 rows of that table
 * starting at scroll_top. No per-cell pre-render — each vrow is drawn
 * on demand from its type + data, so memory footprint is ~1 KB for
 * the vrow table, flat.
 *
 * Navigation via BIOS INT 16h (8088 compatible):
 *   Up / Down arrow    scroll one row
 *   PgUp / PgDn        scroll one viewport
 *   Home / End         jump to top / bottom
 *   Q or Esc           exit to DOS
 *
 * Row 24 hosts the status bar (position indicator + nav hints).
 * No information is truncated or dropped — a run with 100 verdicts
 * scrolls cleanly through 100 verdicts.
 *
 * Adapter compatibility: VRAM base selection same as v0.4
 * (B800 color / B000 mono). Head-art glyphs are CP437 primitives
 * that render identically on every adapter that renders the intro.
 * On MDA / Hercules the body color degrades to high-intensity white;
 * eyes and fangs stay bright.
 *
 * /NOUI path: ui_render_batch() prints a plain-text rendition of the
 * virtual rows to stdout without touching VRAM. Main.c dispatches
 * based on opts->no_ui. Preserves the escape hatch for real-iron
 * UI-render hangs (issue #3).
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include "ui.h"
#include "display.h"
#include "head_art.h"
#include "tui_util.h"      /* M3.1: tui_wait_cga_retrace_edge() */
#include "../core/report.h"

/* ----------------------------------------------------------------------- */
/* Geometry                                                                 */
/* ----------------------------------------------------------------------- */

#define VRAM_COLS       80
#define VRAM_ROWS       25
#define VIEWPORT_ROWS   24    /* rows 0..23 display content; row 24 = status */

/* Head art — 9 cols wide, 4 rows tall, positioned with a 3-col left margin
 * so there's room for the status-bar scroll-position glyph on the right. */
#define HEAD_LEFT_COL   3
#define HEAD_WIDTH      9
#define HEAD_HEIGHT     4
#define TITLE_COL       14    /* section title begins here, past the head */
#define KV_LABEL_COL    4     /* k/v label start */
#define KV_LABEL_WIDTH  14    /* wider than v0.4's 12 to fit "Conv memory" */
#define KV_VALUE_COL    (KV_LABEL_COL + KV_LABEL_WIDTH + 1)
#define KV_VALUE_MAX    (VRAM_COLS - KV_VALUE_COL - 2)

#define VERDICT_TAG_COL     4    /* "<TAG>" or "[TAG]" starts here */
#define VERDICT_LABEL_COL   11   /* display label starts here */
#define VERDICT_LABEL_WIDTH 20
#define VERDICT_NARR_COL    (VERDICT_LABEL_COL + VERDICT_LABEL_WIDTH + 1)
#define VERDICT_NARR_MAX    (VRAM_COLS - VERDICT_NARR_COL - 2)

/* Head-art glyphs centralized in src/core/head_art.c since v0.6.0 T0c. */

/* ----------------------------------------------------------------------- */
/* VRAM helpers                                                             */
/* ----------------------------------------------------------------------- */

static unsigned char __far *vram_base(void)
{
    adapter_t a = display_adapter();
    if (a == ADAPTER_MDA || a == ADAPTER_HERCULES ||
        a == ADAPTER_EGA_MONO || a == ADAPTER_VGA_MONO) {
        return (unsigned char __far *)MK_FP(0xB000, 0x0000);
    }
    return (unsigned char __far *)MK_FP(0xB800, 0x0000);
}

static int ui_is_mono(void)
{
    /* M3.5: unified via display_is_mono() so /MONO force flag
     * propagates through all rendering paths (this function and
     * tui_util's tui_is_mono both resolve to the same answer). */
    return display_is_mono();
}

static void vram_cursor(int row, int col)
{
    union REGS r;
    r.h.ah = 0x02;
    r.h.bh = 0;
    r.h.dh = (unsigned char)row;
    r.h.dl = (unsigned char)col;
    int86(0x10, &r, &r);
}

static void vram_putc(int row, int col, unsigned char ch, unsigned char attr)
{
    unsigned char __far *v = vram_base();
    unsigned int off = (unsigned int)((row * VRAM_COLS + col) * 2);
    /* M3.1: CGA snow gate. No-op on MDA/Hercules/EGA/VGA. */
    tui_wait_cga_retrace_edge();
    v[off]     = ch;
    v[off + 1] = attr;
}

static void vram_puts(int row, int col, const char *s, unsigned char attr)
{
    while (*s && col < VRAM_COLS) {
        vram_putc(row, col++, (unsigned char)*s++, attr);
    }
}

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

/* Mirror of report.c's format_result_value. Scratch buffer is static.
 * Return pointer is valid until the next call. Callers that compose
 * with the returned string must copy it out before re-calling. */
static const char *value_str(const result_t *r)
{
    static char scratch[32];
    if (!r) return "";
    if (r->display) return r->display;
    switch (r->type) {
        case V_STR:
            return r->v.s ? r->v.s : "";
        case V_U32:
            sprintf(scratch, "%lu", r->v.u);
            return scratch;
        case V_Q16:
            sprintf(scratch, "%ld.%04ld",
                    (long)(r->v.fixed >> 16),
                    (long)((r->v.fixed & 0xFFFFUL) * 10000UL >> 16));
            return scratch;
    }
    return "";
}

/* ----------------------------------------------------------------------- */
/* Head rendering                                                           */
/* ----------------------------------------------------------------------- */

/* Draw one row (0..3) of a directional Cerberus head starting at
 * HEAD_LEFT_COL on the given screen row. variant picks LEFT/CENTER/
 * RIGHT art from the shared head_art table. eye_attr drives eye color;
 * fang_attr drives fang color (typically both the same "accent" hue at
 * section-header scale — passed separately so the caller can hot-color
 * one without the other during effects). */
static void render_head_row(int screen_row, int head_row,
                            head_dir_t variant,
                            unsigned char body_attr,
                            unsigned char eye_attr,
                            unsigned char fang_attr)
{
    int c;
    if (head_row < 0 || head_row >= HEAD_ROWS) return;
    for (c = 0; c < HEAD_COLS; c++) {
        head_cell_t cell = head_art[variant][head_row][c];
        unsigned char attr;
        switch (cell.kind) {
            case HEAD_CELL_EYE:  attr = eye_attr;  break;
            case HEAD_CELL_FANG: attr = fang_attr; break;
            default:             attr = body_attr; break;
        }
        vram_putc(screen_row, HEAD_LEFT_COL + c, cell.glyph, attr);
    }
}

/* ----------------------------------------------------------------------- */
/* K/V row + verdict row renderers                                          */
/* ----------------------------------------------------------------------- */

/* Write val + optional CONF_LOW text marker into disp[], bounded by cap. */
static void compose_value(char *disp, int cap,
                          const char *val, int conf_low, int value_width)
{
    int vlen = (int)strlen(val);
    int vcap = vlen <= cap ? vlen : cap;
    int room = cap - vcap;
    const char *suffix = "";
    int slen = 0;
    int i, j;

    if (conf_low) {
        const char *primary  = " (low conf.)";
        const char *fallback = " (low)";
        int plen = (int)strlen(primary);
        int flen = (int)strlen(fallback);
        if (vlen + plen <= value_width) {
            suffix = primary;  slen = plen;
        } else {
            suffix = fallback; slen = flen;  /* may still trigger truncation */
        }
    }

    for (i = 0; i < vcap; i++) disp[i] = val[i];
    for (j = 0; j < slen && j < room; j++) disp[vcap + j] = suffix[j];
    disp[vcap + j] = '\0';
}

static void render_kv(int screen_row, const char *label, const result_t *r)
{
    const char *val = value_str(r);
    static char disp[64];
    int total, i;

    vram_puts(screen_row, KV_LABEL_COL, label, ATTR_NORMAL);
    compose_value(disp, (int)sizeof(disp) - 1, val,
                  r->confidence == CONF_LOW, KV_VALUE_MAX);
    total = (int)strlen(disp);
    if (total > KV_VALUE_MAX) {
        for (i = 0; i < KV_VALUE_MAX - 1; i++) {
            vram_putc(screen_row, KV_VALUE_COL + i,
                      (unsigned char)disp[i], ATTR_BOLD);
        }
        vram_putc(screen_row, KV_VALUE_COL + KV_VALUE_MAX - 1,
                  '.', ATTR_DIM);
    } else {
        for (i = 0; i < total; i++) {
            vram_putc(screen_row, KV_VALUE_COL + i,
                      (unsigned char)disp[i], ATTR_BOLD);
        }
    }
}

/* Verdict row support — same tag/attr/label tables as v0.4.1. */

typedef struct {
    const char *stripped_key;
    const char *display;
} verdict_label_t;

static const verdict_label_t verdict_labels[] = {
    /* diagnose heads */
    { "cache.status",        "Cache status" },
    { "dma.summary",         "DMA summary" },
    /* consistency rules */
    { "timing_self_check",   "Timing self-check" },
    { "timing_independence", "Timing independence" },
    { "486dx_fpu",           "486DX implies FPU" },
    { "486sx_fpu",           "486SX no FPU" },
    { "386sx_bus",           "386SX implies ISA-16" },
    { "8086_bus",            "8086 on ISA-8" },
    { "fpu_diag_bench",      "FPU diag+bench" },
    { "extmem_cpu",          "Ext mem vs CPU" },
    { "cpu_ipc_bench",       "CPU IPC bench" },
    { "audio_mixer_chip",    "Audio mixer chip" },
    { "whetstone_fpu",       "Whetstone FPU" },
    { "dma_class_coherence", "DMA class coherence" }
};
#define VERDICT_LABELS_COUNT (sizeof(verdict_labels) / sizeof(verdict_labels[0]))

static const char *verdict_display_label(const char *stripped_key)
{
    unsigned int i;
    for (i = 0; i < VERDICT_LABELS_COUNT; i++) {
        if (strcmp(verdict_labels[i].stripped_key, stripped_key) == 0) {
            return verdict_labels[i].display;
        }
    }
    return stripped_key;
}

static unsigned char verdict_attr(verdict_t v)
{
    switch (v) {
        case VERDICT_PASS: return 0x0A;  /* bright green */
        case VERDICT_WARN: return 0x0E;  /* bright yellow */
        case VERDICT_FAIL: return 0x0C;  /* bright red */
        default:           return 0x07;
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

/* Strip prefix, pick bracket style. Returns open/close bracket chars via
 * out parameters; returns the stripped key string. */
static const char *verdict_strip_key(const char *key,
                                     unsigned char *open_br,
                                     unsigned char *close_br)
{
    if (strncmp(key, "consistency.", 12) == 0) {
        *open_br  = '[';
        *close_br = ']';
        return key + 12;
    }
    if (strncmp(key, "diagnose.", 9) == 0) {
        *open_br  = '<';
        *close_br = '>';
        return key + 9;
    }
    *open_br  = '[';
    *close_br = ']';
    return key;
}

/* Strip the "pass (" / "WARN: " / "FAIL: " / "pass " prefix from the
 * narration text so it doesn't duplicate the tag column. Returns a
 * pointer into the original string. Also sets *strip_trailing_paren to 1
 * if we matched the "pass (" form so the caller can drop the closing ')'. */
static const char *narration_strip_prefix(const char *v, int *strip_trailing)
{
    *strip_trailing = 0;
    if (strncmp(v, "pass (", 6) == 0) { *strip_trailing = 1; return v + 6; }
    if (strncmp(v, "WARN: ", 6) == 0) return v + 6;
    if (strncmp(v, "FAIL: ", 6) == 0) return v + 6;
    if (strncmp(v, "WARN (", 6) == 0) { *strip_trailing = 1; return v + 6; }
    if (strncmp(v, "pass ",  5) == 0) return v + 5;
    return v;
}

static void render_verdict(int screen_row, const result_t *r)
{
    const char *val = value_str(r);
    const char *stripped;
    const char *label;
    const char *narr;
    int strip_trailing = 0;
    unsigned char va = verdict_attr(r->verdict);
    unsigned char open_br, close_br;
    const char *tag = verdict_tag(r->verdict);
    int i;

    stripped = verdict_strip_key(r->key, &open_br, &close_br);
    label    = verdict_display_label(stripped);

    /* Bracket + tag */
    vram_putc(screen_row, VERDICT_TAG_COL,     open_br,  ATTR_NORMAL);
    vram_putc(screen_row, VERDICT_TAG_COL + 1, (unsigned char)tag[0], va);
    vram_putc(screen_row, VERDICT_TAG_COL + 2, (unsigned char)tag[1], va);
    vram_putc(screen_row, VERDICT_TAG_COL + 3, (unsigned char)tag[2], va);
    vram_putc(screen_row, VERDICT_TAG_COL + 4, (unsigned char)tag[3], va);
    vram_putc(screen_row, VERDICT_TAG_COL + 5, close_br, ATTR_NORMAL);

    /* Display label (bold, max 20 chars) */
    {
        int c = VERDICT_LABEL_COL;
        int klen = (int)strlen(label);
        if (klen > VERDICT_LABEL_WIDTH) klen = VERDICT_LABEL_WIDTH;
        for (i = 0; i < klen; i++) {
            vram_putc(screen_row, c++, (unsigned char)label[i], ATTR_BOLD);
        }
    }

    /* Narration — now runs to cols up through 77 (VERDICT_NARR_MAX chars).
     * Truncation indicator still supported as a safety net, but at the new
     * width few narrations will reach it. */
    narr = narration_strip_prefix(val, &strip_trailing);
    {
        int c = VERDICT_NARR_COL;
        int maxcol = VERDICT_NARR_COL + VERDICT_NARR_MAX;
        while (*narr && c < maxcol) {
            if (strip_trailing && *narr == ')' && narr[1] == '\0') break;
            vram_putc(screen_row, c++, (unsigned char)*narr++, va);
        }
        if (*narr && !(strip_trailing && *narr == ')' && narr[1] == '\0')) {
            vram_putc(screen_row, maxcol, (unsigned char)0xAF, ATTR_DIM);
        }
    }
}

/* ----------------------------------------------------------------------- */
/* Display-row tables                                                       */
/* ----------------------------------------------------------------------- */

typedef struct {
    const char *key;
    const char *label;
} display_row_t;

/* DETECTION. Labels unchanged from v0.4.1; the 14-wide label column in
 * this layout means all labels fit with comfortable room. */
static const display_row_t detect_rows[] = {
    { "environment.emulator",      "Emulator" },
    { "cpu.detected",              "CPU" },
    { "cpu.family_model_stepping", "  F/M/Step" },
    { "fpu.friendly",              "FPU" },
    { "memory.conventional_kb",    "Conv memory" },
    { "memory.extended_kb",        "Ext memory" },
    { "cache.present",             "Cache" },
    { "bus.class",                 "Bus" },
    { "video.adapter",             "Video" },
    { "video.chipset",             "  Chipset" },
    { "audio.detected",            "Audio" },
    { "bios.family",               "BIOS" },
    { "bios.date",                 "  Date" }
};
#define DETECT_ROWS_COUNT (sizeof(detect_rows) / sizeof(detect_rows[0]))

/* BENCHMARKS. Grouped by subsystem: CPU/FPU raw values first, then
 * memory raw values, then the PC-XT ratio block. CONF_LOW rows get the
 * " (low conf.)" marker automatically via render_kv. */
static const display_row_t bench_rows[] = {
    /* CPU + FPU raw values */
    { "bench.cpu.int_iters_per_sec", "CPU int/s"   },
    { "bench.cpu.dhrystones",        "Dhrystones"  },
    { "bench.fpu.ops_per_sec",       "FPU ops/s"   },
    { "bench.fpu.k_whetstones",      "K-Whet"      },
    /* Memory raw values */
    { "bench.memory.write_kbps",     "Mem write"   },
    { "bench.memory.read_kbps",      "Mem read"    },
    { "bench.memory.copy_kbps",      "Mem copy"    },
    /* PC-XT ratios */
    { "bench.cpu_xt_factor",         "x PC-XT CPU" },
    { "bench.mem_xt_factor",         "x PC-XT mem" },
    { "bench.fpu_xt_factor",         "x PC-XT FPU" }
};
#define BENCH_ROWS_COUNT (sizeof(bench_rows) / sizeof(bench_rows[0]))

/* True if this result row contributes a meaningful verdict to the pane. */
static int is_verdict_row(const result_t *r)
{
    if (r->verdict == VERDICT_UNKNOWN) return 0;
    if (strncmp(r->key, "consistency.", 12) == 0) return 1;
    if (strcmp(r->key, "diagnose.cache.status") == 0) return 1;
    if (strcmp(r->key, "diagnose.dma.summary") == 0) return 1;
    return 0;
}

/* ----------------------------------------------------------------------- */
/* Virtual row model                                                        */
/* ----------------------------------------------------------------------- */

typedef enum {
    VROW_BLANK = 0,
    VROW_HEAD,       /* head-art row, maybe with section title */
    VROW_HLINE,      /* horizontal rule below a section head */
    VROW_KV,         /* label + result_t */
    VROW_VERDICT     /* verdict row */
} vrow_type_t;

typedef struct {
    vrow_type_t type;
    union {
        struct {
            unsigned char head_row;  /* 0..3 */
            unsigned char variant;   /* head_dir_t — LEFT/CENTER/RIGHT */
            const char *title;       /* non-NULL on the title row only */
        } head;
        struct {
            const char *label;
            const result_t *r;
        } kv;
        struct {
            const result_t *r;
        } verdict;
    } u;
} vrow_t;

#define MAX_VROWS 80
static vrow_t vrows[MAX_VROWS];
static int    vrow_count;

static void vrow_push(vrow_t v)
{
    if (vrow_count < MAX_VROWS) vrows[vrow_count++] = v;
}

static void vrow_blank(void)
{
    vrow_t v;
    v.type = VROW_BLANK;
    vrow_push(v);
}

static void vrow_head(int head_row, head_dir_t variant, const char *title)
{
    vrow_t v;
    v.type = VROW_HEAD;
    v.u.head.head_row = (unsigned char)head_row;
    v.u.head.variant  = (unsigned char)variant;
    v.u.head.title    = title;
    vrow_push(v);
}

static void vrow_hline(void)
{
    vrow_t v;
    v.type = VROW_HLINE;
    vrow_push(v);
}

static void vrow_kv(const char *label, const result_t *r)
{
    vrow_t v;
    v.type = VROW_KV;
    v.u.kv.label = label;
    v.u.kv.r     = r;
    vrow_push(v);
}

static void vrow_verdict(const result_t *r)
{
    vrow_t v;
    v.type = VROW_VERDICT;
    v.u.verdict.r = r;
    vrow_push(v);
}

/* Build one section: head art (4 rows, title on row 1) + hline + rows.
 * variant picks the directional head: LEFT guards Detection (scans
 * the hardware landscape), CENTER guards Benchmarks (measures head-on),
 * RIGHT guards Verdicts (judges outcomes). */
static void build_section_head(head_dir_t variant, const char *title)
{
    vrow_head(0, variant, (const char *)0);
    vrow_head(1, variant, title);
    vrow_head(2, variant, (const char *)0);
    vrow_head(3, variant, (const char *)0);
    vrow_hline();
}

static void build_section_detection(const result_table_t *t)
{
    unsigned int i;
    build_section_head(HEAD_LEFT, "DETECTION");
    for (i = 0; i < DETECT_ROWS_COUNT; i++) {
        const result_t *r = find_key(t, detect_rows[i].key);
        if (r) vrow_kv(detect_rows[i].label, r);
    }
}

static void build_section_benchmarks(const result_table_t *t)
{
    unsigned int i;
    build_section_head(HEAD_CENTER, "BENCHMARKS");
    for (i = 0; i < BENCH_ROWS_COUNT; i++) {
        const result_t *r = find_key(t, bench_rows[i].key);
        if (r) vrow_kv(bench_rows[i].label, r);
    }
}

static void build_section_verdicts(const result_table_t *t)
{
    unsigned int i;
    build_section_head(HEAD_RIGHT, "SYSTEM VERDICTS");
    for (i = 0; i < t->count; i++) {
        const result_t *r = &t->results[i];
        if (is_verdict_row(r)) vrow_verdict(r);
    }
}

/* v0.7.0: UPLOAD STATUS section. Uses HEAD_CENTER (forward-facing,
 * "sending data outward" fits the direction metaphor). Each row is a
 * plain k/v pointing at a network.* or upload.* result. */
static const display_row_t upload_status_rows[] = {
    { "network.transport",      "Network"     },
    { "upload.status",          "Status"      },
    { "upload.submission_id",   "Submission"  },
    { "upload.url",             "URL"         },
    { "upload.nickname",        "Nickname"    },
    { "upload.notes",           "Notes"       }
};
#define UPLOAD_STATUS_ROWS_COUNT \
    (sizeof(upload_status_rows) / sizeof(upload_status_rows[0]))

static void build_section_upload(const result_table_t *t)
{
    unsigned int i;
    build_section_head(HEAD_CENTER, "UPLOAD STATUS");
    for (i = 0; i < UPLOAD_STATUS_ROWS_COUNT; i++) {
        const result_t *r = find_key(t, upload_status_rows[i].key);
        /* Skip rows with absent / empty values so the pane doesn't
         * clutter on offline runs (no Submission / URL if we didn't
         * upload). Always-present rows (Network, Status) still render
         * since detect_network always emits transport and upload
         * always sets status. */
        if (!r) continue;
        if (r->type == V_STR && (!r->v.s || r->v.s[0] == '\0')) continue;
        vrow_kv(upload_status_rows[i].label, r);
    }
}

static void build_all_vrows(const result_table_t *t)
{
    vrow_count = 0;
    build_section_detection(t);
    vrow_blank();
    build_section_benchmarks(t);
    vrow_blank();
    build_section_verdicts(t);
    vrow_blank();
    build_section_upload(t);
}

/* ----------------------------------------------------------------------- */
/* Viewport rendering                                                       */
/* ----------------------------------------------------------------------- */

static void render_vrow_to_screen(int screen_row, const vrow_t *v)
{
    unsigned char body_attr, eye_attr;
    if (ui_is_mono()) {
        body_attr = ATTR_BOLD;
        eye_attr  = ATTR_BOLD;
    } else {
        body_attr = ATTR_CYAN;
        eye_attr  = ATTR_YELLOW;
    }

    switch (v->type) {
    case VROW_BLANK:
        break;  /* already cleared */
    case VROW_HEAD:
        render_head_row(screen_row, v->u.head.head_row,
                        (head_dir_t)v->u.head.variant,
                        body_attr, eye_attr, eye_attr);
        if (v->u.head.title) {
            vram_puts(screen_row, TITLE_COL, v->u.head.title, ATTR_BOLD);
        }
        break;
    case VROW_HLINE:
        vram_hline_double(screen_row, HEAD_LEFT_COL,
                          VRAM_COLS - 2, ATTR_NORMAL);
        break;
    case VROW_KV:
        if (v->u.kv.r) render_kv(screen_row, v->u.kv.label, v->u.kv.r);
        break;
    case VROW_VERDICT:
        if (v->u.verdict.r) render_verdict(screen_row, v->u.verdict.r);
        break;
    }
}

static void render_viewport(int scroll_top)
{
    int i;
    vram_fill(0, VIEWPORT_ROWS - 1, ' ', ATTR_NORMAL);
    for (i = 0; i < VIEWPORT_ROWS; i++) {
        int vidx = scroll_top + i;
        if (vidx >= vrow_count) break;
        render_vrow_to_screen(i, &vrows[vidx]);
    }
}

/* Status bar shows scroll position + nav hints. Rendered in reverse-video
 * so it stands apart from the content area above. */
/* M3.2: Norton-style F-key legend on row 24, Borland TStatusLine
 * palette. Replaces the v0.7.x status-bar rendering per 0.8.0 plan §9
 * CUA-lite decision.
 *
 * Color attributes:
 *   0x30  base   (black on cyan) — labels, padding, position indicator
 *   0x3F  hotkey (bright white on cyan) — F-number digits
 *
 * Mono attributes per MS-DOS UI-UX research Tier 0:
 *   0x70  reverse video (MDA-valid; used for base and hotkey both —
 *         mono has no color contrast to distinguish digit from label).
 *
 * Layout (80 cols, 0-indexed):
 *   col 1   '1' F1 hotkey digit
 *   col 2-5 "Help"
 *   col 8   '3' F3 hotkey digit
 *   col 9-12 "Exit"
 *   col 15-19 "Up/Dn"
 *   col 22-28 "PgUp/Dn"
 *   col 31-38 "Home/End"
 *   right end "rows X-Y of N" position indicator, collision-guarded
 *             to not overlap Home/End
 *   col 79    trailing space
 *
 * Esc and Q remain exit keys (legacy + CUA Esc); the legend advertises
 * F3 because it's CUA-canonical and the one-digit-wide label Norton
 * users recognize. */
static void render_legend(int scroll_top, int fits_in_one_page)
{
    static char pos_str[32];
    unsigned char base_attr;
    unsigned char hotkey_attr;
    int end_row = scroll_top + VIEWPORT_ROWS;
    int c;

    if (end_row > vrow_count) end_row = vrow_count;

    if (display_is_mono()) {
        base_attr   = ATTR_INVERSE;
        hotkey_attr = ATTR_INVERSE;
    } else {
        base_attr   = 0x30;
        hotkey_attr = 0x3F;
    }

    for (c = 0; c < VRAM_COLS; c++) {
        vram_putc(VIEWPORT_ROWS, c, ' ', base_attr);
    }

    vram_putc(VIEWPORT_ROWS, 1, '1', hotkey_attr);
    vram_puts(VIEWPORT_ROWS, 2, "Help", base_attr);
    vram_putc(VIEWPORT_ROWS, 8, '3', hotkey_attr);
    vram_puts(VIEWPORT_ROWS, 9, "Exit", base_attr);
    vram_puts(VIEWPORT_ROWS, 15, "Up/Dn",    base_attr);
    vram_puts(VIEWPORT_ROWS, 22, "PgUp/Dn",  base_attr);
    vram_puts(VIEWPORT_ROWS, 31, "Home/End", base_attr);

    if (fits_in_one_page) {
        sprintf(pos_str, "%d rows total", vrow_count);
    } else {
        sprintf(pos_str, "rows %d-%d of %d",
                scroll_top + 1, end_row, vrow_count);
    }
    {
        int len = (int)strlen(pos_str);
        int start = VRAM_COLS - len - 1;
        if (start < 45) start = 45;  /* collision-guard vs Home/End */
        vram_puts(VIEWPORT_ROWS, start, pos_str, base_attr);
    }
}

/* M3.3: F1 help overlay. Reuses the viewport area to display static
 * navigation and command reference. Any keypress restores the
 * scrollable summary (caller re-renders viewport + legend after
 * this returns). No modal-window chrome — deliberately minimal to
 * match the M3 "interaction grammar, not architecture" scope. */
static void render_help_overlay(void)
{
    static const char *lines[] = {
        "",
        "  CERBERUS help",
        "",
        "  Navigation:",
        "    Up / Down arrow       scroll one line",
        "    Page Up / Page Down   scroll one viewport",
        "    Home / End            jump to top / bottom",
        "",
        "  Commands:",
        "    F1                    this help screen",
        "    F3   or   Esc         exit to DOS",
        "    Q                     exit (legacy alias)",
        "",
        "  Version: " CERBERUS_VERSION,
        "",
        "  Build variants: wmake (stock), wmake WHETSTONE=1, wmake UPLOAD=1",
        "",
        "  Press any key to close this help screen.",
    };
    unsigned int n = sizeof(lines) / sizeof(lines[0]);
    unsigned int i;
    unsigned char base_attr;
    int c;

    base_attr = display_is_mono() ? ATTR_INVERSE : 0x30;

    vram_fill(0, VIEWPORT_ROWS - 1, ' ', ATTR_NORMAL);
    for (i = 0; i < n && (int)i < VIEWPORT_ROWS; i++) {
        vram_puts((int)i, 0, lines[i], ATTR_NORMAL);
    }

    for (c = 0; c < VRAM_COLS; c++) {
        vram_putc(VIEWPORT_ROWS, c, ' ', base_attr);
    }
    vram_puts(VIEWPORT_ROWS, 1, "Press any key to close help", base_attr);

    {
        union REGS r;
        r.h.ah = 0x00;
        int86(0x16, &r, &r);
    }
}

/* ----------------------------------------------------------------------- */
/* Keyboard navigation                                                      */
/* ----------------------------------------------------------------------- */

typedef enum {
    NAV_UP, NAV_DOWN, NAV_PGUP, NAV_PGDN, NAV_HOME, NAV_END,
    NAV_EXIT, NAV_HELP, NAV_OTHER
} nav_key_t;

/* M3.4: CUA-lite key dispatch. Scan codes used:
 *   F1 = 0x3B      help overlay
 *   F3 = 0x3D      exit (CUA canonical; alias for Esc/Q)
 *   Esc = 0x1B     exit (CUA canonical cancel; ASCII path)
 *   Q/q = 0x71/51  exit (legacy alias, predates F3 wiring)
 *   arrows, PgUp/Dn, Home/End: standard extended scan codes
 */
static nav_key_t read_nav_key(void)
{
    union REGS r;
    r.h.ah = 0x00;  /* blocking read */
    int86(0x16, &r, &r);
    if (r.h.al == 0) {
        /* extended key, AH is scan code */
        switch (r.h.ah) {
        case 0x3B: return NAV_HELP;  /* F1 */
        case 0x3D: return NAV_EXIT;  /* F3 (CUA) */
        case 0x48: return NAV_UP;
        case 0x50: return NAV_DOWN;
        case 0x49: return NAV_PGUP;
        case 0x51: return NAV_PGDN;
        case 0x47: return NAV_HOME;
        case 0x4F: return NAV_END;
        }
        return NAV_OTHER;
    }
    switch (r.h.al) {
    case 'q': case 'Q': case 0x1B:
        return NAV_EXIT;
    }
    return NAV_OTHER;
}

/* ----------------------------------------------------------------------- */
/* Public entry points                                                      */
/* ----------------------------------------------------------------------- */

/* Clear the full 25-row text screen to ATTR_NORMAL and park the cursor
 * at (0,0). Called before returning to DOS so the TUI's reverse-video
 * status bar and any other non-default attributes don't bleed into the
 * DOS prompt (UX item #7 from the 2026-04-21 real-iron review: the
 * grey status-bar row persisted after Q and colored subsequent DOS
 * output). The all-rows fill is deliberate: rendering the content
 * cleared rows 0..23 but row 24 retained ATTR_INVERSE from the status
 * bar; a post-exit DOS prompt landing on row 24 inherited that. */
static void reset_screen_to_dos(void)
{
    vram_fill(0, VRAM_ROWS - 1, ' ', ATTR_NORMAL);
    vram_cursor(0, 0);
}

void ui_render_summary(const result_table_t *t, const opts_t *o)
{
    int scroll_top = 0;
    int max_scroll;
    int fits;
    (void)o;

    build_all_vrows(t);

    fits = (vrow_count <= VIEWPORT_ROWS);
    max_scroll = fits ? 0 : vrow_count - VIEWPORT_ROWS;

    for (;;) {
        render_viewport(scroll_top);
        render_legend(scroll_top, fits);
        switch (read_nav_key()) {
        case NAV_UP:
            if (!fits && scroll_top > 0) scroll_top--;
            break;
        case NAV_DOWN:
            if (!fits && scroll_top < max_scroll) scroll_top++;
            break;
        case NAV_PGUP:
            if (!fits) {
                scroll_top -= VIEWPORT_ROWS;
                if (scroll_top < 0) scroll_top = 0;
            }
            break;
        case NAV_PGDN:
            if (!fits) {
                scroll_top += VIEWPORT_ROWS;
                if (scroll_top > max_scroll) scroll_top = max_scroll;
            }
            break;
        case NAV_HOME:
            if (!fits) scroll_top = 0;
            break;
        case NAV_END:
            if (!fits) scroll_top = max_scroll;
            break;
        case NAV_HELP:
            render_help_overlay();
            /* next loop iteration re-renders viewport + legend */
            break;
        case NAV_EXIT:
            reset_screen_to_dos();
            return;
        case NAV_OTHER:
            /* On fits-in-one-page mode, any non-handled key exits —
             * preserves v0.7.x "any key" convenience for static summaries
             * while still giving F1 help and proper exit keys. */
            if (fits) {
                reset_screen_to_dos();
                return;
            }
            break;
        }
    }
}

/* The consistency alerts are now rendered inline as the third section of
 * ui_render_summary. This entry point is preserved as a no-op for ABI
 * compatibility with anything that calls both functions in sequence. */
void ui_render_consistency_alerts(const result_table_t *t)
{
    (void)t;
}

/* /NOUI batch mode: print the virtual rows as plain text to stdout without
 * touching VRAM. Same content as the interactive renderer; same visual
 * contract (section titles, k/v pairs, verdict brackets) in ASCII. */
void ui_render_batch(const result_table_t *t)
{
    int i;
    build_all_vrows(t);

    for (i = 0; i < vrow_count; i++) {
        const vrow_t *v = &vrows[i];
        switch (v->type) {
        case VROW_BLANK:
            printf("\n");
            break;
        case VROW_HEAD:
            if (v->u.head.title) printf("\n=== %s ===\n", v->u.head.title);
            break;
        case VROW_HLINE:
            break;  /* visual-only separator suppressed in batch */
        case VROW_KV:
            if (v->u.kv.r) {
                const char *val = value_str(v->u.kv.r);
                printf("  %-14s %s%s\n",
                       v->u.kv.label, val,
                       v->u.kv.r->confidence == CONF_LOW ? " (low conf.)" : "");
            }
            break;
        case VROW_VERDICT:
            if (v->u.verdict.r) {
                const result_t *r = v->u.verdict.r;
                unsigned char ob, cb;
                const char *stripped = verdict_strip_key(r->key, &ob, &cb);
                const char *label    = verdict_display_label(stripped);
                int strip_trailing = 0;
                const char *narr = narration_strip_prefix(value_str(r),
                                                          &strip_trailing);
                printf("  %c%s%c %-20s %s\n",
                       ob, verdict_tag(r->verdict), cb, label, narr);
            }
            break;
        }
    }
}
