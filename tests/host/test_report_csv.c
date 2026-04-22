/*
 * Host-side test for report_write_csv (0.8.1 M1.2, research gap H).
 *
 * Covers:
 * - RFC 4180 field escaping: commas, quotes, newlines require quoting;
 *   embedded quotes double. Plain fields emit unquoted.
 * - Schema: header row "key,value,confidence,verdict" followed by one
 *   data row per emitted result in insertion order.
 * - Enum rendering: CONF_HIGH/MEDIUM/LOW; VERDICT_PASS/WARN/FAIL/UNK.
 * - Writes round-trip through the V_U32, V_Q16, V_STR display paths.
 */

#ifndef CERBERUS_HOST_TEST
#define CERBERUS_HOST_TEST
#endif

#include "../../src/core/sha1.c"
#include "../../src/core/report.c"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

/* Read a whole file to a static buffer. Returns length, or -1.
 * Text mode so CRLF -> LF normalization happens under the C runtime,
 * letting the test assertions use \n regardless of whether the writer
 * emitted LF or CRLF. The CSV file on disk is CRLF (RFC 4180 default)
 * since report_write_csv uses fopen(..., "wt"); readers of actual CSV
 * files on disk must accept either. */
static char file_buf[8192];
static int read_file(const char *path)
{
    FILE *f = fopen(path, "rt");
    size_t n;
    if (!f) return -1;
    n = fread(file_buf, 1, sizeof(file_buf) - 1, f);
    fclose(f);
    file_buf[n] = '\0';
    return (int)n;
}

static void test_basic_emit(void)
{
    result_table_t t;
    int n;
    const char *tmp = "tmp_csv_basic.csv";

    printf("\n[test_basic_emit]\n");
    memset(&t, 0, sizeof(t));

    report_add_str(&t, "cpu.detected", "Intel i486DX-2-66",
                   CONF_HIGH, VERDICT_PASS);
    report_add_u32(&t, "bench.cpu.int_iters_per_sec", 1964636UL, NULL,
                   CONF_HIGH, VERDICT_PASS);
    report_add_str(&t, "consistency.486dx_fpu",
                   "PASS: fpu integrated as expected",
                   CONF_HIGH, VERDICT_PASS);

    CHECK(report_write_csv(&t, tmp) == 0, "write returns 0 on success");
    n = read_file(tmp);
    CHECK(n > 0, "file readable after write");

    CHECK(strstr(file_buf, "key,value,confidence,verdict\n") == file_buf,
          "file starts with header row");
    CHECK(strstr(file_buf, "cpu.detected,Intel i486DX-2-66,HIGH,PASS\n") != NULL,
          "string value emits unquoted when no special chars");
    CHECK(strstr(file_buf, "bench.cpu.int_iters_per_sec,1964636,HIGH,PASS\n") != NULL,
          "V_U32 value emits decimal");

    remove(tmp);
}

static void test_rfc4180_escaping(void)
{
    result_table_t t;
    const char *tmp = "tmp_csv_escape.csv";

    printf("\n[test_rfc4180_escaping]\n");
    memset(&t, 0, sizeof(t));

    /* Value containing comma -> must be quoted */
    report_add_str(&t, "comma.test", "one, two, three",
                   CONF_HIGH, VERDICT_PASS);
    /* Value containing quote -> doubled + quoted */
    report_add_str(&t, "quote.test", "he said \"hi\"",
                   CONF_MEDIUM, VERDICT_WARN);
    /* Value containing newline -> quoted (CSV permits embedded newlines in quoted fields) */
    report_add_str(&t, "nl.test", "line1\nline2",
                   CONF_LOW, VERDICT_FAIL);
    /* Plain key with dots stays unquoted (dots are not CSV specials) */
    report_add_str(&t, "plain.key", "plain_value",
                   CONF_HIGH, VERDICT_UNKNOWN);

    CHECK(report_write_csv(&t, tmp) == 0, "write returns 0");
    read_file(tmp);

    CHECK(strstr(file_buf, "comma.test,\"one, two, three\",HIGH,PASS\n") != NULL,
          "comma in value triggers quoting");
    CHECK(strstr(file_buf, "quote.test,\"he said \"\"hi\"\"\",MEDIUM,WARN\n") != NULL,
          "embedded quotes double + field is quoted");
    CHECK(strstr(file_buf, "nl.test,\"line1\nline2\",LOW,FAIL\n") != NULL,
          "embedded newline triggers quoting (newline preserved)");
    CHECK(strstr(file_buf, "plain.key,plain_value,HIGH,UNK\n") != NULL,
          "plain value emits unquoted; VERDICT_UNKNOWN renders as UNK");

    remove(tmp);
}

static void test_confidence_verdict_names(void)
{
    result_table_t t;
    const char *tmp = "tmp_csv_enums.csv";

    printf("\n[test_confidence_verdict_names]\n");
    memset(&t, 0, sizeof(t));

    report_add_str(&t, "c.high", "x", CONF_HIGH,   VERDICT_PASS);
    report_add_str(&t, "c.med",  "x", CONF_MEDIUM, VERDICT_WARN);
    report_add_str(&t, "c.low",  "x", CONF_LOW,    VERDICT_FAIL);
    report_add_str(&t, "c.unk",  "x", CONF_HIGH,   VERDICT_UNKNOWN);

    report_write_csv(&t, tmp);
    read_file(tmp);

    CHECK(strstr(file_buf, "c.high,x,HIGH,PASS\n")   != NULL, "CONF_HIGH -> HIGH");
    CHECK(strstr(file_buf, "c.med,x,MEDIUM,WARN\n")  != NULL, "CONF_MEDIUM -> MEDIUM, VERDICT_WARN -> WARN");
    CHECK(strstr(file_buf, "c.low,x,LOW,FAIL\n")     != NULL, "CONF_LOW -> LOW, VERDICT_FAIL -> FAIL");
    CHECK(strstr(file_buf, "c.unk,x,HIGH,UNK\n")     != NULL, "VERDICT_UNKNOWN -> UNK");

    remove(tmp);
}

static void test_fopen_failure(void)
{
    result_table_t t;

    printf("\n[test_fopen_failure]\n");
    memset(&t, 0, sizeof(t));
    /* Nonexistent directory + filename — fopen will fail */
    CHECK(report_write_csv(&t, "zzz_no_such_dir/out.csv") == -1,
          "fopen failure returns -1");
}

int main(void)
{
    test_basic_emit();
    test_rfc4180_escaping();
    test_confidence_verdict_names();
    test_fopen_failure();

    printf("\n=== %d failure(s) ===\n", failures);
    return failures ? 1 : 0;
}
