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
global  fpu_fp_try_fptan_
global  fpu_fp_probe_rounding_
global  fpu_fp_probe_precision_
global  fpu_fp_probe_exceptions_

; --- CS-local storage (INT 6 handler runs with arbitrary DS) ---
fault_flag: dw 0

; --- FPTAN probe constants (CS-local, 80-bit extended precision) ---
; 0.5 is chosen as the FPTAN input angle because:
;   - It's well inside the 387's |θ| < π/4 valid range (π/4 ≈ 0.785).
;   - tan(0.5) ≈ 0.5463, not 1.0, so the 387's "always pushes 1.0"
;     semantic is distinguishable from the 8087/287 representation
;     that leaves ST(0) = cos(0.5) ≈ 0.8776 (denominator of Y/X
;     tangent representation). tan(0) would not distinguish because
;     cos(0) = 1 coincidentally matches the 387 push pattern.
fptan_half: dt 0.5
fptan_one:  dt 1.0

; --- Rounding-control probe constants (M2.3, research gap J) ---
; 1.5 and -1.5 are the classic discriminators: each RC mode rounds
; them to a unique (pos, neg) pair of integers.
;   RC=00 (nearest even):  FISTP(1.5) = 2, FISTP(-1.5) = -2
;   RC=01 (round down):    FISTP(1.5) = 1, FISTP(-1.5) = -2
;   RC=10 (round up):      FISTP(1.5) = 2, FISTP(-1.5) = -1
;   RC=11 (round to zero): FISTP(1.5) = 1, FISTP(-1.5) = -1
; All four pairs are distinct, so the 8 returned integers fully
; characterize RC behavior on the tested FPU.
rnd_val_pos: dt 1.5
rnd_val_neg: dt -1.5
saved_cw:    dw 0
rnd_new_cw:  dw 0

; --- Precision-control probe constants (M2.4, research gap K) ---
; 1.0 and 3.0 as tword extended. 1.0/3.0 = 0.333... is inexact in every
; binary precision, so the result mantissa's bit count differs by PC
; mode: the FPU rounds to the PC width before placing in ST(0). A
; 10-byte extended tword store preserves the round-to-PC-width boundary.
prc_one:     dt 1.0
prc_three:   dt 3.0
prc_new_cw:  dw 0

; --- Exception-probe constants (M2.6, research gap M) ---
; neg_one: triggers IE (invalid operation) via FSQRT(-1).
; denormal_value: triggers DE (denormal operand) on FLD. Encoding:
;   80-bit extended with biased exponent = 0 (all zeros) and explicit
;   integer bit = 0 with nonzero fraction. The "unnormal" shape the
;   80387 handles as a denormal.
; scale_pos_20k / scale_neg_20k: FSCALE exponents for triggering OE
;   (2^20000 overflows) and UE (2^-20000 underflows).
neg_one:         dt -1.0
denormal_value:  db 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
scale_pos_20k:   dw 20000
scale_neg_20k:   dw -20000

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

