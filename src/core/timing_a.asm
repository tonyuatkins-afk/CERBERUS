; CERBERUS timing — RDTSC wrapper for timing.c (v0.7.1)
;
; Watcom medium memory model: public functions use RETF. Watcom's default
; register calling convention returns 32-bit in DX:AX. Symbol naming gets
; a trailing underscore (matches the convention in cpu_a.asm / fpu_a.asm).
;
; Caller MUST verify CPUID presence (via cpu_asm_id_test) and then CPUID
; leaf 1 EDX bit 4 (TSC) before invoking rdtsc_lo — this file has NO trap
; handling. timing_has_rdtsc() in timing.c is responsible for the gate.
;
; Only the low 32 bits of the TSC are returned. For any benchmark kernel
; that CERBERUS runs (<5 s), 32 bits is sufficient up to ~400 MHz. The
; caller subtracts with 32-bit unsigned wrap arithmetic — one wrap can
; occur mid-measurement at high clocks and is correctly handled by the
; usual "end - start" subtraction when both are unsigned long.

bits 16

segment timing_a_TEXT public class=CODE

global timing_asm_rdtsc_lo_

; ---------------------------------------------------------------------
; unsigned long timing_asm_rdtsc_lo(void)
;   Returns the low 32 bits of the Time Stamp Counter.
;   Caller has already verified RDTSC is supported.
;
;   Watcom 16-bit return convention: 32-bit unsigned long in DX:AX
;   (AX = low 16 bits, DX = high 16 bits of the returned value).
; ---------------------------------------------------------------------
timing_asm_rdtsc_lo_:
    rdtsc                      ; -> EDX:EAX (NASM emits 0F 31).
                               ; EAX = low 32 bits of TSC, EDX = high 32 (unused).
    mov edx, eax               ; copy low 32 into edx for high-half extraction
    shr edx, 16                ; DX = bits 16..31 of the low 32 (NASM emits 66 prefix)
    retf
