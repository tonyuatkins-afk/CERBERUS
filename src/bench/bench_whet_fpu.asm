; =====================================================================
; CERBERUS Whetstone FPU kernel — x87 assembly (NASM)
;
; Curnow-Wichmann 1976 Whetstone benchmark, hand-coded for 287+/387/486
; x87 FPUs. Replaces the Watcom-compiled C kernel on FPU-equipped
; systems so the inner loops use x87 register state directly rather
; than volatile-spilled memory traffic.
;
; Public entry:
;   void whet_fpu_run_units(void);
;
; Reads `whet_fpu_units` from DGROUP. Runs that many Whetstone "units"
; (each unit = one pass through all 11 modules). Writes final
; accumulator state to the external whet_fpu_* globals for the
; bench_whetstone.c checksum observer to read.
;
; Calling convention: Watcom medium model, RETF. No register args
; (all inputs and outputs through DGROUP globals). Uses the x87 stack
; plus AX/BX/CX/DX/SI/DI; preserves BP.
;
; Module inventory — iteration counts match the reference C kernel
; (run_whetstone_units in bench_whetstone.c):
;   N1 =   0  (omitted — no work in reference)
;   N2 =  12  Array element arithmetic
;   N3 =  14  Array-as-parameter rotation, 6 inner passes each
;   N4 = 345  Conditional jumps (integer)
;   N6 = 210  Integer + double array
;   N7 =  32  Transcendental (sin/cos/atan)
;   N8 = 899  Procedure calls with FPU
;   N9 = 616  Integer array references
;   N10=   7  Integer arithmetic
;   N11=  93  sqrt/exp/log
;
; Transcendentals use x87 native instructions: FSIN, FCOS, FPATAN,
; FSQRT, F2XM1 (exp building block), FYL2X (log building block).
; All present on 387+ and 486.
;
; x87 stack discipline: each module entry assumes an empty stack;
; each exit leaves it empty.
; =====================================================================

bits 16

; ---------------------------------------------------------------------
; Data — DGROUP globals visible to C via bench_whetstone.c extern decls.
; Watcom prefixes C data names with '_' so C's `whet_fpu_units`
; links to NASM's `_whet_fpu_units`.
; ---------------------------------------------------------------------
segment _DATA public class=DATA

global _whet_fpu_units
global _whet_fpu_E1
global _whet_fpu_J, _whet_fpu_K, _whet_fpu_L
global _whet_fpu_X1, _whet_fpu_X2, _whet_fpu_X3, _whet_fpu_X4
global _whet_fpu_X,  _whet_fpu_Y,  _whet_fpu_Z

_whet_fpu_units:  dd 0
_whet_fpu_E1:     dq 0.0, 0.0, 0.0, 0.0
_whet_fpu_J:      dw 0
_whet_fpu_K:      dw 0
_whet_fpu_L:      dw 0
_whet_fpu_X1:     dq 0.0
_whet_fpu_X2:     dq 0.0
_whet_fpu_X3:     dq 0.0
_whet_fpu_X4:     dq 0.0
_whet_fpu_X:      dq 0.0
_whet_fpu_Y:      dq 0.0
_whet_fpu_Z:      dq 0.0

; Reference constants
whet_fpu_const_T:   dq 0.499975
whet_fpu_const_T1:  dq 0.50025
whet_fpu_const_T2:  dq 2.0
whet_fpu_const_1:   dq 1.0
whet_fpu_const_m1:  dq -1.0
whet_fpu_const_05:  dq 0.5
whet_fpu_const_075: dq 0.75
whet_fpu_const_2:   dq 2.0
whet_fpu_const_3:   dq 3.0

; Integer scratch for FILD (x87 integer loads need a memory source).
whet_fpu_i16_scratch: dw 0

; ---------------------------------------------------------------------
; Code segment
; ---------------------------------------------------------------------
segment bench_whet_fpu_TEXT public class=CODE

global whet_fpu_run_units_

; ---------------------------------------------------------------------
; whet_fpu_run_units_
;
; Reads _whet_fpu_units, runs that many Whetstone units, writes final
; accumulator state to _whet_fpu_{X1..X4,X,Y,Z,E1,J,K,L}.
; ---------------------------------------------------------------------
whet_fpu_run_units_:
    push bp
    mov  bp, sp
    push si
    push di

    ; Outer counter in a local on the stack: [bp-4..bp-2].
    ; Modules freely clobber CX/SI/DI/AX/BX/DX.
    sub  sp, 4
    mov  ax, [_whet_fpu_units]
    mov  dx, [_whet_fpu_units+2]
    mov  [bp-4], ax
    mov  [bp-2], dx

