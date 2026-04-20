# CHRISB, Chris's 3D Benchmark

**Tool:** Chris's 3d Benchmark (BENCH.EXE) and Chris's 3d
SVGA Benchmark (SVGABNCH.EXE)
**Author:** Chris (specific surname not embedded in the binary)
**Source binaries:**
- `Homage/CHRISB/BENCH.EXE` (DJGPP-hosted, 92,191 bytes)
- `Homage/CHRISB/SVGABNCH.EXE` (DJGPP-hosted, 156,987 bytes)
Shared input scene `HTOR.3DR` in the same directory.
**CERBERUS diff target:** `bench.video.mode13h_kbps` (issue
#6), third independent 3D reference alongside PCPBENCH and
3DBENCH

## What we were looking for

T17 asked whether CHRISB provides an additional independent
reference for issue #6. I triaged it alongside LM60,
MTRRLFBE, and SYSINFO; the two CHRISB binaries covered
VGA-only and SVGA-capable 3D rendering respectively, which
makes them a natural third data point for the mode 13h
bandwidth question (on top of PCPBENCH and 3DBENCH).

## What we found

**Attribution:** self-identifying strings "This is Chris's 3d
Benchmark" and "This is Chris's 3d SVGA Benchmark" at the top
of each binary's data section. Author's surname is not
embedded. Both binaries are DJGPP-hosted (DJ Delorie's DOS
GCC port):

- Build date captured in-binary: `DJGPP libc built Feb 4 1996
  20:49:03 by gcc 2.7.2`
- STUB loader attribution: `The STUB.EXE stub loader is
  Copyright (C) 1993-1995 DJ Delorie.`

**Output format:**

```
Your computer received a Chris's Bench Score of %d.%d
```

Unitless composite score. Higher is better. No fps or KB/s
breakdown.

**Input scene:** shared 3D model file `htor.3dr` (likely a
Simulink / 3D Studio-era exchange format) used by both
variants.

**BENCH.EXE (VGA variant)** targets mode 13h (320x200x8). No
SVGA path. Source filename embedded: `bench.c`.

**SVGABNCH.EXE (SVGA variant)** targets VBE 2.0 with a linear
framebuffer, with specific S3-chipset detection. Error
strings visible in the binary:

- `Mode 13h`
- `Not a VGA mode`
- `VBE 2.0 protected mode interface not available`
- `S3 set gfx mode failed`

The "S3 set gfx mode failed" error path is noteworthy for
issue #6: CHRISB's SVGABNCH has explicit S3-chipset code.
The BEK-V409 bench box has a Trio64 (S3). If SVGABNCH runs
successfully on BEK-V409 and produces a bench score, that
score is a second-opinion number from a DJGPP-built 32-bit
protected-mode 3D renderer using S3-specific paths.

## Consequence for issue #6

**CHRISB is a useful third 3D reference,** complementing
PCPBENCH (Phase 3 T14) and 3DBENCH (Phase 3 T15). Three
independent 3D benchmarks on BEK-V409 would give a robust
corroboration window:

| Benchmark | Report | Mode | Relevance |
|---|---|---|---|
| REPSTOSD.EXE (own) | KB/s | mode 13h | methodology-free hardware ceiling |
| CERBERUS bench_video | KB/s | mode 13h | current ~4.6 MB/s result |
| PCPBENCH | fps | mode 13h + VESA | integrated 3D fps |
| 3DBENCH v1/v2 | per-phase ms | mode 13h | VRAM-clear phase broken out |
| CHRISB BENCH | score | mode 13h | DJGPP 32-bit-mode reference |
| CHRISB SVGABNCH | score | VBE 2.0 LFB | S3-specific path |

If all of these produce numbers that imply similar underlying
bandwidth, the hardware path is fine and CERBERUS's
measurement stands. If CHRISB's SVGA variant through S3 LFB
shows markedly better throughput, the VLB path has capability
CERBERUS's current methodology is not reaching.

## Author identity footnote

"Chris" without surname is thin. The binaries date to 1996
(DJGPP libc Feb 4 1996 timestamp), and CHRISB's 3D benchmarks
were shared on early shareware archives (Garbo, SimTel, Hobbes)
in that era. If further attribution work is wanted, the
`HTOR.3DR` scene filename (HTOR = "HTORUS" or similar?) might
cross-reference to a published demoscene production; not
pursued here because the benchmark's utility to issue #6 does
not require knowing Chris's last name.

## Recommended action

**Run both CHRISB binaries on BEK-V409 as part of the issue
#6 cross-corroboration run.** Document the Bench Scores
alongside the other five numbers in the above table. Not a
CERBERUS code change; a deployment-time data collection.
Filed as part of the issue #6 next-steps.

## Attribution

Chris's 3d Benchmark and Chris's 3d SVGA Benchmark, author
"Chris" (surname not embedded), ca. 1996. DJGPP-hosted DOS
protected-mode binaries using DJ Delorie's STUB.EXE loader.
Findings from byte-level scans of both shipping binaries
without any unpacking step (DJGPP binaries carry their
protected-mode COFF content in a DOS-MZ wrapper plus the
DJGPP-standard STUB.EXE real-mode loader). No code reproduced;
findings are limited to the visible self-identification
strings, build metadata, error-path strings (the VBE / S3
error messages), and the output format string.
