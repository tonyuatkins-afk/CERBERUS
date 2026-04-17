#include "../../src/detect/fpu_db.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL %s\n", msg); failures++; } \
    else         { printf("  OK   %s\n", msg); } \
} while (0)

int main(void)
{
    const fpu_db_entry_t *e;

    printf("=== CERBERUS host unit test: fpu_db ===\n");
    CHECK(fpu_db_count > 0, "fpu_db has entries");

    e = fpu_db_lookup("none");
    CHECK(e != NULL, "'none' tag is present");
    CHECK(e != NULL && strstr(e->friendly, "No FPU") != NULL,
          "'none' friendly mentions No FPU");

    e = fpu_db_lookup("8087");
    CHECK(e != NULL,                                        "8087 lookup");
    CHECK(e != NULL && strcmp(e->vendor, "Intel") == 0,     "8087 vendor is Intel");

    e = fpu_db_lookup("287");
    CHECK(e != NULL, "287 lookup");

    e = fpu_db_lookup("387");
    CHECK(e != NULL, "387 lookup");

    e = fpu_db_lookup("integrated-486");
    CHECK(e != NULL, "integrated-486 lookup");

    e = fpu_db_lookup("integrated-pentium");
    CHECK(e != NULL, "integrated-pentium lookup");

    e = fpu_db_lookup("rapidcad");
    CHECK(e != NULL, "rapidcad lookup");
    CHECK(e != NULL && strstr(e->notes, "async") != NULL,
          "rapidcad notes mention async clock");

    e = fpu_db_lookup("bogus-tag");
    CHECK(e == NULL, "unknown tag returns NULL");

    e = fpu_db_lookup(NULL);
    CHECK(e == NULL, "NULL tag returns NULL");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