.outer_loop:
    mov  ax, [bp-4]
    or   ax, [bp-2]
    jz   .outer_done

    ; ==================================================================
    ; Module 1: N1 = 0, no inner work. Seed the X1..X4 accumulators
    ; so the final checksum has deterministic values.
    ; ==================================================================
    fld  qword [whet_fpu_const_1]
    fstp qword [_whet_fpu_X1]
    fld  qword [whet_fpu_const_m1]
    fstp qword [_whet_fpu_X2]
    fld  qword [whet_fpu_const_m1]
    fstp qword [_whet_fpu_X3]
    fld  qword [whet_fpu_const_m1]
    fstp qword [_whet_fpu_X4]

    ; ==================================================================
    ; Module 2: N2 = 12. E1 array chain arithmetic.
    ;   E1[0] = ( E1[0] + E1[1] + E1[2] - E1[3]) * T
    ;   E1[1] = ( E1[0] + E1[1] - E1[2] + E1[3]) * T
    ;   E1[2] = ( E1[0] - E1[1] + E1[2] + E1[3]) * T
    ;   E1[3] = (-E1[0] + E1[1] + E1[2] + E1[3]) * T
    ; Seed: E1 = [1.0, -1.0, -1.0, -1.0].
    ; ==================================================================
    fld  qword [whet_fpu_const_1]
    fstp qword [_whet_fpu_E1]
    fld  qword [whet_fpu_const_m1]
    fstp qword [_whet_fpu_E1+8]
    fld  qword [whet_fpu_const_m1]
    fstp qword [_whet_fpu_E1+16]
    fld  qword [whet_fpu_const_m1]
    fstp qword [_whet_fpu_E1+24]

    mov  cx, 12
.m2_loop:
    fld  qword [_whet_fpu_E1]
    fadd qword [_whet_fpu_E1+8]
    fadd qword [_whet_fpu_E1+16]
    fsub qword [_whet_fpu_E1+24]
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_E1]

    fld  qword [_whet_fpu_E1]
    fadd qword [_whet_fpu_E1+8]
    fsub qword [_whet_fpu_E1+16]
    fadd qword [_whet_fpu_E1+24]
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_E1+8]

    fld  qword [_whet_fpu_E1]
    fsub qword [_whet_fpu_E1+8]
    fadd qword [_whet_fpu_E1+16]
    fadd qword [_whet_fpu_E1+24]
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_E1+16]

    fld  qword [_whet_fpu_E1+24]
    fadd qword [_whet_fpu_E1+16]
    fadd qword [_whet_fpu_E1+8]
    fsub qword [_whet_fpu_E1]
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_E1+24]

    loop .m2_loop

    ; ==================================================================
    ; Module 3: N3 = 14. PA(E) called N3 times. PA does 6 inner
    ; passes each, with the last assignment using division by T2
    ; instead of multiplication by T. Flattened (no call overhead).
    ; ==================================================================
    mov  si, 14
.m3_outer:
    mov  cx, 6
.m3_inner:
    fld  qword [_whet_fpu_E1]
    fadd qword [_whet_fpu_E1+8]
    fadd qword [_whet_fpu_E1+16]
    fsub qword [_whet_fpu_E1+24]
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_E1]

    fld  qword [_whet_fpu_E1]
    fadd qword [_whet_fpu_E1+8]
    fsub qword [_whet_fpu_E1+16]
    fadd qword [_whet_fpu_E1+24]
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_E1+8]

    fld  qword [_whet_fpu_E1]
    fsub qword [_whet_fpu_E1+8]
    fadd qword [_whet_fpu_E1+16]
    fadd qword [_whet_fpu_E1+24]
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_E1+16]

    fld  qword [_whet_fpu_E1+24]
    fadd qword [_whet_fpu_E1+16]
    fadd qword [_whet_fpu_E1+8]
    fsub qword [_whet_fpu_E1]
    fdiv qword [whet_fpu_const_T2]
    fstp qword [_whet_fpu_E1+24]

    loop .m3_inner
    dec  si
    jnz  .m3_outer

    ; ==================================================================
    ; Module 4: N4 = 345. Integer conditional jumps, no FPU.
    ;   J = 1
    ;   loop: if (J==1) J=2 else J=3
    ;         if (J>2)  J=0 else J=1
    ;         if (J<1)  J=1 else J=0
    ; ==================================================================
    mov  bx, 1
    mov  cx, 345
.m4_loop:
    cmp  bx, 1
    jne  .m4_b1_else
    mov  bx, 2
    jmp  .m4_b1_done
