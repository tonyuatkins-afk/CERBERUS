/*
 * Video detection — Phase 1 Task 1.6 + 1.6a.
 *
 * Reuses the layered adapter detection already in core/display.c (runs
 * at display_init() time). Adds:
 *   - Chipset identification via BIOS ROM signature scan at
 *     C000:0000-C000:7FFF, matched against hw_db/video.csv.
 *   - VBE info query via INT 10h AX=4F00h for SVGA adapters — not
 *     plumbed through to the DB yet; reports VBE version and OEM
 *     string directly.
 *
 * The adapter enum already provides MDA/CGA/Hercules/EGA/VGA/MCGA; the
 * DB fills in the chipset specifics (Trident, Tseng, S3, etc.). On
 * non-SVGA adapters the DB lookup simply won't find a match and we
 * fall back to the adapter class for identification.
 */

#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <i86.h>
#include <conio.h>
#include "detect.h"
#include "env.h"
#include "video_db.h"
#include "../core/display.h"
#include "../core/report.h"

/* VBE version display buffer. report_add_str stores the value pointer
 * verbatim (report.c:55), so a stack-local sprintf target would dangle
 * after detect_video returns. Only one dynamic string is emitted
 * (video.vbe_version), so one static suffices. */
static char video_vbe_version_val[16];

/* ----------------------------------------------------------------------- */
/* Adapter-class reporting                                                  */
/* ----------------------------------------------------------------------- */

static const char *adapter_token(adapter_t a)
{
    /* Canonical tokens for signature stability — do NOT rename without
     * bumping signature_schema per PF-4. */
    switch (a) {
        case ADAPTER_MDA:       return "mda";
        case ADAPTER_CGA:       return "cga";
        case ADAPTER_HERCULES:  return "hercules";
        case ADAPTER_EGA_MONO:  return "ega";
        case ADAPTER_EGA_COLOR: return "ega";
        case ADAPTER_VGA_MONO:  return "vga";
        case ADAPTER_VGA_COLOR: return "vga";
        case ADAPTER_MCGA:      return "mcga";
        default:                return "unknown";
    }
}

/* ----------------------------------------------------------------------- */
/* Video BIOS signature scan                                                */
/* ----------------------------------------------------------------------- */

static int mem_match(const unsigned char __far *mem, const char *needle, unsigned int n)
{
    unsigned int j;
    for (j = 0; j < n; j++) {
        if (mem[j] != (unsigned char)needle[j]) return 0;
    }
    return 1;
}

static const video_db_entry_t *scan_video_bios(void)
{
    /* Video BIOS ROM lives at C000:0000 through C000:7FFF (32KB). Scan
     * byte-by-byte for any known chipset substring. First match wins —
     * the CSV is ordered specific-before-general. */
    const unsigned char __far *vbios = (const unsigned char __far *)MK_FP(0xC000, 0x0000);
    unsigned long i;
    unsigned int  s;
    unsigned int  need_len[32];   /* cap — we have 28 entries and room for growth */

    for (s = 0; s < video_db_count && s < 32; s++) {
        need_len[s] = (unsigned int)strlen(video_db[s].bios_signature);
    }

    for (i = 0; i < 0x7FFEUL; i++) {
        for (s = 0; s < video_db_count && s < 32; s++) {
            if (i + need_len[s] > 0x7FFEUL) continue;
            if (mem_match(vbios + i, video_db[s].bios_signature, need_len[s])) {
                return &video_db[s];
            }
        }
    }
    return (const video_db_entry_t *)0;
}

/* ----------------------------------------------------------------------- */
/* VBE info                                                                 */
/* ----------------------------------------------------------------------- */

typedef struct {
    char           signature[4];   /* "VESA" on success */
    unsigned int   version;        /* BCD: high=major, low=minor */
    unsigned long  oem_string_ptr; /* far pointer as seg:off */
    /* ... further fields omitted; we only need signature + version */
} vbe_info_t;

static vbe_info_t vbe_buf;

/* S3 CRTC chip-ID probe. Some S3 SVGA cards (Trio64 seen on the
 * 486 DX-2 bench) ship BIOS ROMs that contain the literal "IBM VGA"
 * compatibility signature earlier than any "S3"/"Trio64" substring,
 * so scan_video_bios() matches the IBM VGA entry and we lose the
 * chipset identity.
 *
 * S3's chip identifier lives in CRTC extended register 0x30, gated
 * behind an unlock at extended register 0x38. Write 0x48 to 0x38,
 * then read 0x30 via the standard CRTC index/data pair at 0x3D4/0x3D5.
 *
 * Known IDs we map here: 0xE1 → Trio64 (and 32/64 siblings that land
 * on the same chip-id value). Add more mappings as we validate them
 * on real hardware. Returns a video_db_entry_t* via signature lookup
 * or NULL if no S3 chip is present or the ID doesn't match a known DB
 * row. */
