; CERBERUS FPU behavioral fingerprinting probes (v0.7.1)
;
; Four probes distinguish 8087/80287 (legacy x87) from 80387+ (full
; IEEE-754):
;   1. Infinity comparison mode       (projective vs affine)
;   2. Pseudo-NaN encoding handling   (silently accepted vs #IE)
;   3. FPREM1 opcode (D9 F5)          (invalid on pre-387)
;   4. FSIN opcode (D9 FE)            (invalid on pre-387)
;
; Probes 3 and 4 rely on an INT 6 handler that advances the saved IP
; past the 2-byte faulting opcode and sets a CS-local fault flag. Same
; pattern as cpu_a.asm; we clone the mechanism here rather than share
; cpu_a.asm's fault flag to keep phase ownership clean (Phase 1
; detection owns cpu_a.asm's flag; Phase 2 diagnostics owns this one).
;
; Caller contract:
;   - Install fpu_fp_int6_handler via _dos_setvect(6, ...) before any
;     probe that could fault (probes 3 and 4).
;   - Call fpu_fp_int6_clear() immediately before the faulting probe.
;   - Check fpu_fp_int6_fired() after the probe.
;   - Restore the previous INT 6 vector after all probes complete.

bits 16

segment diag_fpu_fp_TEXT public class=CODE

global  fpu_fp_int6_handler_
global  fpu_fp_int6_fired_
global  fpu_fp_int6_clear_
global  fpu_fp_compare_inf_
global  fpu_fp_load_pseudo_nan_
global  fpu_fp_try_fprem1_
global  fpu_fp_try_fsin_

; --- CS-local storage (INT 6 handler runs with arbitrary DS) ---
fault_flag: dw 0

; ---------------------------------------------------------------------
; INT 6 (invalid opcode) handler.
;
; Exactly the same shape as cpu_asm_int6_handler in cpu_a.asm: set the
; fault flag, advance saved IP past the 2-byte faulting opcode, IRET.
; Our faulting sequences are (D9 F5) and (D9 FE), both 2 bytes.
;
; Stack on entry (after INT 6 pushes FLAGS, CS, IP, then our push bp):
;   [BP+0]  saved BP
;   [BP+2]  IP   <- add 2 here
;   [BP+4]  CS
;   [BP+6]  FLAGS
; ---------------------------------------------------------------------
fpu_fp_int6_handler_:
    push bp
    mov bp, sp
    push ax
    mov ax, 1
    mov [cs:fault_flag], ax
    add word [bp+2], 2
    pop ax
    pop bp
    iret

; ---------------------------------------------------------------------
; int fpu_fp_int6_fired(void)  — returns current fault_flag in AX
; ---------------------------------------------------------------------
fpu_fp_int6_fired_:
    mov ax, [cs:fault_flag]
    retf

; ---------------------------------------------------------------------
; void fpu_fp_int6_clear(void)  — zeros fault_flag
; ---------------------------------------------------------------------
fpu_fp_int6_clear_:
    mov word [cs:fault_flag], 0
    retf

; ---------------------------------------------------------------------
; unsigned int fpu_fp_compare_inf(const unsigned char __far *plus_inf,
;                                  const unsigned char __far *minus_inf)
;
; Loads two 80-bit extended-precision values from caller memory, compares
; them with FCOMP, and returns the raw FSTSW AX value. Caller decodes:
;   C3 (bit 14): 1 if +inf and -inf compare EQUAL (projective) or UNORDERED
;   C0 (bit 8):  1 if ST(0) < ST(1)
;
; On 80387+ (affine): -inf < +inf, so C3=0 C2=0 C0=1.
; On 8087/80287 (projective): +inf == -inf, so C3=1 C2=0 C0=0.
;
; Watcom medium model, cdecl via #pragma aux parm caller []. Arg layout:
;   [bp+6]  plus_inf offset
;   [bp+8]  plus_inf segment
;   [bp+10] minus_inf offset
;   [bp+12] minus_inf segment
; ---------------------------------------------------------------------
fpu_fp_compare_inf_:
    push bp
    mov bp, sp
    push es
    push di

    finit                       ; reset FPU state, mask all exceptions

    ; Load ST(0) <- +inf
    mov di, [bp+6]
    mov es, [bp+8]
    fld tword [es:di]

    ; Load ST(0) <- -inf, pushing prior onto ST(1)
    ; So after: ST(0) = -inf, ST(1) = +inf
    mov di, [bp+10]
    mov es, [bp+12]
    fld tword [es:di]

    ; FCOMP: compare ST(0) with ST(1), pop ST(0).
    ; Status word C3/C2/C0 reflect the comparison:
    ;   ST(0) > ST(1)   -> C3=0 C2=0 C0=0
    ;   ST(0) < ST(1)   -> C3=0 C2=0 C0=1
    ;   ST(0) == ST(1)  -> C3=1 C2=0 C0=0
    ;   unordered       -> C3=1 C2=1 C0=1
    fcomp st1
    fnstsw ax                   ; 16-bit status word -> AX

    finit                       ; scrub stack regardless of outcome

    pop di
    pop es
    pop bp
    retf

; ---------------------------------------------------------------------
; unsigned int fpu_fp_load_pseudo_nan(const unsigned char __far *ptr)
;
; Clears FPU exceptions, loads a 10-byte "pseudo-NaN" encoding (exponent
; all 1s, explicit integer bit = 0 — not a valid 80387 value), and
; returns the raw FSTSW AX.
;
; On 80387+: IE (bit 0) set. Other bits may be set too (SF, C1).
; On 8087/80287: IE stays 0; value accepted as-is.
;
; Arg layout (medium model cdecl):
;   [bp+6]  ptr offset
;   [bp+8]  ptr segment
; ---------------------------------------------------------------------
fpu_fp_load_pseudo_nan_:
    push bp
    mov bp, sp
    push es
    push di

    finit                       ; clean slate, all exceptions masked+cleared
    fnclex                      ; belt+braces: clear exception flags only

    mov di, [bp+6]
    mov es, [bp+8]
    fld tword [es:di]
    fnstsw ax                   ; capture status BEFORE cleanup FINIT

    finit                       ; drop the value (and any exceptions)

    pop di
    pop es
    pop bp
    retf

; ---------------------------------------------------------------------
; void fpu_fp_try_fprem1(void)
;
; Attempts FPREM1 (opcode D9 F5). This instruction was introduced in
; the 80387; on 8087/80287 it is an INVALID OPCODE and raises INT 6.
;
; Caller MUST:
;   - have installed fpu_fp_int6_handler
;   - have called fpu_fp_int6_clear() immediately before this probe
; Caller inspects fpu_fp_int6_fired() after return to determine whether
; the opcode executed or trapped.
;
; FPREM1 requires two operands on the FPU stack: ST(0) and ST(1). We
; preload 2.0 and 3.0 — arbitrary nonzero values that produce a sane
; partial remainder when the instruction executes on a 387+. If it
; traps, the stack state before the trap doesn't matter (FINIT resets).
; ---------------------------------------------------------------------
fpu_fp_try_fprem1_:
    finit
    fld1                        ; ST(0) = 1.0
    fld1                        ; ST(0) = 1.0, ST(1) = 1.0
    ; FPREM1 needs two operands on the stack. We don't care about the
    ; numerical result — only whether the opcode itself is valid. Any
    ; two finite nonzero values will compute cleanly on a 387+, and on
    ; 8087/287 the invalid-opcode trap fires before the FPU sees them.
    db 0D9h, 0F5h               ; FPREM1
    finit                       ; drop whatever's on the stack
    retf

; ---------------------------------------------------------------------
; void fpu_fp_try_fsin(void)
;
; Attempts FSIN (opcode D9 FE). Introduced in 80387; INT 6 on earlier.
; Same contract as fpu_fp_try_fprem1 regarding the INT 6 handler.
;
; FSIN needs ST(0) in range; 0.0 is universally safe (sin(0) = 0).
; ---------------------------------------------------------------------
fpu_fp_try_fsin_:
    finit
    fldz                        ; ST(0) = 0.0
    db 0D9h, 0FEh               ; FSIN
    finit
    retf