.m4_b1_else:
    mov  bx, 3
.m4_b1_done:
    cmp  bx, 2
    jle  .m4_b2_else
    mov  bx, 0
    jmp  .m4_b2_done
.m4_b2_else:
    mov  bx, 1
.m4_b2_done:
    cmp  bx, 1
    jge  .m4_b3_else
    mov  bx, 1
    jmp  .m4_b3_done
.m4_b3_else:
    mov  bx, 0
.m4_b3_done:
    loop .m4_loop
    mov  [_whet_fpu_J], bx

    ; Module 5: omitted per reference.

    ; ==================================================================
    ; Module 6: N6 = 210. Integer arithmetic with FPU store.
    ;   J = 1; K = 2; L = 3;
    ;   for (I = 1; I <= N6; I++) {
    ;     J = J * (K - J) * (L - K);
    ;     K = L * K - (L - J) * K   ≡   K * J  (identity)
    ;     L = (L - K) * (K + J);
    ;     E1[L - 2] = (double)(J + K + L);
    ;     E1[K - 2] = (double)(J * K * L);
    ;   }
    ;
    ; Integer state lives in memory (_whet_fpu_J/K/L) so imul's dx
    ; clobber doesn't force a dance. di holds J_new, si holds K_new.
    ;
    ; Fixed-point analysis: with J=1,K=2,L=3 the three formulas
    ; evaluate to J=1, K=2, L=3 identically, so the E1 writes land
    ; at E1[1] and E1[0] every iteration — always in bounds.
    ; ==================================================================
    mov  word [_whet_fpu_J], 1
    mov  word [_whet_fpu_K], 2
    mov  word [_whet_fpu_L], 3
    mov  cx, 210
.m6_loop:
    push cx                          ; save outer counter

    ; J_new = J * (K - J) * (L - K)
    mov  ax, [_whet_fpu_J]
    mov  bx, [_whet_fpu_K]
    mov  cx, bx
    sub  cx, ax                      ; cx = K - J
    imul cx                          ; ax = J * (K - J)
    mov  cx, [_whet_fpu_L]
    sub  cx, [_whet_fpu_K]           ; cx = L - K
    imul cx                          ; ax = J * (K-J) * (L-K)
    mov  di, ax                      ; di = J_new

    ; K_new = K * J   (identity: L*K - (L-J)*K = K*J)
    mov  ax, [_whet_fpu_K]
    imul word [_whet_fpu_J]          ; ax = K_old * J_old
    mov  si, ax                      ; si = K_new

    ; L_new = (L - K) * (K + J)
    mov  ax, [_whet_fpu_L]
    sub  ax, [_whet_fpu_K]           ; ax = L - K
    mov  cx, [_whet_fpu_K]
    add  cx, [_whet_fpu_J]           ; cx = K + J
    imul cx                          ; ax = L_new

    ; Commit new J/K/L to memory
    mov  [_whet_fpu_L], ax
    mov  [_whet_fpu_J], di
    mov  [_whet_fpu_K], si

    ; E1[L - 2] = (double)(J + K + L)  — using NEW J/K/L
    mov  ax, [_whet_fpu_J]
    add  ax, [_whet_fpu_K]
    add  ax, [_whet_fpu_L]
    mov  [whet_fpu_i16_scratch], ax
    fild word [whet_fpu_i16_scratch]  ; ST0 = (double)(J+K+L)
    mov  ax, [_whet_fpu_L]
    sub  ax, 2
    shl  ax, 3                       ; ax = 8 * (L-2) byte offset
    mov  bx, ax
    fstp qword [_whet_fpu_E1 + bx]

    ; E1[K - 2] = (double)(J * K * L)
    mov  ax, [_whet_fpu_J]
    imul word [_whet_fpu_K]          ; ax = J * K
    imul word [_whet_fpu_L]          ; ax = J * K * L
    mov  [whet_fpu_i16_scratch], ax
    fild word [whet_fpu_i16_scratch]
    mov  ax, [_whet_fpu_K]
    sub  ax, 2
    shl  ax, 3
    mov  bx, ax
    fstp qword [_whet_fpu_E1 + bx]

    pop  cx                          ; restore outer counter
    ; Module 6 body exceeds the 128-byte short-jump reach of LOOP, so
    ; use a near jump manually. DEC + JZ-then-JMP replaces the single
    ; LOOP instruction.
    dec  cx
    jz   .m6_done
    jmp  .m6_loop
