# CheckIt — "Dhrystone" identification

**Tool:** CheckIt 3.0, Jan 1991 (`Homage/Checkit/CHECKIT.EXE`)
**Author:** TouchStone Software Corporation, Copyright 1988-1990
**CERBERUS diff target:** `bench.cpu.dhrystones` (issue #4)

## What we were looking for

Phase 2 T2 asked whether CheckIt runs **Dhrystone 1.1** or
**Dhrystone 2.1**, because CERBERUS ships a Dhrystone 2.1 port
(`bench_dhrystone.c`) and bench-calibration against CheckIt's
number on matched hardware only makes sense if the two tools are
measuring the same thing.

## What we found

**Neither.** CheckIt's binary contains **none of the
characteristic strings or symbol markers of any genuine Dhrystone
port.** A byte-level search across both `CHECKIT.EXE` and
`CHECKIT.OVL` for the following canonical tokens returns zero
hits:

- `DHRYSTONE PROGRAM` (the `Ptr_Glob` initialization string,
  present in every reference Dhrystone implementation since 1984)
- `SOME STRING`, `1'ST STRING`, `2'ND STRING`, `3'RD STRING`
  (Str_1_Loc / Str_2_Loc / Str_3_Loc literal initializers)
- `Proc_`, `Func_`, `Ident_`, `Enum_`, `Int_Glob`, `Ptr_Glob`,
  `Discr`, `Rec_` — Dhrystone's public variable names, normally
  present at least as data-section symbols or format strings
- `Weicker`, `Reinhold`, `Version 1.1`, `Version 2.1`,
  `Number_Of_Runs`, `Microseconds for one run`

The only Dhrystone-related string in CheckIt is the **output
format** `%5.0f Dhrystones`. That is a label, not a marker of
the underlying algorithm. CheckIt reports a number it calls
"Dhrystones" but the measurement itself is a custom synthetic,
not a Dhrystone 1.1 or 2.1 port.

This is consistent with 1990-era benchmark practice:
"Dhrystones" became a generic term for "integer-throughput
score" independent of whether the Weicker procedure was actually
used. Vendors and benchmark-tool authors sometimes wrote their
own integer-mix loops, normalized the output to look
Dhrystone-shaped, and kept the familiar label.

## What CERBERUS currently does

`bench_dhrystone` is a faithful Dhrystone 2.1 port — every
`Proc_*` / `Func_*` / `Ident_*` / `Str_*` construct is present,
`Number_Of_Runs` is the outer loop counter, the global storage
pattern matches Weicker's reference. The source header at
`bench_dhrystone.c:1-8` explicitly cites ACM SIGPLAN Notices
August 1988 (the 2.1 announcement) as lineage.

## Gap analysis — consequence for issue #4

The empirical calibration "CheckIt on BEK-V409 = 33,609
Dhrystones; CERBERUS ±5%" at `bench_dhrystone.c:30` remains
valid **as a matched-hardware agreement point**, not as an
algorithmic equivalence.

Two distinct benchmarks reporting numbers in the same format —
"N Dhrystones/second" — that happen to land close together on a
given 486 DX-2 do not mean they are measuring the same thing.
They agree on this hardware because CheckIt's custom synthetic
and Dhrystone 2.1 both produce per-second rates in the same
order of magnitude when the CPU is running at ~66 MHz. On
meaningfully different hardware (8088, Pentium II, Cyrix 6x86,
anything with unusual branch-predictor / cache behavior), the
two numbers can drift apart and neither is "wrong" — they are
just measuring different work mixes.

**No conversion factor applies.** A conversion factor between
Dhrystone 1.1 and 2.1 would be mechanical (identifiable string
compiler-optimization differences produce a known ratio).
CheckIt's synthetic has no such relationship to either Dhrystone
version — there's nothing to convert.

## Recommended action

**Keep CERBERUS's Dhrystone 2.1 port unchanged.** Issue #4's
CheckIt-comparable calibration should be framed as:

> CERBERUS targets agreement with CheckIt's reported number on
> matched 486-class hardware to within ±5%. This is an empirical
> anchor, not an algorithmic equivalence. On hardware outside
> the 486 family, CERBERUS's Dhrystone 2.1 number is what it is
> — a faithful Weicker 2.1 result — and drift from CheckIt's
> synthetic on the same machine is expected and not a defect.

**Update the `bench_dhrystone.c:30` comment** to add this
framing, so a future reader doesn't mistake the 33,609 target
for a claim of algorithm equivalence. Small edit; not doing it
in this pass (out of scope for Phase 2 which is research-only).

## Attribution

CheckIt 3.0 (c) 1988-1990 TouchStone Software Corporation.
Findings above are from byte-level searches of the shipping
binary + overlay file, conducted with a custom Python scanner
against the full file (not just the MZ image). No code
reproduced.
