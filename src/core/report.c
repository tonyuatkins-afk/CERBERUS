#include <stdio.h>
#include <string.h>
#include "report.h"

static void push(result_table_t *t, const char *key, value_type_t type,
                 const char *display, confidence_t conf, verdict_t v)
{
    result_t *r;
    if (t->count >= MAX_RESULTS) return;
    r = &t->results[t->count++];
    r->key = key;
    r->type = type;
    r->display = display;
    r->confidence = conf;
    r->verdict = v;
}

void report_add_str(result_table_t *t, const char *key, const char *value,
                    confidence_t conf, verdict_t verdict)
{
    result_t *r;
    if (t->count >= MAX_RESULTS) return;
    r = &t->results[t->count];
    push(t, key, V_STR, value, conf, verdict);
    r->v.s = value;
}

void report_add_u32(result_table_t *t, const char *key, unsigned long value,
                    const char *display, confidence_t conf, verdict_t verdict)
{
    result_t *r;
    if (t->count >= MAX_RESULTS) return;
    r = &t->results[t->count];
    push(t, key, V_U32, display, conf, verdict);
    r->v.u = value;
}

void report_add_q16(result_table_t *t, const char *key, long fixed,
                    const char *display, confidence_t conf, verdict_t verdict)
{
    result_t *r;
    if (t->count >= MAX_RESULTS) return;
    r = &t->results[t->count];
    push(t, key, V_Q16, display, conf, verdict);
    r->v.fixed = fixed;
}

int report_write_ini(const result_table_t *t, const opts_t *o, const char *path)
{
    FILE *f = fopen(path, "wt");
    if (!f) return -1;
    fprintf(f, "[cerberus]\n");
    fprintf(f, "version=%s\n", CERBERUS_VERSION);
    fprintf(f, "schema_version=%s\n", CERBERUS_SCHEMA_VERSION);
    fprintf(f, "signature_schema=%s\n", CERBERUS_SIGNATURE_SCHEMA);
    fprintf(f, "mode=%s\n", o->mode == MODE_CALIBRATED ? "calibrated" : "quick");
    fprintf(f, "runs=%u\n", (unsigned)o->runs);
    fprintf(f, "signature=00000000\n");
    fprintf(f, "run_signature=0000000000000000\n");
    fprintf(f, "results=%u\n", t->count);
    /* Full section grouping and dual-signature computation land in Task 0.4 */
    fclose(f);
    return 0;
}

void report_hardware_signature(const result_table_t *t, char out_hex[9])
{
    /* Phase 0 stub — real SHA-1 in Task 0.4 */
    (void)t;
    strcpy(out_hex, "00000000");
}