.m6_done:

    ; ==================================================================
    ; Module 7: N7 = 32. Trigonometric via FSIN, FCOS, FPATAN.
    ;   X = 0.5; Y = 0.5;
    ;   for (I = 1; I <= N7; I++) {
    ;     X = T * atan(T2 * sin(X) * cos(X) / (cos(X+Y) + cos(X-Y) - 1.0));
    ;     Y = T * atan(T2 * sin(Y) * cos(Y) / (cos(X+Y) + cos(X-Y) - 1.0));
    ;   }
    ; ==================================================================
    fld  qword [whet_fpu_const_05]
    fstp qword [_whet_fpu_X]
    fld  qword [whet_fpu_const_05]
    fstp qword [_whet_fpu_Y]

    mov  cx, 32
.m7_loop:
    ; X update
    fld  qword [_whet_fpu_X]
    fsin                             ; ST0 = sin(X)
    fld  qword [_whet_fpu_X]
    fcos                             ; ST1=sin(X), ST0=cos(X)
    fmulp st1, st0                   ; ST0 = sin(X)*cos(X)
    fmul qword [whet_fpu_const_T2]   ; ST0 = T2 * sin*cos  (numerator)

    fld  qword [_whet_fpu_X]
    fadd qword [_whet_fpu_Y]
    fcos                             ; ST0=cos(X+Y), ST1=num
    fld  qword [_whet_fpu_X]
    fsub qword [_whet_fpu_Y]
    fcos                             ; ST0=cos(X-Y), ST1=cos(X+Y), ST2=num
    faddp st1, st0                   ; ST0=cos(X+Y)+cos(X-Y), ST1=num
    fsub qword [whet_fpu_const_1]    ; ST0=denom, ST1=num

    fdivp st1, st0                   ; ST0 = num/denom
    fld1                             ; ST0=1, ST1=num/denom
    fpatan                           ; ST0 = atan((num/denom)/1) = atan(num/denom)
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_X]

    ; Y update (same pattern, using new X)
    fld  qword [_whet_fpu_Y]
    fsin
    fld  qword [_whet_fpu_Y]
    fcos
    fmulp st1, st0
    fmul qword [whet_fpu_const_T2]

    fld  qword [_whet_fpu_X]
    fadd qword [_whet_fpu_Y]
    fcos
    fld  qword [_whet_fpu_X]
    fsub qword [_whet_fpu_Y]
    fcos
    faddp st1, st0
    fsub qword [whet_fpu_const_1]

    fdivp st1, st0
    fld1
    fpatan
    fmul qword [whet_fpu_const_T]
    fstp qword [_whet_fpu_Y]

    loop .m7_loop

    ; ==================================================================
    ; Module 8: N8 = 899. Procedure calls with FPU. P3 inlined.
    ;   X = 1.0; Y = 1.0; Z = 1.0;
    ;   for (I = 1; I <= N8; I++) P3(X, Y, &Z);
    ; P3(XX, YY, *ZZ) {
    ;   X_Loc = T * (XX + YY);
    ;   Y_Loc = T * (X_Loc + YY);
    ;   *ZZ = (X_Loc + Y_Loc) / T2;
    ; }
    ;
    ; X and Y don't change across iterations (passed by value),
    ; so the computation is deterministic per-iter. Z is the only
    ; output.
    ; ==================================================================
    fld  qword [whet_fpu_const_1]
    fstp qword [_whet_fpu_X]
    fld  qword [whet_fpu_const_1]
    fstp qword [_whet_fpu_Y]
    fld  qword [whet_fpu_const_1]
    fstp qword [_whet_fpu_Z]

    mov  cx, 899
.m8_loop:
    ; X_Loc = T * (X + Y)
    fld  qword [_whet_fpu_X]
    fadd qword [_whet_fpu_Y]
    fmul qword [whet_fpu_const_T]    ; ST0 = X_Loc
    ; Y_Loc = T * (X_Loc + Y)
    fld  st0                         ; ST0=X_Loc, ST1=X_Loc
    fadd qword [_whet_fpu_Y]
    fmul qword [whet_fpu_const_T]    ; ST0 = Y_Loc, ST1 = X_Loc
    ; *Z = (X_Loc + Y_Loc) / T2
    faddp st1, st0                   ; ST0 = X_Loc + Y_Loc
    fdiv qword [whet_fpu_const_T2]
    fstp qword [_whet_fpu_Z]

    loop .m8_loop

    ; ==================================================================
    ; Module 9: N9 = 616. Integer-indexed array shuffle.
    ;   J = 1; K = 2; L = 3;
    ;   E1[0]=1.0; E1[1]=2.0; E1[2]=3.0;
    ;   for (I = 1; I <= N9; I++) P0();
    ; P0: E1[J]=E1[K]; E1[K]=E1[L]; E1[L]=E1[J];
    ;
    ; With J,K,L fixed the three moves are constant offsets.
    ; ==================================================================
    fld  qword [whet_fpu_const_1]
    fstp qword [_whet_fpu_E1]
    fld  qword [whet_fpu_const_2]
    fstp qword [_whet_fpu_E1+8]
    fld  qword [whet_fpu_const_3]
    fstp qword [_whet_fpu_E1+16]
    mov  word [_whet_fpu_J], 1
    mov  word [_whet_fpu_K], 2
    mov  word [_whet_fpu_L], 3

    mov  cx, 616
