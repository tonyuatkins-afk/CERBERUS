# Homage research — DOS-era reference tools

This directory contains lesson-extraction notes from DOS-era
reference tools studied during CERBERUS v0.4 development. The
purpose is **homage**, not derivation: we read these tools to
understand how the field handled problems that CERBERUS also
faces, and we let the lessons inform CERBERUS design without
copying code structure or algorithms verbatim.

## Ethical framing

- **No code reproduction.** No decompiled function bodies,
  disassembly listings, or verbatim algorithm transliterations
  appear in these docs. Descriptions are abstracted to prose —
  the goal is that a future reader understands the *approach*,
  not re-implements the tool.
- **No binaries redistributed.** Source binaries live in
  `C:\Development\Homage\<TOOL>\` on the author's development
  machine. This directory contains only lessons-derived
  narrative.
- **Attribution preserved.** Each doc credits the original
  tool's author, version, and publication status.
- **Corrections flagged, not silenced.** Where the Phase 1 brief
  misidentified a tool, the correction is documented openly
  (see `fastvid-mtrr-reference.md` for the FASTVID case).

## Audit trail

- Phase 1 discovery (inventory + string extraction + decompile
  priority) is summarized in
  `C:\Development\Homage\_research\phase1-report.md` (not
  tracked by CERBERUS's git — it's project-external research
  material).
- Phase 2 authorization was scoped as
  **minimum-plus-one** by the project owner: T1, T2, T4, T6, T7,
  later expanded with T11, T12, T13.
- Phase 2 artifacts (Ghidra project, string dumps, unpacked
  binaries, custom analysis scripts) stay in
  `C:\Development\Homage\_research\`, outside this repo.
- This directory contains one finished lesson doc per authorized
  task; each was committed separately to keep diff review clean.

## Index

### CheckIt 3.0 (TouchStone Software Corp, 1988-1990)

- [`checkit-pcxt-baseline.md`](checkit-pcxt-baseline.md) — **T1.**
  The "IBM PC-XT" default baseline is a hardcoded reference to a
  4.77 MHz PC/XT's Dhrystone + Whetstone + video numbers. Exact
  constants live behind the MS-C overlay loader and are not
  cleanly recoverable via static analysis alone; CERBERUS's
  absolute-Dhrystones approach sidesteps the dependency.
- [`checkit-dhrystone-version.md`](checkit-dhrystone-version.md)
  — **T2.** CheckIt's "Dhrystones" is **not a Dhrystone port at
  all**. Zero characteristic Weicker 1.1 or 2.1 strings or
  symbols present. The number is a custom synthetic using the
  familiar label. CERBERUS's Dhrystone 2.1 port stays unchanged;
  the ±5% BEK-V409 calibration is a matched-hardware agreement
  point, not an algorithmic equivalence. Conversion factor
  would be meaningless and is not proposed.
- [`checkit-fpu-detection.md`](checkit-fpu-detection.md) —
  **T4.** CheckIt uses a three-step FPU probe: presence via
  FNINIT + FNSTCW high-byte check, functional sanity via
  FLDZ/FLDZ/FCOMPP + FNSTSW, and vendor discrimination via
  IIT-reserved undefined-opcode probes. CERBERUS's simpler
  FNINIT + FNSTSW sentinel is correct for its scope; the IIT
  discrimination gap is the only genuine capability gap, filed
  for v0.5+ if Cyrix/IIT hardware enters the capture corpus.

### CACHECHK v4 (Ray Van Tassle, 1995-1996)

- [`cachechk-cache-thresholds.md`](cachechk-cache-thresholds.md)
  — **T6 + T7.** PKLITE unpacked via UNP 4.11 (first attempt
  via custom Python decompressor abandoned at >>15 min from
  working per owner's guidance). CACHECHK sweeps 12 block sizes
  and counts plateaus to infer cache topology; CERBERUS
  `diag_cache` uses a two-point ratio to answer a different
  question (health, not topology). CERBERUS's 1.30× per-line
  PASS threshold is well inside the envelope CACHECHK's
  real-hardware measurements establish — no recalibration
  needed. Plateau-sweep mode is a deferred v0.5+ idea.

### FASTVID (John Hinkley, 1996)

- [`fastvid-mtrr-reference.md`](fastvid-mtrr-reference.md) —
  **T11.** Phase 2 brief misidentified this tool as Bill
  Yerazunis's VLB TSR. FASTVID is actually a Pentium Pro 82450
  chipset MTRR / write-posting configurator — entirely
  different hardware generation and mechanism. Negative-space
  finding: issue #6 (slow VLB bandwidth) requires a different
  reference tool not currently in the Homage corpus.

### CHKCPU 1.27.1 (Jan Steunebrink, 1997-2022)

- [`chkcpu-lessons.md`](chkcpu-lessons.md) — **T12.**
  Documentation-only read; no decompile needed. CHKCPU covers
  the full x86 lineage including Cyrix DIR-based pre-CPUID
  discrimination that CERBERUS currently lacks. Four deferred
  candidates filed for v0.5+ (Cyrix DIR, ISA extension flags,
  cache WB/WT mode, multiplier decomposition) — none urgent.
  `CHKCPU.TXT` is thorough enough to reimplement any of these
  from scratch without touching CHKCPU's binary.

### TOPBENCH (Jim "Trixter" Leonard, community-contributed)

- [`topbench-database-model.md`](topbench-database-model.md) —
  **T13.** Documentation-only read. TOPBENCH's
  `DATABASE.INI` is a human-readable INI of user-submitted
  machine fingerprints keyed on BIOS CRC16. Near-neighbor
  records for CERBERUS issue #6 (486 DX2-66 + Cirrus VLB; Am486
  DX4 + S3 Trio 64 likely-PCI) bracket the BEK-V409 target
  without an exact match. Community contribution is possible
  but requires a CERBERUS-keys → TOPBENCH-keys mapping; deferred.

## Phase 3, 2026-04-19 evening

Seven additional lesson docs shipped under the same ethical
frame as Phase 2: no decompiled code reproduced, no binaries
redistributed, attribution preserved, corrections flagged
openly. Ordered by CERBERUS-issue relevance:

### Deferred tasks from Phase 2 now closed

- [`checkit-whetstone-version.md`](checkit-whetstone-version.md)
  . **T3.** CheckIt's "Whetstones" is NOT a Curnow-Wichmann
  port. Zero canonical markers (Module 1..11, Curnow, Wichmann,
  PA, P0, Wichmann constants, MFLOPS). Custom FPU synthetic
  using the familiar label, mirroring the T2 Dhrystone finding.
  Direct consequence for issue #4: the 100x gap between
  CERBERUS's ~109 K-Whet and CheckIt's 11,420 is a label
  collision, not a port defect. Reframe issue #4 around
  published Curnow Whetstone reference numbers
  (1,500-3,000 K-Whet on 486 DX-2-66) instead of CheckIt.
- [`checkit-video-methodology.md`](checkit-video-methodology.md)
  . **T5.** CheckIt measures ONLY text-mode video throughput
  (BIOS Video CPS and Direct Video CPS), never mode 13h. No
  CheckIt reference exists for CERBERUS's
  `bench.video.mode13h_kbps`. Reference sources for issue #6's
  mode 13h bandwidth question must come from tools that
  actually measure mode 13h: PCPBENCH, 3DBENCH, CHRISB,
  REPSTOSD.
- [`cachechk-umc-timer.md`](cachechk-umc-timer.md) . **T8.**
  CACHECHK uses the same 8254 PIT C2 / C0 cross-check CERBERUS
  uses in `timing_compute_dual`. Workaround is structural
  match, not a technique delta. Useful addition from the
  finding: emit raw forensic values on failure (the "Timer
  messed up! %08lx %08lx %08lx" pattern) instead of just a
  "measurement_failed" status string. Filed as v0.5+ Rule 4a
  enhancement.
- [`speedsys-methodology.md`](speedsys-methodology.md) . **T9
  + T10.** SPEEDSYS is Vladimir Afanasiev's Russian-origin
  benchmark, calibrated to Pentium MMX 200 + ASUS TXP4 rev1.02
  as its memory-speed-index anchor (later abandoned in v4.76
  for absolute Peak Bandwidth). Feature matrix overlaps
  CERBERUS only at pre-CPUID CPU detection; CHKCPU (Phase 2
  T12) already covers that range better. No CERBERUS action.
  Attribution correction: Phase 1 notes had Roedy Green;
  actual author is Vladimir Afanasiev.

### New tasks for issue-#6 second-opinion data

- [`pcpbench-methodology.md`](pcpbench-methodology.md) .
  **T14.** PCPBENCH by PC Player magazine (Computec Media,
  Germany, 1995-1996), NOT Jim "Trixter" Leonard as Phase 2
  planning notes assumed. DOS/4GW 32-bit 3D scene renderer
  using 16x REP STOSD + 11x REP MOVSD, the theoretical upper
  bound for DOS-era VRAM throughput. Reports frame rate (fps),
  not raw bandwidth. Useful as a "does the VLB path work at
  all" signal if a run on BEK-V409 lands in community-reference
  fps ranges for 486 DX-2-66 + Trio64 VLB.
- [`3dbench-methodology.md`](3dbench-methodology.md) . **T15.**
  3DBENCH v1 and v2 by Superscape VRT Ltd (UK), NOT Future
  Crew / Paralax as Phase 2 planning notes assumed. Key
  finding: 3DBENCH reports a **per-frame phase breakdown**
  (`Mov Prc Srt Clr Drw Cpy Tot Fps`), with Clr as a
  dedicated column for VRAM-clear time. This is the most
  directly-comparable reference for CERBERUS's
  `bench.video.mode13h_kbps`: convert 3DBENCH's Clr ms/frame
  into KB/s via (64000 / 1024) * (1000 / clr_ms) and
  compare.
- [`chrisb-3d-benchmark.md`](chrisb-3d-benchmark.md) . **T16.**
  "Chris's 3d Benchmark" (VGA) and "Chris's 3d SVGA Benchmark"
  (VBE 2.0 + S3-specific paths). Author first name only
  ("Chris", surname not embedded), DJGPP-built 1996. Reports a
  unitless "Bench Score." SVGA variant's S3-specific code path
  is particularly interesting for the BEK-V409 Trio64
  (S3 chipset) VLB system.
- [`lm60-landmark-speed.md`](lm60-landmark-speed.md) . **T17.**
  Landmark System Speed Test 6.0 (Landmark Research
  International Corp, 1993). Tightly packed binary; deeper
  methodology details not accessible without disassembly. The
  externally-documented "Landmark Speed" rating uses the same
  IBM PC/XT anchor CheckIt does (Phase 2 T1 finding). The PC/XT
  anchor was era convention, not a tool-specific decision.
  CERBERUS's absolute-numbers approach sidesteps the need.

## Phase 3 addendum, 2026-04-20 evening

### QA-Plus v3.12 (Diagsoft, Inc., 1987-1989)

- [`qa-plus-lessons.md`](qa-plus-lessons.md) — Commercial
  service-technician diagnostic studied as the last significant
  DOS-era competitor in the detect / diag / bench space. No
  unpacking needed (all four binaries are raw MZ / COM).
  Version confirmed as 3.12 from binary strings; the shipped
  READ.ME only documents changes through 3.11, and the 3.12
  binaries dated 1989-07-11 appear to be an undocumented
  point release. Attribution correction: QAPLUS1.COM credits
  "code licensed from Chesapeake Data Systems, Inc.", so not
  all algorithms in that binary are DiagSoft originals.
  Architectural finding: QA-Plus ships as **four cooperating
  binaries** (QAPLUS.EXE control panel, QAPLUS1.COM destructive
  diagnostic, QARAM.EXE chip-level RAM map, HDPREP1.COM
  low-level format) because the RAM test must relocate DOS
  and the diagnostic itself out of low 128 KB; MZ EXEs can't
  do that, so the destructive diag is a relocatable COM.
  Consistency cross-checks: embedded ad-hoc in each test, not
  abstracted — CERBERUS's dedicated `consist.c` rule engine
  is cleaner, no new rule to port. Diagnostic-methodology
  gaps worth a v0.5+ pass: checkerboard / inv-checkerboard
  memory patterns (adjacent-cell coupling), 8259A interrupt
  controller functional test, "possible causes" mapping per
  failing verdict. Address-to-chip physical translator
  (QARAM pattern) deferred to v0.6+. Anti-patterns flagged:
  bundled destructive format tool (HDPREP had a silent
  interleave-corruption bug), OEM dealer-info hooks, licensed
  third-party code without traceability, forced-reboot test
  modes.

## Not produced this pass

The corpus still holds three tools untouched that would be
plausible future Phase 3+ candidates:

- **MTRRLFBE.** A FASTVID-family MTRR write-combining enabler
  for VESA Linear Framebuffers, UPX-packed. Same category as
  the T11 FASTVID correction; would need unpacking before
  useful research.
- **SYSINFO (Norton Utilities 8, Symantec 1993).** Classic
  distinguished DOS utility. Detection technique catalog
  likely to be rich. Full study would be its own pass.
- **DOOMS and QUAKES.** Full game installations, not
  benchmarks. id Software's TIMEDEMO mode in these engines
  provides community-reference fps numbers per timedemo; a
  separate research pass could extract the measurement
  methodology from those engines' source (which is now open-
  source, so the technique is documented upstream).

All deferred; not urgent.

## Upstream references

- Reinhold Weicker, "Dhrystone: A Synthetic Systems Programming
  Benchmark," *Communications of the ACM*, Oct 1984 + ACM
  SIGPLAN Notices, Aug 1988 (v2.1 revision).
- Harold J. Curnow and Brian A. Wichmann, "A Synthetic
  Benchmark," *The Computer Journal*, Vol. 19 No. 1, Feb 1976
  (original Whetstone algorithm).
- Intel 80287 / 80387 datasheets (FPU opcode semantics).
- Intel 8254 datasheet (PIT Channel 0 / 2 timing semantics
  that CACHECHK and CERBERUS both exploit).
- IIT 2C87 / 3C87 datasheets (undefined-opcode extensions used
  by CheckIt's vendor discrimination).
- VESA Bios Extensions (VBE) 1.2 and 2.0 specifications (3D
  benchmark Linear Framebuffer paths).
- JEDEC JEP-106 manufacturer identification codes (referenced
  by SPEEDSYS for DDR SPD analysis).
- Ben Castricum's UNP 4.11 (DOS executable unpacker, used for
  CACHECHK PKLITE unwrap).
