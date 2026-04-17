/*
 * SHA-1 — RFC 3174 algorithmic reference implementation.
 *
 * Operates on 32-bit unsigned long values. On 16-bit Watcom DOS targets,
 * `unsigned long` is 32 bits; `<<` and `>>` on unsigned long do the right
 * thing provided the shift count is in [0, 31]. All rotate counts here are
 * in that range.
 */

#include <string.h>
#include "sha1.h"

#define ROL32(x, n)  ((((unsigned long)(x)) << (n)) | (((unsigned long)(x)) >> (32 - (n))))

static void sha1_process_block(sha1_ctx_t *ctx, const unsigned char *block)
{
    unsigned long w[80];
    unsigned long a, b, c, d, e, f, k, temp;
    int i;

    /* Load 16 big-endian 32-bit words */
    for (i = 0; i < 16; i++) {
        w[i] = ((unsigned long)block[i*4]     << 24) |
               ((unsigned long)block[i*4 + 1] << 16) |
               ((unsigned long)block[i*4 + 2] << 8)  |
               ((unsigned long)block[i*4 + 3]);
    }
    /* Extend to 80 */
    for (i = 16; i < 80; i++) {
        unsigned long t = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
        w[i] = ROL32(t, 1);
    }

    a = ctx->h[0];
    b = ctx->h[1];
    c = ctx->h[2];
    d = ctx->h[3];
    e = ctx->h[4];

    for (i = 0; i < 80; i++) {
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999UL;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1UL;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCUL;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6UL;
        }
        temp = ROL32(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROL32(b, 30);
        b = a;
        a = temp;
    }

    ctx->h[0] += a;
    ctx->h[1] += b;
    ctx->h[2] += c;
    ctx->h[3] += d;
    ctx->h[4] += e;
}

void sha1_init(sha1_ctx_t *ctx)
{
    ctx->h[0] = 0x67452301UL;
    ctx->h[1] = 0xEFCDAB89UL;
    ctx->h[2] = 0x98BADCFEUL;
    ctx->h[3] = 0x10325476UL;
    ctx->h[4] = 0xC3D2E1F0UL;
    ctx->bytes_lo = 0;
    ctx->bytes_hi = 0;
    ctx->buf_used = 0;
}

void sha1_update(sha1_ctx_t *ctx, const void *data, unsigned int len)
{
    const unsigned char *p = (const unsigned char *)data;
    unsigned long prev_lo = ctx->bytes_lo;

    ctx->bytes_lo = prev_lo + (unsigned long)len;
    if (ctx->bytes_lo < prev_lo) {
        ctx->bytes_hi++;
    }

    while (len > 0) {
        unsigned int take = SHA1_BLOCK_SIZE - ctx->buf_used;
        if (take > len) take = len;
        memcpy(ctx->buf + ctx->buf_used, p, take);
        ctx->buf_used += take;
        p   += take;
        len -= take;
        if (ctx->buf_used == SHA1_BLOCK_SIZE) {
            sha1_process_block(ctx, ctx->buf);
            ctx->buf_used = 0;
        }
    }
}

void sha1_final(sha1_ctx_t *ctx, unsigned char digest[SHA1_DIGEST_LEN])
{
    unsigned long bits_lo, bits_hi;
    int i;

    /* bits = bytes * 8. Compute 64-bit big-endian length first, before
     * we clobber the counter. */
    bits_hi = (ctx->bytes_hi << 3) | (ctx->bytes_lo >> 29);
    bits_lo =  ctx->bytes_lo << 3;

    /* Append 0x80 terminator */
    ctx->buf[ctx->buf_used++] = 0x80;

    /* Pad with zeros until there are 8 bytes left in the current block */
    if (ctx->buf_used > 56) {
        while (ctx->buf_used < SHA1_BLOCK_SIZE) ctx->buf[ctx->buf_used++] = 0;
        sha1_process_block(ctx, ctx->buf);
        ctx->buf_used = 0;
    }
    while (ctx->buf_used < 56) ctx->buf[ctx->buf_used++] = 0;

    /* Append 64-bit big-endian bit length */
    ctx->buf[56] = (unsigned char)((bits_hi >> 24) & 0xFF);
    ctx->buf[57] = (unsigned char)((bits_hi >> 16) & 0xFF);
    ctx->buf[58] = (unsigned char)((bits_hi >> 8)  & 0xFF);
    ctx->buf[59] = (unsigned char)( bits_hi        & 0xFF);
    ctx->buf[60] = (unsigned char)((bits_lo >> 24) & 0xFF);
    ctx->buf[61] = (unsigned char)((bits_lo >> 16) & 0xFF);
    ctx->buf[62] = (unsigned char)((bits_lo >> 8)  & 0xFF);
    ctx->buf[63] = (unsigned char)( bits_lo        & 0xFF);
    sha1_process_block(ctx, ctx->buf);

    /* Emit digest in big-endian */
    for (i = 0; i < 5; i++) {
        digest[i*4]     = (unsigned char)((ctx->h[i] >> 24) & 0xFF);
        digest[i*4 + 1] = (unsigned char)((ctx->h[i] >> 16) & 0xFF);
        digest[i*4 + 2] = (unsigned char)((ctx->h[i] >> 8)  & 0xFF);
        digest[i*4 + 3] = (unsigned char)( ctx->h[i]        & 0xFF);
    }
}

void sha1_hash(const void *data, unsigned long len,
               unsigned char digest[SHA1_DIGEST_LEN])
{
    sha1_ctx_t ctx;
    const unsigned char *p = (const unsigned char *)data;
    sha1_init(&ctx);
    /* For len >=65536 we'd need to chunk — split into <=32K feeds to respect
     * sha1_update's `unsigned int len` parameter on 16-bit targets. */
    while (len > 32768UL) {
        sha1_update(&ctx, p, 32768U);
        p   += 32768U;
        len -= 32768UL;
    }
    sha1_update(&ctx, p, (unsigned int)len);
    sha1_final(&ctx, digest);
}

void sha1_to_hex(const unsigned char digest[SHA1_DIGEST_LEN], char *out)
{
    static const char hex[] = "0123456789abcdef";
    int i;
    for (i = 0; i < SHA1_DIGEST_LEN; i++) {
        out[i*2]     = hex[(digest[i] >> 4) & 0x0F];
        out[i*2 + 1] = hex[ digest[i]       & 0x0F];
    }
    out[SHA1_HEX_LEN] = '\0';
}
