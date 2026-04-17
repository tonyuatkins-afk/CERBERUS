/*
 * CERBERUS INI report writer with dual signatures.
 *
 * Two signatures are emitted in [cerberus]:
 *
 *   signature      — HARDWARE IDENTITY. SHA-1 (first 8 hex) over a frozen
 *                    7-key canonical subset at CONF_HIGH only. MEDIUM/LOW
 *                    values serialize as "key=unknown" so later detection
 *                    improvements do not retroactively change the identity
 *                    of historical submissions. signature_schema=1 locks
 *                    these rules; bump only when the canonical rules change.
 *
 *   run_signature  — RECORD IDENTITY. SHA-1 (first 16 hex) over the full
 *                    INI contents excluding the run_signature line itself.
 *                    Same hardware, different run → different run_signature,
 *                    which lets the consistency engine spot divergent
 *                    results from what the hardware *claims* to be the same
 *                    system (counterfeit-CPU scenario).
 *
 * Build strategy: serialize everything EXCEPT the run_signature line into
 * an in-memory buffer, compute run_signature over that buffer, then write
 * the buffer to disk followed by the run_signature line. No two-pass
 * re-read needed and no DOS file-buffering surprises.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "report.h"
#include "sha1.h"

/* ----------------------------------------------------------------------- */
/* Result table builders                                                    */
/* ----------------------------------------------------------------------- */

static result_t *push_slot(result_table_t *t, const char *key, value_type_t type,
                           const char *display, confidence_t conf, verdict_t v)
{
    result_t *r;
    if (t->count >= MAX_RESULTS) return (result_t *)0;
    r = &t->results[t->count++];
    r->key        = key;
    r->type       = type;
    r->display    = display;
    r->confidence = conf;
    r->verdict    = v;
    return r;
}

void report_add_str(result_table_t *t, const char *key, const char *value,
                    confidence_t conf, verdict_t verdict)
{
    result_t *r = push_slot(t, key, V_STR, value, conf, verdict);
    if (r) r->v.s = value;
}

void report_add_u32(result_table_t *t, const char *key, unsigned long value,
                    const char *display, confidence_t conf, verdict_t verdict)
{
    result_t *r = push_slot(t, key, V_U32, display, conf, verdict);
    if (r) r->v.u = value;
}

void report_add_q16(result_table_t *t, const char *key, long fixed,
                    const char *display, confidence_t conf, verdict_t verdict)
{
    result_t *r = push_slot(t, key, V_Q16, display, conf, verdict);
    if (r) r->v.fixed = fixed;
}

/* ----------------------------------------------------------------------- */
/* Hardware-identity signature (frozen canonical subset)                    */
/* ----------------------------------------------------------------------- */

/* Canonical keys in FIXED ORDER. Changing this list bumps signature_schema. */
static const char *const canonical_keys[] = {
    "cpu.detected",
    "cpu.class",
    "fpu.detected",
    "memory.conventional_kb",
    "memory.extended_kb",
    "video.adapter",
    "bus.class"
};
#define CANONICAL_KEY_COUNT (sizeof(canonical_keys) / sizeof(canonical_keys[0]))

static const result_t *find_result(const result_table_t *t, const char *key)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, key) == 0) {
            return &t->results[i];
        }
    }
    return (const result_t *)0;
}

void report_hardware_signature(const result_table_t *t, char out_hex[9])
{
    sha1_ctx_t ctx;
    unsigned char digest[SHA1_DIGEST_LEN];
    char full_hex[SHA1_HEX_LEN + 1];
    unsigned int i;

    sha1_init(&ctx);
    for (i = 0; i < CANONICAL_KEY_COUNT; i++) {
        const char *key = canonical_keys[i];
        const result_t *r = find_result(t, key);
        const char *value;

        sha1_update(&ctx, key, (unsigned int)strlen(key));
        sha1_update(&ctx, "=", 1);

        if (r && r->confidence == CONF_HIGH) {
            value = r->display ? r->display :
                    (r->type == V_STR ? r->v.s : "");
            if (!value) value = "";
        } else {
            value = "unknown";
        }
        sha1_update(&ctx, value, (unsigned int)strlen(value));
        sha1_update(&ctx, "\n", 1);
    }
    sha1_final(&ctx, digest);
    sha1_to_hex(digest, full_hex);
    memcpy(out_hex, full_hex, 8);
    out_hex[8] = '\0';
}