; ---------------------------------------------------------------------
; unsigned int fpu_fp_try_fptan(void)
;
; FPTAN behavioral probe (M2.2, research gap I per docs/FPU Test Research).
;
; On 80387 and later, FPTAN was standardized to ALWAYS push 1.0 onto
; the x87 stack after computing the tangent. Before: ST(0) = θ.
; After:  ST(0) = 1.0, ST(1) = tan(θ). The 1.0 is not the computed
; result; it's a deliberate constant pushed so software can recover
; scaled Y/X by popping.
;
; On 8087 and 80287, FPTAN used the "partial tangent" representation:
; after FPTAN, ST(0) = X (denominator, typically cos(θ) or a scaled
; version) and ST(1) = Y (numerator, sin(θ)-scaled). X is NOT 1.0
; for a general angle (it's 1.0 for θ = 0 as a coincidence of
; cos(0) = 1, which is why we do NOT use 0 here).
;
; Probe: compute tan(0.5), compare ST(0) to 1.0 via FCOMP, return
; FSTSW AX. Caller decodes C3 (bit 14):
;   C3 = 1  ->  ST(0) == 1.0 after FPTAN (80387+ behavior)
;   C3 = 0  ->  ST(0) != 1.0 (8087/287 behavior, or out-of-range
;                             on 387+ with C2 set)
;
; FPTAN on both 80387 and 8087/287 requires |ST(0)| < 2^63 (87/287)
; or |ST(0)| < π/4 (387+). 0.5 < π/4 ≈ 0.785 satisfies both; no
; out-of-range path is entered for our angle.
; ---------------------------------------------------------------------
fpu_fp_try_fptan_:
    push bp
    mov bp, sp

    finit                       ; clean state
    fld tword [cs:fptan_half]   ; ST(0) = 0.5 (our θ)
    fptan                       ; 387+: ST(0)=1.0, ST(1)=tan(0.5)
                                ; 87/287: ST(0)=cos-ish denom ≈ 0.8776
    fld tword [cs:fptan_one]    ; ST(0)=1.0 reference, ST(1)=post-FPTAN top
    fcomp st1                   ; compare ST(0) to ST(1), pop ST(0)
    fnstsw ax                   ; raw status word to AX
    finit                       ; scrub stack regardless of outcome

    pop bp
    retf

; ---------------------------------------------------------------------
; void fpu_fp_probe_rounding(unsigned int mode, int __far *out_pair)
;
; Rounding-control cross-check (M2.3, research gap J per
; docs/FPU Test Research.md).
;
; Sets the FPU control word's RC field (bits 10..11) to the requested
; mode, performs FISTP on +1.5 and -1.5 (both stored to out_pair),
; restores the original CW. Caller runs this 4 times (modes 0..3) and
; verifies the 8 resulting integers match the canonical table.
;
; Mode argument is clamped to 0..3 defensively.
;
; Arg layout (medium-model cdecl, parm caller []):
;   [bp+6]  mode (unsigned int, will be masked to 2 bits)
;   [bp+8]  out_pair offset (int [2] far pointer low word)
;   [bp+10] out_pair segment
;
; out_pair[0] <- FISTP(1.5 under requested RC)
; out_pair[1] <- FISTP(-1.5 under requested RC)
; ---------------------------------------------------------------------
fpu_fp_probe_rounding_:
    push bp
    mov bp, sp

    finit

    fnstcw [cs:saved_cw]
    mov bx, [cs:saved_cw]
    and bx, 0xF3FF              ; clear RC bits 10..11
    mov ax, [bp+6]
    and ax, 0x0003              ; sanitize: mode in 0..3
    mov cl, 10
    shl ax, cl                  ; shift into RC position
    or ax, bx
    mov [cs:rnd_new_cw], ax
    fldcw [cs:rnd_new_cw]

    mov di, [bp+8]
    mov es, [bp+10]

    fld tword [cs:rnd_val_pos]
    fistp word [es:di]

    fld tword [cs:rnd_val_neg]
    fistp word [es:di+2]

    fldcw [cs:saved_cw]

    pop bp
    retf

