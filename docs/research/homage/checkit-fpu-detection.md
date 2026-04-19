# CheckIt — FPU detection methodology

**Tool:** CheckIt 3.0, Jan 1991 (`Homage/Checkit/CHECKIT.EXE`)
**Author:** TouchStone Software Corporation, Copyright 1988-1990
**CERBERUS diff target:** `detect_fpu` (`src/detect/fpu.c`,
`src/detect/fpu_a.asm`) and `diag_fpu` (`src/diag/diag_fpu.c`)

## What CheckIt does, observed

CheckIt's reported FPU types (from binary strings at file offsets
0x48f5c onward): `No Coprocessor`, `80287`, `80387`, `80387SX`,
`80486 FPU`, `IIT 80287`, `IIT 80387`. Seven distinct classes.

Static disassembly of the FPU probe region
(image offset 0x16bd7-0x16c55, visible after loading the full
file past the MZ image_size marker) reveals **three distinct
sub-functions**:

### Probe 1 — presence via FNSTCW control-word bit check

```
; near-pseudocode, NOT a direct code quote
; (describes the observed instruction sequence, paraphrased)
FNINIT                         ; reset FPU if present
short delay (loop cx=0x32)
FNSTCW [sentinel_word]         ; dump control word
short delay
cmp byte [sentinel+1], 0x03    ; FNINIT sets CW to 0x037F on a real FPU
jne .no_fpu                    ; high byte of CW != 3 → no FPU wrote it
ax = 1 (present) / 0 (absent)
```

**Rationale:** after `FNINIT`, a live FPU sets the control word
to `0x037F` — the high byte is exactly 3. On a system with no
FPU, the no-wait variants execute as effective no-ops, the
sentinel byte is whatever was there before, and the `cmp byte
[sentinel+1], 3` fails. More robust than a "did the FPU write
anything at all" sentinel check because `0x037F`'s high byte is a
specific post-FNINIT state, not just "anything non-sentinel."

### Probe 2 — functional sanity via FLDZ/FLDZ/FCOMPP + FNSTSW

```
FNINIT
FLDZ ; FLDZ ; FCOMPP              ; compare two zeros, pop both
FNINIT                            ; re-init
FNSTSW [sw_word]
SAHF                              ; load status-word condition bits
                                  ; into flags
check ZF / condition
```

**Rationale:** comparing two zeros should set specific status-
word condition bits. Reading them back via FNSTSW + SAHF lets
CheckIt verify the FPU's compare-and-condition logic works — not
just that it's present, but that it responds to stack operations
correctly.

### Probe 3 — vendor/model discrimination

A cluster of operations mixing `FNINIT`, `FLDZ`/`FLD1`, and what
capstone decodes as `FUCOMI` (opcode 0xDB-family) follows. On a
1991 binary predating the Pentium Pro, `FUCOMI` encoding
(`0xDB 0xEB`) either:
- decodes as **undefined/reserved on Intel 287/387**, different
  behavior on IIT parts — IIT extended the undefined 0xDB
  encodings for extra features, and CheckIt probes those specific
  bytes to detect IIT chips. This matches the presence of the
  `IIT 80287` / `IIT 80387` tag strings in the binary.

