# PCPBENCH, 3D rendering reference

**Tool:** PCPBENCH v1.0 (PC Player 3D Benchmark)
**Author:** PC Player magazine, ca. 1995-1996 (German PC gaming
magazine by Computec Media; specific author not named in the
binary)
**Source binary:** `Homage/PCPBENCH/PCPBENCH.EXE` (DOS/4GW
protected-mode, 111,814 bytes) plus `PCPBENCH.DAT` (411,402
bytes; 3D scene and texture data)
**CERBERUS diff target:** `bench.video.mode13h_kbps` (issue #6)

## Attribution correction from Phase 2 planning notes

Phase 2 triage had this tool labeled as Jim "Trixter" Leonard's
work. That was incorrect. Strings inside the binary and the
accompanying data file cite "PC Player 3D Benchmark V1.0" and
the interactive text is German, consistent with a PC Player
magazine cover-disk release. No Trixter / Oldskool PC markers
found in the binary; the attribution was a planning-note
confusion and is corrected here.

## What we were looking for

T14 asked whether PCPBENCH's inner loops and measurement
methodology inform the issue #6 investigation (why
`bench.video.mode13h_kbps` lands in the ISA range on the
BEK-V409 Trio64 VLB system). Goals:

- Identify what PCPBENCH measures.
- Confirm or rule out that its numbers can serve as a second-
  opinion data point for CERBERUS's mode 13h bandwidth.

## What we found

**PCPBENCH is a full 3D scene renderer, not a raw VRAM
throughput benchmark.** The tool renders a pre-built 3D scene
(411 KB of scene and texture data in `PCPBENCH.DAT`) to the
framebuffer across a sequence of frames, times the run, and
reports **frames per second** in the chosen graphics mode.

Supported modes (from the usage help strings in the binary):

- VGA mode 0x13 (320x200 8bpp) via `/VGAMODE`
- Arbitrary VESA modes via numeric argument (e.g. `PCPBENCH 101`
  for mode 101h = 640x480 8bpp)
- Linear framebuffer via VESA 2.0 if available, toggleable via
  `/NOLINEAR`
- 400-line variant via `/400LINES`

Output format is literally `Erreichte Framerate im VGA Modus
(%dx%d %dbpp): %.1f fps` (German: "Achieved framerate in VGA
mode...: N.N fps").

## Inner-loop technique

PCPBENCH uses the fastest possible 32-bit protected-mode VRAM
primitives:

- **16 instances of `REP STOSD` (F3 AB in 32-bit flat mode).**
  Pure 32-bit dword store, no operand-prefix overhead. The
  hot paths for clearing framebuffer regions and filling solid
  polygons.
- **11 instances of `REP MOVSD` (F3 A5 in 32-bit flat mode).**
  Used for texture copies and pre-rendered bitmap blits.

The tool is DOS/4GW-hosted, running in 32-bit flat protected
mode, so these instructions execute at the CPU's native
32-bit bus width with no mode-switch or prefix overhead. This
is the theoretical upper bound for DOS-era VRAM throughput.

**Implication:** if PCPBENCH on the BEK-V409 system shows a
frame rate characteristic of a healthy VLB system (community
references for 486 DX-2-66 + Trio64 VLB typically land in the
8 to 14 fps range depending on mode), the hardware path is
fine. If it drops to ISA-class fps (2 to 5 fps on the same
hardware if something is wrong with the VLB slot), we have
corroboration that the ISA-range number CERBERUS reports
reflects a real bus-path problem rather than a CERBERUS
measurement artifact.

## Consequence for issue #6

**PCPBENCH is a useful second-opinion data point, but it does
NOT produce a directly-comparable MB/s number.** CERBERUS
reports `bench.video.mode13h_kbps` in KB/s (raw bandwidth);
PCPBENCH reports fps (integrated CPU + FPU + VRAM + 3D math
workload). The derivation from fps to equivalent bandwidth
requires knowing how many bytes of framebuffer PCPBENCH
touches per frame, which depends on scene complexity and
would need a separate analysis pass.

What PCPBENCH CAN tell us:

1. Whether the BEK-V409 system's VLB path is healthy (fps
   in known community ranges for the hardware).
2. Whether CERBERUS's raw-bandwidth measurement is grossly
   out of scale with a real-game-shaped workload (if CERBERUS
   reports 5 MB/s but PCPBENCH sustains the fps that would
   demand 15 MB/s effective bandwidth, CERBERUS's methodology
   is the story, not the hardware).

**Recommended use:** run PCPBENCH on the BEK-V409 in both VGA
mode 13h (`/VGAMODE`) and a comparable VESA mode. Cross-check
the fps against community reference tables (e.g. the PC Player
magazine readership baseline numbers, still hosted on
pcplayer.de archives and community forums). That data point
augments the REPSTOSD.EXE test-2 already built for issue #6
and the Phase 3 T15 3DBENCH analysis.

## Attribution

PCPBENCH (c) PC Player / Computec Media, ca. 1995-1996.
German magazine cover disk release, freely distributed.
Findings are from byte-level scans of the DOS/4GW-hosted
PE-ish binary (no MZ stub unpacking needed; DOS/4GW wraps
LE/LX format automatically). The 16 REP STOSD and 11 REP
MOVSD occurrences were counted via opcode-pattern search
across the full binary. No code reproduced; references
point at the 1-byte F3-prefix opcode patterns only.
