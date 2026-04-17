; CERBERUS FPU presence probe.
;
; Classic sentinel-based test:
;   1. Write 0x5A5A to a word in memory.
;   2. Execute FNINIT — resets the FPU if present. On systems with no
;      coprocessor installed the instruction is ignored (8086/286/386
;      without FPU socket populated) or no-ops silently.
;   3. Execute FNSTSW into the same memory word.
;   4. Read back — if still 0x5A5A, no FPU wrote to it; else FPU present.
;
; The "N" prefix on FNINIT/FNSTSW means "no wait" — the host CPU doesn't
; insert a WAIT (implicit FWAIT) prefix, so on systems without a
; coprocessor the instructions execute as effective no-ops without
; stalling on the BUSY# pin.
;
; Sentinel lives in DGROUP (C-side `static unsigned short`) so FNSTSW
; writes via DS (default segment) land in the right place. In medium
; memory model DS = DGROUP throughout normal execution.

bits 16

segment fpu_a_TEXT public class=CODE

global fpu_asm_probe_

extern _fpu_sentinel             ; C-side variable; Watcom prefixes data with _

; ---------------------------------------------------------------------
; int fpu_asm_probe(void)
;   Returns 1 if an FPU wrote to our sentinel (present), 0 otherwise.
;
;   Must not be called from an interrupt context. Must be called with
;   DS = DGROUP (normal C-call state under medium model).
; ---------------------------------------------------------------------
fpu_asm_probe_:
    mov word [_fpu_sentinel], 5A5Ah
    fninit
    fnstsw [_fpu_sentinel]
    mov ax, [_fpu_sentinel]
    cmp ax, 5A5Ah
    je .no_fpu
    mov ax, 1
    retf
.no_fpu:
    xor ax, ax
    retf
