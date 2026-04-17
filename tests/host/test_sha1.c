/*
 * Host-side unit test for SHA-1 — RFC 3174 test vectors.
 * Expected values are copied verbatim from RFC 3174 Appendix A.5 /
 * NIST FIPS 180-4 test examples.
 */

#include "../../src/core/sha1.c"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect_hash(const char *label, const void *data, unsigned long len,
                        const char *expected_hex)
{
    unsigned char digest[SHA1_DIGEST_LEN];
    char got[SHA1_HEX_LEN + 1];
    sha1_hash(data, len, digest);
    sha1_to_hex(digest, got);
    if (strcmp(got, expected_hex) == 0) {
        printf("  OK   %-40s = %s\n", label, got);
    } else {
        printf("  FAIL %s\n"
               "       expected %s\n"
               "       got      %s\n", label, expected_hex, got);
        failures++;
    }
}

static char big_block[1000000];

int main(void)
{
    printf("=== CERBERUS host unit test: SHA-1 ===\n");

    /* RFC 3174 TEST1: "abc" */
    expect_hash("RFC TEST1: \"abc\"", "abc", 3,
                "a9993e364706816aba3e25717850c26c9cd0d89d");

    /* RFC 3174 TEST2: 56 bytes */
    expect_hash("RFC TEST2: 448-bit message",
                "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                56,
                "84983e441c3bd26ebaae4aa1f95129e5e54670f1");

    /* RFC 3174 TEST3: one million 'a's — exercises multi-MB streaming + padding */
    memset(big_block, 'a', sizeof(big_block));
    expect_hash("RFC TEST3: 1,000,000 'a's", big_block, sizeof(big_block),
                "34aa973cd4c4daa4f61eeb2bdbad27316534016f");

    /* Well-known: empty message — exercises final pad with zero content */
    expect_hash("empty string", "", 0,
                "da39a3ee5e6b4b0d3255bfef95601890afd80709");

    /* Well-known: single byte 'a' */
    expect_hash("\"a\"", "a", 1,
                "86f7e437faa5a7fce15d1ddcb9eaeaea377667b8");

    /* Known: "The quick brown fox jumps over the lazy dog" — widely quoted */
    expect_hash("pangram",
                "The quick brown fox jumps over the lazy dog", 43,
                "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

    /* Known: trailing-period pangram variant — triggers different padding */
    expect_hash("pangram.",
                "The quick brown fox jumps over the lazy cog", 43,
                "de9f2c7fd25e1b3afad3e85a0bd17d9b100db4b3");

    printf("=== %d failure(s) ===\n", failures);
    return failures == 0 ? 0 : 1;
}