; ---------------------------------------------------------------------
; void fpu_fp_probe_precision(unsigned int pc_mode,
;                             unsigned char __far *out_10_bytes)
;
; Precision-control cross-check (M2.4, research gap K).
;
; Sets the FPU control word's PC field (bits 8..9) to the requested
; mode, computes 1.0/3.0 (always inexact under any binary precision),
; stores the 10-byte extended result to caller memory, restores CW.
;
; Caller runs this 3 times (modes 0/2/3; mode 1 is reserved) and
; verifies the three 10-byte results differ (which proves PC actually
; changed the precision).
;
; PC modes per Intel docs:
;   00 = single (24-bit significand)
;   01 = reserved
;   10 = double (53-bit significand)
;   11 = extended (64-bit significand, default)
;
; Arg layout (medium-model cdecl):
;   [bp+6]  pc_mode (0, 2, or 3)
;   [bp+8]  out_10_bytes offset
;   [bp+10] out_10_bytes segment
; ---------------------------------------------------------------------
fpu_fp_probe_precision_:
    push bp
    mov bp, sp

    finit

    fnstcw [cs:saved_cw]
    mov bx, [cs:saved_cw]
    and bx, 0xFCFF               ; clear PC bits 8..9
    mov ax, [bp+6]
    and ax, 0x0003               ; sanitize
    mov cl, 8
    shl ax, cl                   ; shift into PC position
    or ax, bx
    mov [cs:prc_new_cw], ax
    fldcw [cs:prc_new_cw]

    fld tword [cs:prc_one]       ; ST(0) = 1.0
    fld tword [cs:prc_three]     ; ST(0) = 3.0, ST(1) = 1.0
    fdivp st1, st0               ; ST(1) = ST(1) / ST(0), pop. ST(0) = 1/3

    mov di, [bp+8]
    mov es, [bp+10]
    fstp tword [es:di]           ; store result + pop

    fldcw [cs:saved_cw]

    pop bp
    retf

; ---------------------------------------------------------------------
; void fpu_fp_probe_exceptions(unsigned int __far *out_6_words)
;
; Exception-flag roundtrip (M2.6, research gap M).
;
; For each of the 6 x87 exceptions (IE DE ZE OE UE PE), triggers a
; known operation that should set that flag, reads FSTSW, stores it.
; Caller inspects each word's expected bit.
;
; out_6_words[0] = SW after FSQRT(-1)          (IE bit 0 expected set)
; out_6_words[1] = SW after FLD denormal       (DE bit 1 expected set)
; out_6_words[2] = SW after 1.0/0              (ZE bit 2 expected set)
; out_6_words[3] = SW after FSCALE(1, +20000)  (OE bit 3 expected set)
; out_6_words[4] = SW after FSCALE(1, -20000)  (UE bit 4 expected set)
; out_6_words[5] = SW after 1.0/3.0            (PE bit 5 expected set)
;
; All 6 exceptions are run with default-masked FPU state (after FINIT,
; all 6 exception masks are set). The flags are sticky; reading them
; via FSTSW does not clear them. FINIT between probes scrubs state.
;
; Arg layout:
;   [bp+6]  out_6_words offset (array of 6 unsigned int = 12 bytes)
;   [bp+8]  out_6_words segment
; ---------------------------------------------------------------------
fpu_fp_probe_exceptions_:
    push bp
    mov bp, sp
    push es
    push di

    mov di, [bp+6]
    mov es, [bp+8]

    ; IE: FSQRT(-1)
    finit
    fld tword [cs:neg_one]
    fsqrt
    fnstsw ax
    mov [es:di+0], ax

    ; DE: load an unnormal (denormal-like) encoding
    finit
    fld tword [cs:denormal_value]
    fnstsw ax
    mov [es:di+2], ax

    ; ZE: 1.0 / 0.0
    finit
    fld1                         ; ST(0) = 1.0
    fldz                         ; ST(0) = 0.0, ST(1) = 1.0
    fdivp st1, st0               ; ST(1) = ST(1) / ST(0) = 1/0 = +inf
    fnstsw ax
    mov [es:di+4], ax

    ; OE: FSCALE with exponent +20000, result = 2^20000 overflow
    finit
    fld1
    fild word [cs:scale_pos_20k]
    fscale                       ; ST(0) = ST(0) * 2^ST(1) = 2^20000
    fnstsw ax
    mov [es:di+6], ax

    ; UE: FSCALE with exponent -20000, result underflow
    finit
    fld1
    fild word [cs:scale_neg_20k]
    fscale                       ; ST(0) = 2^-20000
    fnstsw ax
    mov [es:di+8], ax

    ; PE: 1.0 / 3.0 = 0.333... (always inexact in binary)
    finit
    fld tword [cs:prc_one]
    fld tword [cs:prc_three]
    fdivp st1, st0
    fnstsw ax
    mov [es:di+10], ax

    finit

    pop di
    pop es
    pop bp
    retf