/* ----------------------------------------------------------------------- */
/* Section-aware INI serialization                                          */
/* ----------------------------------------------------------------------- */

/* Preferred section order — everything else sorts alphabetically after these. */
static const char *const section_order[] = {
    "cerberus",
    "environment",
    "cpu",
    "fpu",
    "memory",
    "cache",
    "bus",
    "video",
    "audio",
    "bios",
    "diagnose",
    "bench",
    "consistency"
};
#define SECTION_ORDER_COUNT (sizeof(section_order) / sizeof(section_order[0]))

/* Split a key "cpu.detected" into section "cpu" and subkey "detected".
 * If no dot, the whole key is the subkey and section is empty. */
static void split_key(const char *key, char *section, char *subkey, unsigned int cap)
{
    const char *dot = strchr(key, '.');
    unsigned int sec_len;
    if (!dot) {
        section[0] = '\0';
        strncpy(subkey, key, cap - 1);
        subkey[cap - 1] = '\0';
        return;
    }
    sec_len = (unsigned int)(dot - key);
    if (sec_len >= cap) sec_len = cap - 1;
    memcpy(section, key, sec_len);
    section[sec_len] = '\0';
    strncpy(subkey, dot + 1, cap - 1);
    subkey[cap - 1] = '\0';
}

static int section_index(const char *section)
{
    unsigned int i;
    for (i = 0; i < SECTION_ORDER_COUNT; i++) {
        if (strcmp(section, section_order[i]) == 0) {
            return (int)i;
        }
    }
    return (int)SECTION_ORDER_COUNT;  /* unknown sections sort after known */
}

/* INI build buffer — 8KB covers Phase 1 expected output comfortably.
 * DGROUP growth is acceptable under medium model (64KB data ceiling). */
#define INI_BUF_SIZE 8192
static char ini_buf[INI_BUF_SIZE];
static unsigned int ini_buf_used;

static void buf_reset(void) { ini_buf_used = 0; }

static int buf_write(const char *s)
{
    unsigned int n = (unsigned int)strlen(s);
    if (ini_buf_used + n >= INI_BUF_SIZE) return -1;
    memcpy(ini_buf + ini_buf_used, s, n);
    ini_buf_used += n;
    return 0;
}

