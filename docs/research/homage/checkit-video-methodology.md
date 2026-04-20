# CheckIt, video benchmark methodology

**Tool:** CheckIt 3.0, Jan 1991 (`Homage/Checkit/CHECKIT.EXE`)
**Author:** TouchStone Software Corporation, Copyright 1988-1990
**CERBERUS diff target:** `bench.video.*` (issue #6)

## What we were looking for

Phase 3 T5 asked what CheckIt's video benchmark actually
measures, and whether it provides a reference number that
could serve as a second-opinion data point for CERBERUS's
`bench.video.mode13h_kbps` result (4,613 KB/s on the BEK-V409
Trio64 VLB system, per v0.4.0 validation INI). Issue #6
observes that the number is ISA-range on VLB-class hardware
and asks whether that is a real measurement, a methodology
defect in CERBERUS, or a hardware-path issue.

## What we found

**CheckIt measures only text-mode video throughput. No mode
13h benchmark exists in CheckIt at all.** The tool reports two
numbers:

- **BIOS Video Speed.** Characters per second writing text
  through INT 10h BIOS teletype calls. Represents the old-DOS
  slow path that word-processing and terminal emulators used
  before direct VRAM became standard.
- **Direct Video Speed.** Characters per second writing text
  directly to VRAM. Represents the fast path that shells,
  spreadsheets, and early Windows used.

Both measurements run **in text mode only** (80x25 or similar;
exact mode inferred from the "characters/second" output unit).
The help text at `HELP:bench_video` is explicit:

> Although the measurement is taken in text mode, the relative
> speed also applies to graphics.

So CheckIt's authors concede that their text-mode measurement
is a proxy for graphics-mode throughput, not a direct graphics
benchmark. No mode 13h, no mode 12h, no SVGA mode.

## How the numbers are stored

Two keys in `CHECKIT.CNF` (the saved-for-compare config file
format identified in Phase 2 T1):

- `bm_bvideo` stores the BIOS Video CPS value for the saved
  reference machine.
- `bm_dvideo` stores the Direct Video CPS value.

Output format pulled from the binary:

- `BIOS VIDEO CPS` label
- `DIRECT VIDEO CPS` label
- `%6.0f Characters/Second` data row
- `Rating: %5.2f times %s` against the named reference
  (default: the hardcoded IBM PC-XT constants discussed in
  `checkit-pcxt-baseline.md`).

## Consequence for issue #6

**There is no CheckIt reference number for CERBERUS's mode 13h
benchmark.** CheckIt never measured mode 13h. The 4,613 KB/s
CERBERUS reports on the BEK-V409 Trio64 VLB system cannot be
validated or invalidated by a CheckIt run on the same machine
because CheckIt would produce only text-mode numbers.

The text-mode side IS comparable. CERBERUS's
`bench.video.text_write_kbps = 4,668` maps to a CheckIt Direct
Video CPS number: each text cell is 2 bytes (character plus
attribute), so 4,668 KB/s × 1024 bytes/KB / 2 bytes/cell ≈
**2,391,000 characters per second**. CheckIt on the same
machine, same mode, should produce a Direct Video CPS result
in the same neighborhood. A TouchStone-validated capture on
the BEK-V409 would anchor the text-mode measurement; it
would tell us nothing about the mode 13h number.

**For issue #6 the implication is that a second-opinion
benchmark must come from tools that specifically measure mode
13h bandwidth.** PCPBENCH (Phase 3 T14) and 3DBENCH (Phase 3
T15) are candidate sources. The REPSTOSD.EXE reference binary
at `tools/repstosd/REPSTOSD.EXE` built for issue #6 test-2 is
the methodology-free hardware ceiling; compare its output
against CERBERUS's mode 13h number and against PCPBENCH /
3DBENCH mode 13h results.

## Why two video speeds, not one

The dual BIOS vs Direct measurement is interesting as a
methodological choice. BIOS teletype goes through int 10h
AH=0Eh which invokes the VGA BIOS's character-rendering path:
fetch glyph bitmap from a ROM font table, render into VRAM,
advance cursor, handle scroll at screen bottom. This is a
very different workload from a direct store to `B800:offset`.
The ratio of Direct Video CPS to BIOS Video CPS is a rough
proxy for how bloated the system's video BIOS is.

CERBERUS does NOT measure the BIOS teletype path today.
`bench_video.text_write_kbps` is direct-only. Adding a BIOS
teletype variant would be a low-effort complement that allows
a CheckIt-style rating of BIOS-call overhead on each system,
and it would explicitly surface the slow-BIOS case that
Manifest and QAPlus report separately. Not a v0.4 deliverable;
filed for v0.5+.

## What CERBERUS currently does

`bench_video.c` measures two paths today:

- `bench.video.text_write_kbps`: direct VRAM write to the
  detected text segment (B800 on color adapters, B000 on mono).
  Maps approximately to CheckIt's Direct Video CPS after
  unit conversion.
- `bench.video.mode13h_kbps`: direct VRAM write to A000:0000
  in VGA mode 13h. No CheckIt analog.

Both are direct-write paths. The measurement methodology is
sound for the text-mode case; the mode 13h case is a CERBERUS
original measurement that must be validated against mode
13h-aware reference benchmarks rather than against CheckIt.

## Recommended action for issue #6

**Do not treat CheckIt as a reference for mode 13h.** The
issue's investigation plan should cite PCPBENCH and 3DBENCH as
the relevant second-opinion sources, plus REPSTOSD.EXE as the
methodology-free hardware-ceiling probe. CheckIt belongs in
issue #6's text-mode calibration path, not its mode 13h path.

**Consider adding a BIOS-teletype variant to `bench_video` for
v0.5+.** CheckIt-style dual measurement gives CERBERUS a
CheckIt-rating-compatible text throughput pair and surfaces
slow-BIOS systems explicitly. Three new INI keys:
`bench.video.bios_cps`, `bench.video.direct_cps`, and an
inferred `bench.video.bios_overhead_ratio`. Low cost; not
scope for this phase.

## Attribution

CheckIt 3.0 (c) 1988-1990 TouchStone Software Corporation.
Findings are from byte-level scans of `CHECKIT.EXE` plus the
help-text archive `CHECKIT.HLP`, using the strings already
extracted during Phase 1 discovery. Help text quoted
verbatim for methodology framing. No code reproduced.