.m9_loop:
    ; E1[1] = E1[2]
    fld  qword [_whet_fpu_E1+16]
    fstp qword [_whet_fpu_E1+8]
    ; E1[2] = E1[3]
    fld  qword [_whet_fpu_E1+24]
    fstp qword [_whet_fpu_E1+16]
    ; E1[3] = E1[1]
    fld  qword [_whet_fpu_E1+8]
    fstp qword [_whet_fpu_E1+24]
    loop .m9_loop

    ; ==================================================================
    ; Module 10: N10 = 7. Integer arithmetic.
    ;   J = 2; K = 3;
    ;   for (I = 1; I <= 7; I++) {
    ;     J = J + K; K = J + K; J = K - J; K = K - J - J;
    ;   }
    ; ==================================================================
    mov  ax, 2
    mov  bx, 3
    mov  cx, 7
.m10_loop:
    add  ax, bx                      ; J = J + K
    add  bx, ax                      ; K = J + K  (new J)
    mov  dx, bx
    sub  dx, ax                      ; dx = K - J
    mov  ax, dx                      ; J = K - J
    sub  bx, ax                      ; K = K - J
    sub  bx, ax                      ;       - J
    loop .m10_loop
    mov  [_whet_fpu_J], ax
    mov  [_whet_fpu_K], bx

    ; ==================================================================
    ; Module 11: N11 = 93. Standard-library transcendentals.
    ;   X = 0.75;
    ;   for (I = 1; I <= N11; I++) X = sqrt(exp(log(X) / T1));
    ;
    ; log(X) = ln(X) via FYL2X with y = ln(2):
    ;   ln(2) * log2(X) = ln(X)
    ;
    ; exp(arg) via F2XM1 + FSCALE:
    ;   exp(arg) = 2^(arg * log2(e))
    ;   z = arg * log2(e)
    ;   split z = int(z) + frac(z), frac in [-1,+1]
    ;   2^z = 2^int * (2^frac-1 + 1)
    ;
    ; sqrt: FSQRT (native).
    ; ==================================================================
    fld  qword [whet_fpu_const_075]
    fstp qword [_whet_fpu_X]

    mov  cx, 93
.m11_loop:
    ; ln(X)
    fldln2                           ; ST0 = ln(2)
    fld  qword [_whet_fpu_X]         ; ST0 = X, ST1 = ln(2)
    fyl2x                            ; ST0 = ln(2) * log2(X) = ln(X)

    fdiv qword [whet_fpu_const_T1]   ; ST0 = ln(X) / T1 = arg

    ; z = arg * log2(e)
    fldl2e                           ; ST0 = log2(e), ST1 = arg
    fmulp st1, st0                   ; ST0 = z

    ; Split z into int(z) + frac(z)
    fld  st0                         ; ST0=z, ST1=z
    frndint                          ; ST0 = int(z), ST1 = z
    fxch st1                         ; ST0 = z, ST1 = int(z)
    fsub st0, st1                    ; ST0 = frac, ST1 = int(z)
    f2xm1                            ; ST0 = 2^frac - 1
    fld1
    faddp st1, st0                   ; ST0 = 2^frac, ST1 = int(z)
    fscale                           ; ST0 = 2^frac * 2^int(z) = 2^z = exp(arg)
    fstp st1                         ; drop int(z); ST0 = exp(arg)

    fsqrt                            ; ST0 = sqrt(exp(arg))
    fstp qword [_whet_fpu_X]

    loop .m11_loop

    ; ------------------------------------------------------------------
    ; End of unit. Decrement outer 32-bit counter.
    ; ------------------------------------------------------------------
    mov  ax, [bp-4]
    mov  dx, [bp-2]
    sub  ax, 1
    sbb  dx, 0
    mov  [bp-4], ax
    mov  [bp-2], dx
    jmp  .outer_loop

.outer_done:
    add  sp, 4
    pop  di
    pop  si
    pop  bp
    retf