static const video_db_entry_t *find_video_db_entry(const char *signature)
{
    unsigned int i;
    for (i = 0; i < video_db_count; i++) {
        if (strcmp(video_db[i].bios_signature, signature) == 0) {
            return &video_db[i];
        }
    }
    return (const video_db_entry_t *)0;
}

static const video_db_entry_t *probe_s3_chipid(void)
{
    unsigned char saved_38, saved_idx, id;

    /* Save then unlock extended CRTC registers (S3 "CR38 unlock"). */
    saved_idx = (unsigned char)inp(0x3D4);
    outp(0x3D4, 0x38);
    saved_38 = (unsigned char)inp(0x3D5);
    outp(0x3D5, 0x48);

    /* Read CRTC index 0x30 — chip ID. */
    outp(0x3D4, 0x30);
    id = (unsigned char)inp(0x3D5);

    /* Restore CR38 and the prior CRTC index so we don't disturb the
     * display state of any later consumer. */
    outp(0x3D4, 0x38);
    outp(0x3D5, saved_38);
    outp(0x3D4, saved_idx);

    switch (id) {
        case 0xE1:
            return find_video_db_entry("Trio64");
        case 0xE6:
            return find_video_db_entry("Virge");
        /* Other S3 IDs (0xE0 911/924, 0x90/0xB0 928, 0xC0 864, 0xD0 964,
         * 0xE3 Vision864, 0xE5 Vision964, 0xA5 801/805) fall through —
         * add DB rows + cases here as real hardware validates them. */
        default:
            return (const video_db_entry_t *)0;
    }
}

static int probe_vbe(unsigned int *out_version)
{
    union  REGS  r;
    struct SREGS sr;
    memset(&vbe_buf, 0, sizeof(vbe_buf));
    vbe_buf.signature[0] = 'V';
    vbe_buf.signature[1] = 'B';
    vbe_buf.signature[2] = 'E';
    vbe_buf.signature[3] = '2';  /* request VBE 2.0+ info if available */

    segread(&sr);
    r.x.ax = 0x4F00;
    r.x.di = FP_OFF(&vbe_buf);
    sr.es  = FP_SEG(&vbe_buf);
    int86x(0x10, &r, &r, &sr);

    if (r.x.ax != 0x004F) return 0;
    if (vbe_buf.signature[0] != 'V' || vbe_buf.signature[1] != 'E' ||
        vbe_buf.signature[2] != 'S' || vbe_buf.signature[3] != 'A') {
        return 0;
    }
    *out_version = vbe_buf.version;
    return 1;
}

/* ----------------------------------------------------------------------- */
/* Orchestration                                                            */
/* ----------------------------------------------------------------------- */

void detect_video(result_table_t *t)
{
    adapter_t a = display_adapter();
    const char *adapter_tok = adapter_token(a);
    const video_db_entry_t *chip;
    unsigned int vbe_version = 0;

    report_add_str(t, "video.adapter", adapter_tok,
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    report_add_str(t, "video.color",
                   display_has_color() ? "yes" : "no",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    /* Chipset: HW probe first (S3 chip-ID register), then fall back to
     * BIOS ROM string scan. The HW probe wins because many SVGA cards
     * embed "IBM VGA" for compatibility, which the BIOS scan would match
     * with higher priority than the true vendor signature. */
    chip = (const video_db_entry_t *)0;
    if (a == ADAPTER_VGA_COLOR || a == ADAPTER_VGA_MONO || a == ADAPTER_MCGA) {
        chip = probe_s3_chipid();
    }
    if (!chip) chip = scan_video_bios();
    if (chip) {
        report_add_str(t, "video.vendor",  chip->vendor,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        report_add_str(t, "video.chipset", chip->chipset,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        report_add_str(t, "video.family",  chip->family,
                       env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        if (chip->notes && *chip->notes) {
            report_add_str(t, "video.notes", chip->notes,
                           env_clamp(CONF_MEDIUM), VERDICT_UNKNOWN);
        }
    }

    /* VBE on VGA/SVGA adapters — meaningful information on ~1992+ SVGA
     * cards, absent on plain VGA. */
    if (a == ADAPTER_VGA_COLOR || a == ADAPTER_VGA_MONO || a == ADAPTER_MCGA) {
        if (probe_vbe(&vbe_version)) {
            sprintf(video_vbe_version_val, "%u.%u",
                    (vbe_version >> 8) & 0xFF,
                    vbe_version & 0xFF);
            report_add_str(t, "video.vbe_version", video_vbe_version_val,
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        } else {
            report_add_str(t, "video.vbe_version", "none",
                           env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
        }
    }
}
