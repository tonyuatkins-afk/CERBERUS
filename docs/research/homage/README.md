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

## Not authorized / not produced this pass

- **T3 — CheckIt Whetstone.** Deferred pending v0.4 Whetstone
  FPU-asm rework decision.
- **T5 — CheckIt video benchmark methodology.** `bench_video`
  is already sealed at c507990; lessons can influence v0.5 but
  not v0.4.
- **T8 — CACHECHK UMC timer workaround.** Not directly
  CERBERUS-relevant unless a UMC chipset enters the target set.
- **T9, T10 — SPEEDSYS.** The `SPEEDSYS.HIS` file was
  sufficient for Phase 1 understanding at the level CERBERUS
  needs.

All of the above remain accessible for a future Phase 3 pass if
the project owner judges the effort warranted.

## Upstream references

- Reinhold Weicker, "Dhrystone: A Synthetic Systems Programming
  Benchmark," *Communications of the ACM*, Oct 1984 + ACM
  SIGPLAN Notices, Aug 1988 (v2.1 revision).
- Intel 80287 / 80387 datasheets (FPU opcode semantics).
- IIT 2C87 / 3C87 datasheets (undefined-opcode extensions used
  by CheckIt's vendor discrimination).
- Ben Castricum's UNP 4.11 (DOS executable unpacker, used for
  CACHECHK PKLITE unwrap).
