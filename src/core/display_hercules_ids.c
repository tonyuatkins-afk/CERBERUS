/*
 * Pure classifier + token mapper for Hercules variants (0.8.1 M3.3).
 *
 * Extracted from display.c so the host test can include this file
 * directly and exercise the logic without pulling in display.c's
 * DOS-only conio.h / dos.h / i86.h dependencies. Included as source
 * from display.c for the DOS build and from test_hercules_variant.c
 * for the host build.
 */

hercules_variant_t display_classify_hercules_id(unsigned char id_bits)
{
    /* id_bits carries the 3 ID bits (originally bits 6:4 of 3BAh) already
     * right-shifted into 2:0. Documented mappings:
     *   0x00 -> HGC (original, 1982 — no ID bits wired)
     *   0x01 -> HGC+ (1986 — adds softfont RAM)
     *   0x05 -> InColor (1987 — 16-color attribute byte)
     * Any other pattern falls through to UNKNOWN so we don't claim a
     * specific model when the hardware reports something we haven't
     * catalogued. */
    switch (id_bits & 0x07) {
        case 0x00: return HERCULES_VARIANT_HGC;
        case 0x01: return HERCULES_VARIANT_HGCPLUS;
        case 0x05: return HERCULES_VARIANT_INCOLOR;
        default:   return HERCULES_VARIANT_UNKNOWN;
    }
}

const char *display_hercules_variant_token(hercules_variant_t v)
{
    switch (v) {
        case HERCULES_VARIANT_HGC:     return "hgc";
        case HERCULES_VARIANT_HGCPLUS: return "hgcplus";
        case HERCULES_VARIANT_INCOLOR: return "incolor";
        case HERCULES_VARIANT_UNKNOWN: return "unknown";
        default:                       return "na";
    }
}
