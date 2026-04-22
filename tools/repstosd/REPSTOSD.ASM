; REPSTOSD.ASM - hot loop for issue #6 diagnostic quartet
;
; Single exported function that writes one mode 13h frame (16000
; dwords = 64000 bytes) to A000:0000 using REP STOSD. Called
; repeatedly from REPSTOSD.C to accumulate a 100 MB write workload
; with measurable elapsed time on any hardware in the
; 4 MB/s to 60 MB/s range.
;
; Assembled with NASM 2.x, linked against Watcom small-model C
; (see the build script for wmake invocation). Name export follows
; Watcom __cdecl convention: trailing underscore on the symbol.
;
; Uses 32-bit eax/ecx in a 16-bit code segment. NASM emits the
; 0x66 operand-size override automatically when 32-bit register
; names appear, so the actual instruction stream contains the
; real REP STOSD encoding (0x66 0xF3 0xAB) that the 486+ executes
; as a dword store per iteration.

        bits 16

section _TEXT class=CODE public align=1 use16

        global  rep_stosd_frame_

; void rep_stosd_frame(void)
;   ES = A000 on entry is NOT assumed; function reloads ES.
;   Writes 16000 dwords (64000 bytes) starting at ES:0000.
;   Uses eax/ecx/di; preserves nothing else (caller saves).
rep_stosd_frame_:
        push    es
        mov     ax, 0A000h
        mov     es, ax
        xor     di, di
        mov     eax, 55AA55AAh
        mov     ecx, 16000
        cld
        rep     stosd
        pop     es
        ret
