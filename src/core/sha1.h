/*
 * SHA-1 (FIPS 180-4 / RFC 3174), local implementation.
 *
 * Not a cryptographic primitive — CERBERUS uses SHA-1 for hardware and run
 * fingerprinting where collision resistance is optional. A local impl keeps
 * the DOS binary free of external crypto-library dependencies and lets the
 * signature format stay frozen across tool versions.
 */
#ifndef CERBERUS_SHA1_H
#define CERBERUS_SHA1_H

#define SHA1_BLOCK_SIZE 64
#define SHA1_DIGEST_LEN 20
#define SHA1_HEX_LEN    40  /* plus trailing NUL, so buffer must be 41 bytes */

typedef struct {
    unsigned long h[5];
    unsigned long bytes_lo;   /* total bytes low  32 */
    unsigned long bytes_hi;   /* total bytes high 32 */
    unsigned int  buf_used;
    unsigned char buf[SHA1_BLOCK_SIZE];
} sha1_ctx_t;

void sha1_init(sha1_ctx_t *ctx);
void sha1_update(sha1_ctx_t *ctx, const void *data, unsigned int len);
void sha1_final(sha1_ctx_t *ctx, unsigned char digest[SHA1_DIGEST_LEN]);

/* Convenience: one-shot hash */
void sha1_hash(const void *data, unsigned long len,
               unsigned char digest[SHA1_DIGEST_LEN]);

/* Write 40-char lowercase hex (+NUL) into out; out must hold >=41 bytes */
void sha1_to_hex(const unsigned char digest[SHA1_DIGEST_LEN], char *out);

#endif