static int buf_writef(const char *fmt, ...)
{
    /* sprintf into a scratch buffer then append; keeps the call sites clean */
    char tmp[192];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsprintf(tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return -1;
    if (ini_buf_used + (unsigned int)n >= INI_BUF_SIZE) return -1;
    memcpy(ini_buf + ini_buf_used, tmp, (unsigned int)n);
    ini_buf_used += (unsigned int)n;
    return 0;
}

static void format_result_value(const result_t *r, char *out, unsigned int cap)
{
    if (r->display) {
        strncpy(out, r->display, cap - 1);
        out[cap - 1] = '\0';
        return;
    }
    switch (r->type) {
        case V_STR:
            strncpy(out, r->v.s ? r->v.s : "", cap - 1);
            out[cap - 1] = '\0';
            break;
        case V_U32:
            sprintf(out, "%lu", r->v.u);
            break;
        case V_Q16:
            sprintf(out, "%ld.%04ld",
                    (long)(r->v.fixed >> 16),
                    (long)((r->v.fixed & 0xFFFFUL) * 10000UL >> 16));
            break;
    }
}

static void emit_cerberus_section(const result_table_t *t, const opts_t *o,
                                  const char *hw_sig)
{
    buf_write("[cerberus]\n");
    buf_writef("version=%s\n", CERBERUS_VERSION);
    buf_writef("schema_version=%s\n", CERBERUS_SCHEMA_VERSION);
    buf_writef("signature_schema=%s\n", CERBERUS_SIGNATURE_SCHEMA);
    buf_writef("mode=%s\n",
               o->mode == MODE_CALIBRATED ? "calibrated" : "quick");
    buf_writef("runs=%u\n", (unsigned int)o->runs);
    buf_writef("signature=%s\n", hw_sig);
    buf_writef("results=%u\n", t->count);
    buf_write("\n");
}

/* Emit every result whose section matches `target_section`, in table-
 * insertion order. Skips results with the "cerberus" section since we
 * write that section's fields manually in emit_cerberus_section. */
static void emit_section(const result_table_t *t, const char *target_section)
{
    unsigned int i;
    int wrote_header = 0;
    char section[32], subkey[128], value[192];
    for (i = 0; i < t->count; i++) {
        const result_t *r = &t->results[i];
        split_key(r->key, section, subkey, sizeof(section));
        if (strcmp(section, target_section) != 0) continue;
        if (strcmp(section, "cerberus") == 0) continue;  /* manual */
        if (!wrote_header) {
            buf_writef("[%s]\n", target_section);
            wrote_header = 1;
        }
        format_result_value(r, value, sizeof(value));
        buf_writef("%s=%s\n", subkey, value);
    }
    if (wrote_header) buf_write("\n");
}

int report_write_ini(const result_table_t *t, const opts_t *o, const char *path)
{
    char hw_sig[9];
    char run_sig_full[SHA1_HEX_LEN + 1];
    unsigned char digest[SHA1_DIGEST_LEN];
    FILE *f;
    unsigned int i;

    buf_reset();
    report_hardware_signature(t, hw_sig);

    emit_cerberus_section(t, o, hw_sig);

    /* Known sections in fixed order */
    for (i = 0; i < SECTION_ORDER_COUNT; i++) {
        if (strcmp(section_order[i], "cerberus") == 0) continue;
        emit_section(t, section_order[i]);
    }

    /* Any unknown sections — find unique section names in the table that
     * aren't in section_order, emit them in first-seen order.
     * (Belt-and-suspenders — future subsystems that forget to register
     * in section_order still serialize.) */
    {
        char seen[16][32];
        unsigned int seen_count = 0;
        for (i = 0; i < t->count; i++) {
            char section[32], subkey[128];
            unsigned int j;
            int already = 0;
            split_key(t->results[i].key, section, subkey, sizeof(section));
            if (section_index(section) != (int)SECTION_ORDER_COUNT) continue;
            for (j = 0; j < seen_count; j++) {
                if (strcmp(seen[j], section) == 0) { already = 1; break; }
            }
            if (already) continue;
            if (seen_count < 16) {
                strncpy(seen[seen_count], section, sizeof(seen[0]) - 1);
                seen[seen_count][sizeof(seen[0]) - 1] = '\0';
                seen_count++;
                emit_section(t, section);
            }
        }
    }

    /* Compute run_signature over the buffer contents (everything except
     * the run_signature line itself, which we append afterward). */
    sha1_hash(ini_buf, (unsigned long)ini_buf_used, digest);
    sha1_to_hex(digest, run_sig_full);

    f = fopen(path, "wt");
    if (!f) return -1;
    fwrite(ini_buf, 1, ini_buf_used, f);
    /* Emit run_signature as the last line. Use only the first 16 hex chars
     * to keep INI readable while preserving 64 bits of collision space. */
    run_sig_full[16] = '\0';
    fprintf(f, "run_signature=%s\n", run_sig_full);
    fclose(f);

    return 0;
}
