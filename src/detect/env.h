/*
 * Environment / emulator detection.
 *
 * Runs FIRST in the Head I orchestrator so subsequent detect modules can
 * consult env_clamp() to cap their reported confidence when running in
 * an emulator. DOSBox-X cache/timing fidelity is synthetic — the plan's
 * Phase 1 gate requires DOSBox-X output to match real-hardware output at
 * matching confidence or the gate fails, and that only works if emulator
 * detection is reliable.
 */
#ifndef CERBERUS_DETECT_ENV_H
#define CERBERUS_DETECT_ENV_H

#include "../cerberus.h"

typedef enum {
    EMU_NONE        = 0,   /* real hardware (or undetected emulator) */
    EMU_DOSBOX      = 1,
    EMU_DOSBOX_X    = 2,
    EMU_PCEM        = 3,
    EMU_86BOX       = 4,
    EMU_QEMU        = 5,
    EMU_BOCHS       = 6,
    EMU_VIRTUALBOX  = 7,
    EMU_VMWARE      = 8,
    EMU_NTVDM       = 9,
    EMU_OS2_DOS     = 10,
    EMU_UNKNOWN     = 11    /* timing hint fired but no known signature */
} emulator_id_t;

void          detect_env(result_table_t *t);
emulator_id_t env_emulator(void);
const char   *env_emulator_name(void);
int           env_is_emulated(void);
confidence_t  env_clamp(confidence_t c);

#endif
