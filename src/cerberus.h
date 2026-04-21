#ifndef CERBERUS_H
#define CERBERUS_H

#define CERBERUS_VERSION          "0.7.0-rc1"
#define CERBERUS_SCHEMA_VERSION   "1.0"
#define CERBERUS_SIGNATURE_SCHEMA "1"
/* ini_format: integer, increments on INI breaking changes (key removal,
 * renaming, value-format change). Additive changes (new keys, new
 * sections) do NOT bump this. The server parser uses ini_format to
 * decide how to read a submission; it MUST be prepared for additive
 * changes within a format generation. v0.7.0: ini_format=1. */
#define CERBERUS_INI_FORMAT       "1"

typedef enum { MODE_QUICK = 0, MODE_CALIBRATED = 1 } run_mode_t;

typedef enum {
    CONF_LOW    = 0,
    CONF_MEDIUM = 1,
    CONF_HIGH   = 2
} confidence_t;

typedef enum {
    VERDICT_UNKNOWN = 0,
    VERDICT_PASS    = 1,
    VERDICT_WARN    = 2,
    VERDICT_FAIL    = 3
} verdict_t;

typedef enum {
    V_STR  = 0,
    V_U32  = 1,
    V_Q16  = 2
} value_type_t;

typedef struct {
    const char  *key;
    value_type_t type;
    union {
        const char   *s;
        unsigned long u;
        long          fixed;
    } v;
    const char  *display;
    confidence_t confidence;
    verdict_t    verdict;
} result_t;

#define MAX_RESULTS 256

typedef struct {
    result_t     results[MAX_RESULTS];
    unsigned int count;
} result_table_t;

#define Q16_FROM_INT(i) ((long)(i) << 16)
#define Q16_TO_INT(q)   ((int)((q) >> 16))

typedef struct {
    run_mode_t    mode;
    unsigned char runs;
    unsigned char do_detect;
    unsigned char do_diagnose;
    unsigned char do_benchmark;
    unsigned char do_upload;
    unsigned char no_cyrix;
    unsigned char no_intro;
    unsigned char no_ui;       /* /NOUI: skip ui_render_summary and
                                * ui_render_consistency_alerts. Escape
                                * hatch for real-iron UI-render hangs.
                                * INI + UNK are still written. */
    unsigned char do_quick;    /* /QUICK: skip all visual demonstrations
                                * and title cards. Measurements still
                                * run; summary still renders interactively.
                                * For batch users who want timings
                                * without the journey. v0.6.0. */
    unsigned char no_upload;   /* /NOUPLOAD: never prompt, never upload.
                                * v0.7.0. */
    /* do_upload was the pre-v0.7.0 "upload?" flag; now reused as
     * /UPLOAD auto-yes (skip the prompt, upload immediately). */
    char          out_path[64];
    /* v0.7.0 upload metadata from /NICK and /NOTE flags. Both empty
     * strings by default (anonymous submission). */
    char          nickname[33];
    char          note[129];
} opts_t;

#define EXIT_OK          0
#define EXIT_USAGE       1
#define EXIT_UPLOAD_FAIL 2
#define EXIT_HW_HANG     3

#endif
