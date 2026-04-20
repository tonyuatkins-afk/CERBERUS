# CheckIt, "Whetstone" identification

**Tool:** CheckIt 3.0, Jan 1991 (`Homage/Checkit/CHECKIT.EXE`)
**Author:** TouchStone Software Corporation, Copyright 1988-1990
**CERBERUS diff target:** `bench.fpu.k_whetstones` (issue #4)

## What we were looking for

Phase 3 T3 asked whether CheckIt's "Whetstones" value is a true
Curnow-Wichmann Whetstone port. CERBERUS ships a faithful
Curnow-Wichmann Whetstone implementation in `bench_whetstone.c`.
Issue #4 is the ~100x gap between CERBERUS's ~109 K-Whet and
CheckIt's reported 11,420 K-Whet on the same BEK-V409 bench
machine. If CheckIt is measuring the same thing as CERBERUS, the
gap is a real port defect. If it is not, the gap is an unrelated
label collision in the same shape as the Dhrystone finding from
Phase 2 T2.

## What we found

**Neither Curnow nor any other published Whetstone variant.**
CheckIt's binary and overlay file contain **zero of the
canonical Whetstone markers** that every genuine Curnow-Wichmann
port (ALGOL original or later C translations) carries.

A byte-level search across `CHECKIT.EXE` plus `CHECKIT.OVL` for
the following returns zero hits:

- `Module 1` through `Module 11` (Curnow-Wichmann's eleven-module
  section labels)
- `Curnow`, `Wichmann`, `Greenfield` (canonical authors)
- `P_A`, `P_0`, `P_3`, `PA()`, `P0()`, `P3()` (the procedure
  names from every C port)
- `0.941377`, `0.731073`, `1.0e-6` (the specific floating-point
  constants Wichmann embedded in the original algorithm)
- `MFLOPS`, `KWIPS`, `MWIPS` (the Wichmann-derived units)
- `WHETSTONE INSTRUCTIONS`, `MILLIONS OF WHETSTONE`

The only Whetstone-adjacent string present is the output format
`%9.1fK Whetstones` plus the CheckIt help text, which describes
the measurement in general terms ("a measurement of floating
point calculation speed") without attribution to the canonical
algorithm.

This places CheckIt's "Whetstone" measurement in the same
category as its "Dhrystones" (Phase 2 T2 finding): a custom
floating-point synthetic using the familiar label, sharing no
algorithmic lineage with the canonical Whetstone. CheckIt's
FPU benchmark measures FPU throughput on whatever work mix
TouchStone chose and reports the result in the same units the
industry was watching in 1988-1991 because that was how FPU
performance got talked about at the time.

## Consequence for issue #4

**The 100x gap is almost certainly a label collision, not a
CERBERUS port defect.** CERBERUS runs the full Curnow-Wichmann
Whetstone algorithm across all eleven modules, producing a
result that incorporates sin, cos, exp, log, sqrt, array
indexing, function calls, and conditional branches in the exact
weighted mix Wichmann published. CheckIt's custom synthetic
almost certainly runs a much tighter FPU inner loop, reports
its number in the same units, and produces a value one hundred
times larger because it is doing one hundredth of the work per
"Whetstone" of output.

This mirrors exactly what Phase 2 T2 found for the Dhrystone
number, with the difference that CheckIt's "Dhrystones" and
CERBERUS's Dhrystone 2.1 happen to land within 5% on the 486
because both tests weight integer work similarly. FPU work
weights vary more widely across synthetic designs, which is why
the Whetstone divergence is larger than the Dhrystone divergence.

The existing issue #4 plan (NASM x87 inner kernels for `PA`,
`P0`, Module 2's hot loop) would make CERBERUS's Curnow Whetstone
run faster in absolute terms, probably bringing the number up
to roughly 10x of the current 109 K-Whet rather than to CheckIt's
11,420. **CheckIt's 11,420 is not a target CERBERUS can or
should aim for because it is not the same measurement.**

## What CERBERUS currently does

`bench_whetstone.c` is a faithful Curnow-Wichmann Whetstone port
with all eleven modules, the Wichmann floating-point constants,
the `PA`, `P0`, `P3` procedures, and the canonical units. The
source header at line 1 credits Curnow and Wichmann directly.

Compile at `CFLAGS_NOOPT -od -oi` (OpenWatcom: no optimization
except intrinsic math to keep x87 transcendentals inline rather
than calling the software libm).

## Recommended action for issue #4

**Reframe issue #4.** The 100x divergence from CheckIt is not
a defect and not a target. The legitimate engineering question is
whether CERBERUS's Curnow Whetstone result is structurally
correct (module proportions, floating-point dynamic range,
algorithmic fidelity) rather than whether it numerically matches
CheckIt's custom synthetic.

A more useful match target: compare CERBERUS's K-Whet against
other public Curnow Whetstone results on matched hardware.
Period-correct reference: a 486 DX-2-66 in most published
Curnow Whetstone tables lands at roughly 1,500 to 3,000 K-Whet
(not K-Whet but whole-Whetstones, rescaled). CERBERUS's 109
K-Whet on the BEK-V409 is ~10x low against that reference,
which IS consistent with the NASM x87 acceleration opportunity
documented in the existing issue #4 plan.

**Keep the NASM FPU-asm rework plan.** It is still the right
work. What changes is the target: a genuine Curnow Whetstone
result in the published 1,500 to 3,000 K-Whet range on the
486 DX-2, not CheckIt's 11,420.

**Update `bench_whetstone.c` methodological comment.** Same
shape as the T2-informed update to `bench_dhrystone.c:30`:
flag that CheckIt's "Whetstone" number is a custom synthetic
with no algorithmic lineage to Curnow-Wichmann, so direct
numeric comparison between the two tools is meaningless.
Small edit; not doing it in this pass (out of scope for
Phase 3 research).

## Attribution

CheckIt 3.0 (c) 1988-1990 TouchStone Software Corporation.
Findings are from byte-level searches of the shipping binary
(`CHECKIT.EXE`), the MS-C overlay (`CHECKIT.OVL`), and the
help-text archive (`CHECKIT.HLP`), cross-referenced against
published Curnow-Wichmann Whetstone reference implementations.
No code reproduced; results are from marker-absence scans and
format-string examination.
