/*
 * Host-side unit test for the INI report writer and dual signatures.
 *
 * Strategy: populate a result_table with synthetic data, call
 * report_write_ini, read the output file back, and verify:
 *   1. Fixed section ordering
 *   2. [cerberus] section has version/schema_version/signature_schema
 *   3. run_signature line appears last
 *   4. Hardware signature is stable across runs with same inputs
 *   5. Changing a HIGH-confidence canonical key changes signature
 *   6. Changing a LOW-confidence canonical key does NOT change signature
 *   7. Changing a NON-canonical field changes run_signature but not signature
 */

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

static void populate_baseline(result_table_t *t)
{
    memset(t, 0, sizeof(*t));
    report_add_str(t, "cpu.detected",           "i486DX",     CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "cpu.class",              "486dx",      CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "cpu.vendor",             "Intel",      CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "fpu.detected",           "integrated", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "memory.conventional_kb", 640UL,        "640", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_u32(t, "memory.extended_kb",     15360UL,      "15360", CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "video.adapter",          "vga",        CONF_HIGH, VERDICT_UNKNOWN);
    report_add_str(t, "bus.class",              "vlb",        CONF_HIGH, VERDICT_UNKNOWN);
}

static void read_file(const char *path, char *out, unsigned int cap)
{
    FILE *f = fopen(path, "rt");
    unsigned int n;
    if (!f) { out[0] = '\0'; return; }
    n = (unsigned int)fread(out, 1, cap - 1, f);
    out[n] = '\0';
    fclose(f);
}

static void read_signature(const char *path, char *sig_out)
{
    FILE *f = fopen(path, "rt");
    char line[256];
    sig_out[0] = '\0';
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "signature=", 10) == 0 &&
            strncmp(line, "signature_schema=", 17) != 0) {
            /* Copy the 8-char hex signature, strip newline */
            strncpy(sig_out, line + 10, 8);
            sig_out[8] = '\0';
            break;
        }
    }
    fclose(f);
}

static void read_run_signature(const char *path, char *sig_out)
{
    FILE *f = fopen(path, "rt");
    char line[256];
    sig_out[0] = '\0';
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "run_signature=", 14) == 0) {
            strncpy(sig_out, line + 14, 16);
            sig_out[16] = '\0';
            break;
        }
    }
    fclose(f);
}

int main(void)
{
    result_table_t t;
    opts_t o;
    char buf[8192];
    char sig1[32], sig2[32], sig3[32];
    char run1[32], run2[32];

    memset(&o, 0, sizeof(o));
    o.mode = MODE_QUICK;
    o.runs = 1;
    strcpy(o.out_path, "test.ini");

    printf("=== CERBERUS host unit test: report / INI writer ===\n");

    /* Baseline write */
    populate_baseline(&t);
    CHECK(report_write_ini(&t, &o, "test_baseline.ini") == 0,
          "baseline write returns 0");
    read_file("test_baseline.ini", buf, sizeof(buf));

    /* Structure checks */
    CHECK(strstr(buf, "[cerberus]") == buf,
          "[cerberus] is first section");
    CHECK(strstr(buf, "\nversion=0.1.0\n") != NULL,
          "version field present");
    CHECK(strstr(buf, "\nschema_version=") != NULL,
          "schema_version field present");
    CHECK(strstr(buf, "\nsignature_schema=1\n") != NULL,
          "signature_schema=1");
    CHECK(strstr(buf, "\nsignature=") != NULL,
          "signature field present");
    CHECK(strstr(buf, "\n[cpu]") != NULL,
          "[cpu] section present");
    CHECK(strstr(buf, "\ndetected=i486DX\n") != NULL,
          "cpu.detected serialized without section prefix");
    CHECK(strstr(buf, "\n[memory]") != NULL,
          "[memory] section present");
    CHECK(strstr(buf, "\nconventional_kb=640\n") != NULL,
          "memory.conventional_kb serialized");
    CHECK(strstr(buf, "\nrun_signature=") != NULL,
          "run_signature present");

    /* Check ordering: [cpu] must appear before [memory] per section_order */
    {
        const char *cpu_at    = strstr(buf, "[cpu]");
        const char *memory_at = strstr(buf, "[memory]");
        const char *video_at  = strstr(buf, "[video]");
        CHECK(cpu_at && memory_at && cpu_at < memory_at,
              "cpu section precedes memory section");
        CHECK(memory_at && video_at && memory_at < video_at,
              "memory section precedes video section");
    }

    /* Signature stability: same inputs → same signature */
    populate_baseline(&t);
    report_write_ini(&t, &o, "test_1.ini");
    read_signature("test_1.ini", sig1);
    populate_baseline(&t);
    report_write_ini(&t, &o, "test_2.ini");
    read_signature("test_2.ini", sig2);
    CHECK(strcmp(sig1, sig2) == 0,
          "signature stable across runs with same inputs");

    /* Changing HIGH-confidence canonical field → signature changes */
    populate_baseline(&t);
    /* Find cpu.detected and replace its value */
    {
        unsigned int i;
        for (i = 0; i < t.count; i++) {
            if (strcmp(t.results[i].key, "cpu.detected") == 0) {
                t.results[i].v.s     = "Pentium";
                t.results[i].display = "Pentium";
            }
        }
    }
    report_write_ini(&t, &o, "test_3.ini");
    read_signature("test_3.ini", sig3);
    CHECK(strcmp(sig1, sig3) != 0,
          "signature changes when HIGH-confidence canonical field changes");

    /* Changing LOW-confidence canonical field → signature UNCHANGED */
    populate_baseline(&t);
    /* Downgrade cpu.detected to LOW confidence, change its value */
    {
        unsigned int i;
        for (i = 0; i < t.count; i++) {
            if (strcmp(t.results[i].key, "cpu.detected") == 0) {
                t.results[i].v.s        = "SurprisinglyWeird";
                t.results[i].display    = "SurprisinglyWeird";
                t.results[i].confidence = CONF_LOW;
            }
        }
    }
    report_write_ini(&t, &o, "test_4.ini");
    read_signature("test_4.ini", sig2);
    /* Also populate a fresh baseline where cpu.detected is LOW "unknown"-path */
    populate_baseline(&t);
    {
        unsigned int i;
        for (i = 0; i < t.count; i++) {
            if (strcmp(t.results[i].key, "cpu.detected") == 0) {
                t.results[i].confidence = CONF_LOW;
            }
        }
    }
    report_write_ini(&t, &o, "test_4b.ini");
    read_signature("test_4b.ini", sig1);
    CHECK(strcmp(sig1, sig2) == 0,
          "signature IGNORES value of LOW-confidence canonical field");

    /* Changing non-canonical field → signature unchanged, run_signature changes */
    populate_baseline(&t);
    report_write_ini(&t, &o, "test_5.ini");
    read_signature("test_5.ini", sig1);
    read_run_signature("test_5.ini", run1);
    populate_baseline(&t);
    report_add_str(&t, "bench.cpu.mips", "12.3", CONF_HIGH, VERDICT_UNKNOWN);
    report_write_ini(&t, &o, "test_6.ini");
    read_signature("test_6.ini", sig2);
    read_run_signature("test_6.ini", run2);
    CHECK(strcmp(sig1, sig2) == 0,
          "signature stable when non-canonical field added");
    CHECK(strcmp(run1, run2) != 0,
          "run_signature changes when non-canonical field added");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