The third probe's exact IIT-discrimination bytes are not
reproduced here; the operating principle (probe IIT-reserved
undefined opcodes, observe FPU's response) is the lesson.

### IRQ13 story

`IRQ13-Coprocessor Error` is one of the binary's labeled strings,
referenced from a function that installs a far pointer into
memory locations 0xd072/0xd074 before printf-ing an error. This
is an **IRQ13 handler installation**, not a live-fire test. The
handler is armed during FPU diagnostics to catch coprocessor
errors (unmasked exceptions) that would otherwise fault the
machine. It prints the labeled error and lets CheckIt continue.
CheckIt does not appear to generate deliberate FPU errors in
the main diagnostic path; the IRQ13 hook is a safety net, not a
probe.

## What CERBERUS currently does

`detect_fpu` (fpu.c + fpu_a.asm):

```
; CERBERUS's presence probe (actual code, from fpu_a.asm:36)
mov word [_fpu_sentinel], 5A5Ah
fninit
fnstsw [_fpu_sentinel]           ; note: FNSTSW not FNSTCW
mov ax, [_fpu_sentinel]
cmp ax, 5A5Ah
je .no_fpu
```

**Sentinel approach:** pre-fill the word with `0x5A5A`, run
FNINIT + FNSTSW, see if the word changed. Any change means "some
FPU wrote to it." Simpler than CheckIt's FNSTCW high-byte check
but less specific — a hypothetical FPU that wrote 0x0000 would
be correctly detected (word changed), and so would any other
value.

**Classification:** CERBERUS does not probe the FPU for type;
instead it looks up a class tag via `cpu_get_class()` and maps to
one of `integrated-486`, `387`, `287`, `8087`, `external-unknown`
(see `detect/fpu.c:36-59`). The tag is then resolved via
`fpu_db_lookup` for a friendly name. **No IIT distinction.**
Source comment at `fpu.c:11-13` acknowledges:

> we don't distinguish 287 from 387 or identify Cyrix FasMath vs
> Intel 387 vs IIT — those refinements land as follow-ups.

`diag_fpu` (diag/diag_fpu.c) runs 5 bit-exact known-answer tests
(add, sub, mul, div, compound). No IRQ13 handler installation;
relies on Watcom's `-fpi` compiler setting to produce FPU opcodes
that will fault normally if the FPU misbehaves. CERBERUS runs
under DOS with default IRQ13 → INT 0x75 → default handler (BIOS
beeps / ignores); a genuine FPU fault would manifest as the
result differing from the bit-exact expectation, which
`diag_fpu` catches correctly.

## Gap analysis

**Presence probe: equivalent-function.** Both tools correctly
detect the absence of an FPU on a vanilla 8088/no-socket system,
and the presence of a functional FPU on a 486DX. CheckIt's
FNSTCW-high-byte check is marginally more specific; CERBERUS's
FNSTSW-sentinel works because any post-FNINIT write differs from
`0x5A5A`. **No defect.**

**Functional sanity: different approach, same target.** CheckIt
uses a single FCOMPP-based status-word check. CERBERUS uses five
bit-exact double-precision arithmetic checks. Both catch a
faulty FPU. CERBERUS's approach is more thorough (exercises
FADD/FSUB/FMUL/FDIV plus stack usage via the compound test); it
also has a fault-injection hook for host tests (`force_fail_test_
idx`) that CheckIt doesn't. **No defect.**

**Type classification: CheckIt distinguishes IIT, CERBERUS does
not.** This is a genuine capability gap. CheckIt's IIT
discrimination via undefined-opcode probing is a real feature
that CERBERUS could add. Scope: the IIT 2C87 / 3C87 had non-
trivial market share in 1990-1993 and a CERBERUS run on IIT
hardware currently mis-tags as generic `287` or `387`. Whether
this matters depends on how many IIT-equipped machines enter the
CERBERUS capture corpus.

**IRQ13 live-fire test: neither tool does one in the strict
sense.** CheckIt installs an IRQ13 handler for safety; CERBERUS
doesn't. In practice neither matters for CERBERUS's host-
testable bit-exact diagnostics — the failure mode CERBERUS
guards against is "FPU arithmetic returns wrong bits," not "FPU
fires an unmasked exception we need to catch cleanly."

## Recommended action

**v0.4 / v0.5: none.** CERBERUS `detect_fpu` and `diag_fpu` are
sound for the 486 / Pentium target range, where IIT parts were
already uncommon.

**Deferred follow-up (not in current scope):** if CERBERUS ever
targets hardware from the 287/387 era specifically (8088–386
with discrete FPU socket), add an IIT-probe step. CheckIt's
multi-step probe pattern is the reference model for what that
would look like: presence → functional → vendor discrimination,
three separate probes chained together.

**IRQ13 handler:** do not add. CERBERUS runs host tests under
modern environments where IRQ13 doesn't do what it did on a
1990 PC, and the bit-exact diagnostic already catches FPU fault
modes without needing the interrupt path. CheckIt's IRQ13 hook
is a safety belt for 1990-era machine crashes on unmasked FPU
exceptions; that's not CERBERUS's risk profile.

## Attribution

CheckIt 3.0 (c) 1988-1990 TouchStone Software Corporation.
Observations are from static disassembly of the FPU probe region
in `CHECKIT.EXE` (file offsets 0x16bd7-0x16c55, past the MZ
image_size marker), cross-referenced against published Intel
287/387 and IIT 2C87/3C87 opcode semantics. No decompiled code
reproduced; algorithmic descriptions are paraphrased from
instruction-level reading.
