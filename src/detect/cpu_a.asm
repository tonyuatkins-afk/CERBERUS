; CERBERUS CPU class detection probes
;
; Watcom medium memory model: code segments are far-addressed, so every
; public function here uses RETF. Watcom's default register calling
; convention maps return values into AX for 16-bit int — the only return
; type we use.
;
; Symbol naming: Watcom's C compiler appends a trailing underscore to
; public function identifiers when they are referenced via the register
; calling convention. So C's `cpu_asm_flags_test` links to NASM's
; `cpu_asm_flags_test_`. Storage variables use the same convention.
;
; The INT 6 handler lives in this file (private) and is installed via C
; using _dos_setvect(6, handler) before PUSHFD is attempted. When a 286
; encounters the `db 66h, 9Ch` sequence, INT 6 fires — our handler
; advances the saved IP by 2 bytes (past the invalid 2-byte prefix-opcode
; pair) and IRETs. Execution resumes past PUSHFD with a flag set.
;
; Fault flag is stored in the CS segment so interrupt handling doesn't
; need a valid DS. C-callable getter/setter exposes it.

bits 16

segment cpu_a_TEXT public class=CODE

global  cpu_asm_flags_test_
global  cpu_asm_pushfd_test_
global  cpu_asm_ac_test_
global  cpu_asm_id_test_
global  cpu_asm_int6_handler_
global  cpu_asm_int6_fired_
global  cpu_asm_int6_clear_
global  cpu_asm_cpuid_

; --- CS-local storage (keeps INT 6 handler independent of DS) ---
fault_flag:       dw 0

; ---------------------------------------------------------------------
; int cpu_asm_flags_test(void)
;   Returns 1 if CPU is 286 or later, 0 if 8086/8088 (or V20/V30).
;   No interrupt risk — uses only 16-bit PUSHF/POPF.
;
; Method: FLAGS bits 12-15 are hard-wired to 1 on 8086/8088. On 286+,
; they can be cleared via POPF (except bit 15 on 286 which is reserved
; but still writable in real mode).
; ---------------------------------------------------------------------
cpu_asm_flags_test_:
    pushf                    ; save current FLAGS
    pushf                    ; push again — this one will be modified
    pop ax
    and ax, 0FFFh            ; clear bits 12-15
    push ax
    popf                     ; load modified flags
    pushf                    ; read actual current state
    pop ax
    popf                     ; restore original
    and ax, 0F000h           ; isolate bits 12-15
    cmp ax, 0F000h           ; on 8086, they remain set
    je .is_8086
    mov ax, 1
    retf
.is_8086:
    xor ax, ax
    retf

; ---------------------------------------------------------------------
; INT 6 (invalid opcode) handler.
;
; Pre-conditions:
;   - Installed only while cpu_asm_pushfd_test runs (and any other
;     probe that might execute 386-only instructions on a 286).
;   - Faulting instruction is either "66 9C" (PUSHFD), "66 9D" (POPFD),
;     or our 32-bit flag manipulations — all exactly 2 bytes.
;
; Behavior:
;   - Set fault_flag = 1 (in CS so DS is irrelevant).
;   - Advance saved IP on the stack by 2 bytes so execution resumes
;     past the 2-byte invalid sequence.
;   - IRET.
;
; Stack on entry (after INT 6 pushes FLAGS, CS, IP):
;   [SP+0]  IP
;   [SP+2]  CS
;   [SP+4]  FLAGS
;
; After our "push bp / mov bp, sp":
;   [BP+0]  saved BP
;   [BP+2]  IP   <-- we add 2 to this
;   [BP+4]  CS
;   [BP+6]  FLAGS
; ---------------------------------------------------------------------
cpu_asm_int6_handler_:
    push bp
    mov bp, sp
    push ax
    mov ax, 1
    mov [cs:fault_flag], ax
    add word [bp+2], 2       ; advance saved IP past 2-byte fault
    pop ax
    pop bp
    iret

; ---------------------------------------------------------------------
; int cpu_asm_int6_fired(void)
;   Returns current value of the fault flag. Does not clear.
; ---------------------------------------------------------------------
cpu_asm_int6_fired_:
    mov ax, [cs:fault_flag]
    retf

; ---------------------------------------------------------------------
; void cpu_asm_int6_clear(void)
;   Clears fault flag. Call before each probe that might fault.
; ---------------------------------------------------------------------
cpu_asm_int6_clear_:
    mov word [cs:fault_flag], 0
    retf

