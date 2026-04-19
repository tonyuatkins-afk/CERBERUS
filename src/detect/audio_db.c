/*
 * AUTO-GENERATED — DO NOT EDIT.
 * Regenerate with: python hw_db/build_audio_db.py
 * Source: hw_db/audio.csv (31 entries)
 */

#include "audio_db.h"
#include <string.h>

const audio_db_entry_t audio_db[] = {
    { "pc-speaker-only", "PC Speaker only", "IBM-compatible", "Always present on IBM PC-class hardware", "unknown" },
    { "opl2:none", "AdLib (OPL2)", "AdLib", "Original 1987 card \x2014 Yamaha YM3812", "unknown" },
    { "opl2:0100", "Sound Blaster 1.0 (CT1320)", "Creative Technology", "DSP v1.x \x2014 8-bit mono 22kHz max", "unknown" },
    { "opl2:0200", "Sound Blaster 1.5 (CT1320B)", "Creative Technology", "DSP v2.x \x2014 High-speed DMA", "unknown" },
    { "opl2:0201", "Sound Blaster 2.0 (CT1350)", "Creative Technology", "DSP v2.01 \x2014 Auto-init DMA", "unknown" },
    { "opl3:0300", "Sound Blaster Pro (CT1330)", "Creative Technology", "DSP v3.xx \x2014 Stereo 8-bit + OPL2x2", "unknown" },
    { "opl3:0302", "Sound Blaster Pro 2 (CT1600)", "Creative Technology", "DSP v3.02 \x2014 Single OPL3", "unknown" },
    { "opl3:0400", "Sound Blaster 16 (CT1740)", "Creative Technology", "DSP v4.00 \x2014 16-bit stereo", "unknown" },
    { "opl3:0404", "Sound Blaster 16 (CT2230)", "Creative Technology", "DSP v4.04", "CT1745" },
    { "opl3:0405", "Sound Blaster 16 (CT2290)", "Creative Technology", "DSP v4.05 \x2014 Value edition", "CT1745" },
    { "opl3:040B", "Sound Blaster 16 Vibra (CT2800)", "Creative Technology", "DSP v4.11 \x2014 Vibra 16 early revision", "unknown" },
    { "opl3:040C", "Sound Blaster AWE32 (CT2760)", "Creative Technology", "DSP v4.12 \x2014 EMU8000 wavetable", "unknown" },
    { "opl3:040D:T6", "Sound Blaster 16 or Vibra 16S", "Creative Technology", "DSP v4.13 T6 \x2014 CT2230+ SB16 or CT2800/2900 Vibra 16", "CT1745" },
    { "opl3:040D:T8", "Sound Blaster AWE32", "Creative Technology", "DSP v4.13 T8 \x2014 EMU8000-equipped AWE32", "unknown" },
    { "opl3:040D", "Sound Blaster 16/Vibra/AWE32 family", "Creative Technology", "DSP v4.13 \x2014 ambiguous without BLASTER T token", "unknown" },
    { "opl3:0410:T8", "Sound Blaster AWE64 (CT4500)", "Creative Technology", "DSP v4.16 \x2014 AWE64 with original AWE32 compat flag", "unknown" },
    { "opl3:0410:T9", "Sound Blaster AWE64 (CT4500)", "Creative Technology", "DSP v4.16 \x2014 AWE64 with AWE64 T9 token", "unknown" },
    { "opl3:0410", "Sound Blaster AWE64 family", "Creative Technology", "DSP v4.16 \x2014 AWE64/Gold without T token", "unknown" },
    { "opl3:0411", "Sound Blaster AWE64 Gold (CT4540)", "Creative Technology", "DSP v4.17 \x2014 AWE64 Gold premium variant", "unknown" },
    { "opl3:none", "OPL3-based card (unknown)", "various", "OPL3 detected without recognizable DSP", "unknown" },
    { "opl2:none-adlib", "AdLib Gold", "AdLib", "1992 OPL3 + YMZ263 codec (later product)", "unknown" },
    { "gus-classic", "Gravis UltraSound (Classic)", "Advanced Gravis", "GF1 synth \x2014 not OPL-based", "unknown" },
    { "gus-max", "Gravis UltraSound MAX", "Advanced Gravis", "GF1 + CS4231 codec", "unknown" },
    { "gus-ace", "Gravis UltraSound ACE", "Advanced Gravis", "Audio Compression Engine", "unknown" },
    { "ess-688", "ESS AudioDrive ES688", "ESS Technology", "SB Pro compatible", "unknown" },
    { "ess-1688", "ESS AudioDrive ES1688", "ESS Technology", "SB Pro + OPL3", "unknown" },
    { "ess-1868", "ESS AudioDrive ES1868", "ESS Technology", "Plug and Play", "unknown" },
    { "mpas-16", "Pro AudioSpectrum 16", "MediaVision", "SB-compatible mode + native", "unknown" },
    { "mpu401-mt32", "Roland MT-32", "Roland", "MPU-401 synth \x2014 general MIDI on intelligent mode", "unknown" },
    { "mpu401-sc55", "Roland SC-55 Sound Canvas", "Roland", "General MIDI reference", "unknown" },
    { "mpu401-unknown", "MPU-401 compatible MIDI device", "various", "Responds to MPU-401 UART", "unknown" },
};

const unsigned int audio_db_count = 31;

const audio_db_entry_t *audio_db_lookup(const char *match_key)
{
    unsigned int i;
    if (!match_key) return (const audio_db_entry_t *)0;
    for (i = 0; i < audio_db_count; i++) {
        if (strcmp(audio_db[i].match_key, match_key) == 0) return &audio_db[i];
    }
    return (const audio_db_entry_t *)0;
}
