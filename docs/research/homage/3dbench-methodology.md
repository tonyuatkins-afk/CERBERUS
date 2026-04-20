# 3DBENCH, phased-timing 3D rendering benchmark

**Tool:** 3DBENCH v1 and v2 (Superscape 3D Benchmark)
**Author:** Superscape VRT Ltd, UK (Superscape VR platform team;
specific author not named in the binary)
**Source binaries:**
- `Homage/3DBENCH/3DBENCH.EXE` (16-bit real-mode MZ, 54,036 bytes)
- `Homage/3DBENCH2/3DBENCH2.EXE` (larger variant, 285,248 bytes)
Plus shared scene data in `.PC1`, `.PC2`, `.WLD`, `.SHP`, `.HED`,
and `.PAL` asset files.
**CERBERUS diff target:** `bench.video.mode13h_kbps` (issue #6)

## Attribution correction

Phase 2 triage notes speculated this was Future Crew / Paralax.
Incorrect. Both binaries carry the `SUPERSCAPE` attribution
prominently in their data-section strings. Superscape VRT Ltd
was a UK virtual-reality and 3D-engine vendor active through
the 1990s; their in-house 3D rendering kernel was widely
benchmarked via these two binaries. Noted here as a correction
for the record, same shape as the FASTVID correction in
Phase 2 T11.

## What we were looking for

T15 asked whether 3DBENCH's measurement methodology or its
on-screen output provide a second-opinion view for CERBERUS's
`bench.video.mode13h_kbps` number. Goals:

- Identify what the tool measures and how it reports results.
- Evaluate whether a 3DBENCH run on BEK-V409 could corroborate
  or contradict CERBERUS's 4.6 MB/s mode 13h result.

## What we found

**3DBENCH reports a per-frame phase breakdown, not just a
single fps.** The header row in both v1 and v2 is literally:

```
Mov  Prc  Srt  Clr  Drw  Cpy   Tot   Fps
```

Six distinct per-frame work phases plus a total and a derived
frame rate:

- **Mov** (move): object transformations; pure CPU work, matrix
  math per scene object.
- **Prc** (process): typically lighting and clipping; CPU-bound
  floating-point or fixed-point math.
- **Srt** (sort): polygon depth / Z-sort; pure CPU, heavy on
  memory accesses and branch prediction.
- **Clr** (clear): framebuffer clear; raw VRAM write. The
  direct analog to CERBERUS's `bench.video.mode13h_kbps`
  measurement.
- **Drw** (draw): polygon rasterization; mixed CPU fixed-point
  interpolation and VRAM writes.
- **Cpy** (copy): blit from double-buffer to screen. Second
  direct VRAM-write analog, plus a VRAM-read component if
  the double buffer is also in video memory.
- **Tot** (total): sum of the six phases.
- **Fps** (frames per second): derived rate.

**This phase decomposition is exactly what issue #6 needs.**
If BEK-V409's Clr time per frame corresponds to ~5 MB/s (64000
bytes cleared in ~12.5 ms), CERBERUS's mode 13h bandwidth
number is corroborated and the hardware path really is
delivering ISA-range throughput. If Clr time corresponds to
~25 MB/s (~2.5 ms per frame clear), CERBERUS's number is a
methodology artifact and the work lives in bench_video.

## Inner-loop primitives

- **3DBENCH v1** (16-bit real-mode MZ): uses 4x `REP STOSW`, 27x
  `REP MOVSW`, plus byte variants. Word-granularity primitives
  throughout. No 32-bit instructions (386+ opcodes absent).
- **3DBENCH2** (same SUPERSCAPE string, larger binary; likely
  386+ optimized): uses 4x `REP STOSW/STOSD` pattern and 27x
  `REP MOVSW/MOVSD` pattern. Without disassembly I cannot
  distinguish the operand-size override cases, but the
  occurrence counts are identical to v1 which suggests the same
  algorithmic structure with potentially different operand
  widths.

The dominance of `MOVS` over `STOS` (27-to-4 in both versions)
indicates a double-buffered renderer where most VRAM traffic is
the final buffer-to-screen blit rather than pixel fills.

## Consequence for issue #6

**3DBENCH is arguably the best second-opinion tool we own for
issue #6,** because it is the only one that reports VRAM-clear
time as a separately-broken-out column. The plan:

1. Run `3DBENCH.EXE` on BEK-V409 in its default VGA mode 13h.
2. Record the Clr and Cpy numbers (per-frame milliseconds).
3. Convert Clr to equivalent KB/s:
   `kbps = (64000 bytes / 1024) * (1000 ms / clr_ms)`.
4. Compare against `bench.video.mode13h_kbps` from CERBERUS.

If the 3DBENCH Clr-derived KB/s is within 20% of CERBERUS's
number, CERBERUS's measurement is corroborated and the work on
issue #6 becomes "why is this VLB system behaving ISA-range
(BIOS shadow? missing VLB jumper? chipset config?)."

If 3DBENCH Clr-derived KB/s is dramatically higher than
CERBERUS's, the C-loop-vs-asm difference is doing more work than
the CERB-VOX diagnostic suggested, and bench_video needs a
methodological overhaul.

## Recommended action

**Ship 3DBENCH alongside REPSTOSD.EXE as part of the issue #6
test suite.** Together they give three independent data points
for the mode 13h bandwidth question on the BEK-V409:

1. CERBERUS bench_video mode13h_kbps (C-loop, CFLAGS_NOOPT)
2. REPSTOSD.EXE (hand-tuned asm REP STOSW ceiling)
3. 3DBENCH Clr column (Superscape's VRAM-clear reference)

Three-way agreement closes issue #6 as "working as designed on
this hardware." Two-way disagreement tells us which side
(methodology vs hardware) needs further work.

Not in scope for this Phase 3 pass: the 3DBENCH run is a Tony-
driven real-iron action. Filed as the natural next step for
the issue #6 investigation.

## Attribution

3DBENCH v1 and v2 (c) Superscape VRT Ltd, UK, ca. 1994-1996.
Freeware benchmark distribution. Findings are from byte-level
scans of both shipping binaries without any unpacking step
(both are plain MZ with no observable packer). No code
reproduced; findings cite the `SUPERSCAPE` attribution string
at file offsets 0x32B2 (v1) and 0xB664 (v2), plus the per-frame
header string at 0x340D (v1) and 0xB942 (v2). Inner-loop
primitive counts via opcode-pattern search for REP-prefix
instructions.