; ---------------------------------------------------------------------
; int cpu_asm_pushfd_test(void)
;   Attempts PUSHFD. Returns 1 if successful (386+), 0 if faulted (286).
;
;   REQUIRES: INT 6 handler installed via _dos_setvect(6, cpu_asm_int6_handler).
;   CALLER MUST: call cpu_asm_int6_clear() before this.
;
;   On 386+: PUSHFD pushes 4 bytes of EFLAGS. We balance with add sp, 4.
;   On 286:  the 66h prefix faults, handler skips the 2 bytes, nothing
;            was pushed, so no stack cleanup needed.
; ---------------------------------------------------------------------
cpu_asm_pushfd_test_:
    db 66h, 9Ch              ; PUSHFD — 2 bytes, faults on 286
    mov ax, [cs:fault_flag]
    cmp ax, 0
    jne .faulted
    ; Did not fault — 386+. Clean up the 4 bytes PUSHFD pushed.
    add sp, 4
    mov ax, 1
    retf
.faulted:
    xor ax, ax
    retf

; ---------------------------------------------------------------------
; int cpu_asm_ac_test(void)
;   Returns 1 if AC flag (EFLAGS bit 18) is toggleable — CPU is 486+.
;   Returns 0 if AC is hard-wired to 0 — CPU is 386.
;
;   REQUIRES: Only call after pushfd_test returns 1 (CPU is 386+).
;   Safe without INT 6 handler because PUSHFD/POPFD don't fault on 386+.
; ---------------------------------------------------------------------
cpu_asm_ac_test_:
    pushfd                   ; save original EFLAGS (NASM emits 66 9C)
    pushfd                   ; working copy
    pop eax
    or eax, 40000h           ; set AC (bit 18)
    push eax
    popfd                    ; load modified
    pushfd                   ; read back current EFLAGS
    pop eax
    and eax, 40000h          ; isolate AC
    popfd                    ; restore original
    cmp eax, 0
    je .no_ac
    mov ax, 1
    retf
.no_ac:
    xor ax, ax
    retf

; ---------------------------------------------------------------------
; int cpu_asm_id_test(void)
;   Returns 1 if ID flag (EFLAGS bit 21) is toggleable — CPUID is
;   available. Returns 0 otherwise. Requires 386+ (don't call on 286
;   without INT 6 handler installed).
; ---------------------------------------------------------------------
cpu_asm_id_test_:
    pushfd
    pushfd
    pop eax
    mov ebx, eax             ; save original
    xor eax, 200000h         ; flip ID (bit 21)
    push eax
    popfd
    pushfd
    pop eax
    xor eax, ebx             ; what changed?
    and eax, 200000h         ; isolate ID change
    popfd                    ; restore original
    cmp eax, 0
    je .no_id
    mov ax, 1
    retf
.no_id:
    xor ax, ax
    retf

; ---------------------------------------------------------------------
; void cpu_asm_cpuid(unsigned long leaf, cpuid_regs_t __far *out)
;   Execute CPUID with EAX=leaf, write EAX/EBX/ECX/EDX into *out.
;   REQUIRES: caller already verified CPUID availability via cpu_asm_id_test.
;
;   Watcom cdecl via #pragma aux on the C side. Medium-model far call.
;   Arg layout on stack:
;       [bp+0]  saved bp
;       [bp+2]  return IP
;       [bp+4]  return CS
;       [bp+6]  leaf low  16 bits
;       [bp+8]  leaf high 16 bits
;       [bp+10] out pointer offset (16-bit)
;       [bp+12] out pointer segment (16-bit) — far pointer
; ---------------------------------------------------------------------
cpu_asm_cpuid_:
    push bp
    mov bp, sp
    push ebx
    push ecx
    push edx
    push edi
    push es

    mov eax, [bp+6]          ; 32-bit leaf (NASM emits 66 prefix)
    cpuid

    ; Store via far pointer ES:DI (caller passed far ptr via cdecl)
    mov di, [bp+10]          ; offset
    mov es, [bp+12]          ; segment
    mov [es:di+0],  eax
    mov [es:di+4],  ebx
    mov [es:di+8],  ecx
    mov [es:di+12], edx

    pop es
    pop edi
    pop edx
    pop ecx
    pop ebx
    pop bp
    retf
