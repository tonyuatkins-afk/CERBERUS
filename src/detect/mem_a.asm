; CERBERUS XMS extended-memory query
;
; HIMEM.SYS hooks INT 15h AH=88h and returns 0 extended memory so that
; DOS clients go through the XMS driver instead. Our INT 15h probe
; therefore reports 0 on a 64MB machine that has HIMEM loaded. The fix
; is to call the XMS entry point directly: acquire via INT 2Fh AX=4310h
; (returns ES:BX = FAR entry point), then invoke with AH=08h (Query
; Free Extended Memory).
;
; C binding (xms_query_free takes two near pointers to unsigned long,
; returns 1 on success, 0 on failure):
;
;   extern int xms_query_free_(unsigned long *out_largest_kb,
;                              unsigned long *out_total_kb);
;
; Watcom medium model + register calling convention: first pointer arg
; arrives in AX, second in DX (DS-relative offsets because pointers to
; objects in the DGROUP are near). Return value in AX. Function uses
; RETF (far call from C medium-model caller).
;
; Symbol naming: Watcom appends a trailing underscore to register-calling
; public symbols.

bits 16

segment mem_a_TEXT public class=CODE

global  xms_query_free_

; CS-local storage so ES:BX stash survives calls into other segments.
xms_entry_off:    dw 0
xms_entry_seg:    dw 0

; ---------------------------------------------------------------------
; int xms_query_free_(unsigned long *out_largest_kb,
;                     unsigned long *out_total_kb)
;
; Parm 1 (AX): near pointer (DS-relative) to unsigned long receiving
;              largest free contiguous block in KB.
; Parm 2 (DX): near pointer (DS-relative) to unsigned long receiving
;              total free extended memory in KB (XMS 2.0+; falls back
;              to the largest value if DX returns 0).
;
; Returns AX = 1 on success, 0 on failure (no XMS, no free memory, or
; XMS error code in BL).
;
; Modifies: AX, BX, CX, DX, ES (saved/restored where meaningful).
; ---------------------------------------------------------------------
xms_query_free_:
    push    ds               ; preserve caller's DS (we leave it intact)
    push    si
    push    di
    push    bp
    mov     bp, sp            ; frame for possible locals (none right now)

    ; --- Stash the two parm pointers so we can blow away AX/DX during
    ; --- the INT 2Fh / XMS entry call sequence.
    mov     cx, ax            ; CX = out_largest_kb
    mov     si, dx            ; SI = out_total_kb

    ; --- Acquire XMS entry point: INT 2Fh AX=4310h ---
    push    es                ; INT 2F clobbers ES:BX with the entry
    mov     ax, 4310h
    int     2Fh
    mov     word [cs:xms_entry_off], bx
    mov     word [cs:xms_entry_seg], es
    pop     es

    ; --- Is the entry point valid? A zero seg:off means XMS absent. ---
    mov     ax, word [cs:xms_entry_off]
    or      ax, word [cs:xms_entry_seg]
    jnz     .have_entry
    xor     ax, ax            ; no XMS → return 0
    jmp     .done

.have_entry:
    ; --- Call the XMS entry with AH=08h (Query Free Extended Memory).
    ; --- Returns: AX = largest free KB, DX = total free KB (XMS 2.0+),
    ; --- BL = error code (when AX == 0).
    mov     ah, 08h
    call    far [cs:xms_entry_off]

    ; Watcom's `call far [mem]` encoding: the assembler emits EA+m16:16
    ; indirect FAR CALL, which reads 4 bytes (offset then segment) from
    ; the given memory operand. xms_entry_off is followed by
    ; xms_entry_seg in memory, so the FAR pointer is well-formed.

    ; AX = largest, DX = total. Any success leaves AX nonzero.
    test    ax, ax
    jnz     .success
    xor     ax, ax            ; largest == 0 → XMS reported "none free"
    jmp     .done

.success:
    ; --- Store largest into *out_largest_kb (as unsigned long: LSW=AX, MSW=0). ---
    mov     bx, cx            ; BX = &largest
    mov     [bx], ax
    mov     word [bx+2], 0

    ; --- Store total into *out_total_kb (LSW=DX, MSW=0). If DX==0 the
    ; --- driver is XMS <2.0 and only reports largest-contiguous; mirror
    ; --- largest into total so the caller gets a usable value.
    test    dx, dx
    jnz     .have_total
    mov     dx, ax
.have_total:
    mov     bx, si            ; BX = &total
    mov     [bx], dx
    mov     word [bx+2], 0

    mov     ax, 1             ; return 1 on success

.done:
    pop     bp
    pop     di
    pop     si
    pop     ds
    retf
