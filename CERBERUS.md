# CERBERUS

**Retro PC System Intelligence Platform**

Version: 1.0 (Master Specification)
Author: Tony Atkins / Barely Booting
Part of the NetISA ecosystem

---

## 1. What CERBERUS Is

CERBERUS is a unified hardware analysis tool for IBM PC/XT through 486-class systems running DOS. It answers three questions about a vintage PC in a single run:

1. **What is it?** (Detection)
2. **Is it working correctly?** (Diagnostics)
3. **How fast is it actually?** (Benchmarks)

It is named after the three-headed dog of Greek myth that guarded the gates of the underworld. Each head is one of the three analysis domains. They share a common body of timing, display, and reporting infrastructure, and their results are cross-checked against each other by a consistency engine before being reported.

CERBERUS is not a synthetic benchmark. It does not produce a single performance score. It measures subsystems independently, validates them against expected behavior for the detected hardware, and reports every result with an explicit confidence level. When results disagree with what the hardware claims to be, CERBERUS says so.

## 2. What CERBERUS Is Not

- Not a "is my PC fast" toy. It is a diagnostic and measurement instrument.
- Not a Windows utility port. It targets real-mode DOS on period-correct hardware.
- Not a closed-source benchmark. Every measurement method is documented, reproducible, and auditable.
- Not a gatekeeper. It runs on an 8088 with 256KB RAM and a monochrome display. It scales up gracefully to a 486 with VGA and an FPU, but never requires them.

## 3. Status

**v0.8.0-M3** on `main` 2026-04-22. Third milestone of the 0.8.0 "trust and validation" release. The 0.8.0 release doctrine is at [`docs/CERBERUS_0.8.0_PLAN.md`](docs/CERBERUS_0.8.0_PLAN.md).

Release arc (shipped):

- **v0.1.1-scaffold** through **v0.4.0**: Heads I-III scaffolding, then filled in and real-hardware-proven on BEK-V409 (Intel i486DX-2-66 bench box).
- **v0.5.0**: Scrollable three-heads summary UI, Whetstone x87 asm kernel, Mandelbrot FPU visual demo.
- **v0.6.x**: Visual Diagnostics Journey (bit parade, Lissajous, cache waterfall, latency heatmap, PIT metronome, audio scale), OPL2 FM + SB DSP direct PCM audio paths.
- **v0.7.0-rc1 / rc2**: Community upload client (Part A), end-to-end quality-gate fixes.
- **v0.7.1**: Cache characterization + FPU behavioral fingerprint.
- **v0.8.0-M1** (2026-04-21): trust-and-validation milestone. Whetstone emit suppressed in stock builds. Runtime upload compiled out of stock builds. Nickname buffer leak fixed. `cpu.class` normalized to family token. `bench_cpu` DB anchor widened. End-of-run `_exit` bypass for Watcom libc teardown hangs. DGROUP audit tooling. Real-hardware validation on 486 and 386; claim hierarchy currently "Validated on 386 and 486. 286 and 8088 paths untested."
- **v0.8.0-M2** (2026-04-21 overnight): precision expansion. FPU research-gap probes I/J/K/M (FPTAN, rounding-control, precision-control, exception-flag roundtrip). Cache stride=128 for Pentium+ line-size inference. Memory checkerboard + inv-checkerboard patterns. M2.5 IEEE-754 edges and M2.8 CSV output deferred under DGROUP pressure.
- **v0.8.0-M3** (current, 2026-04-22 overnight): CUA-lite interaction polish. F1 help overlay, F3 exit, Norton-style F-key legend on row 24 (Borland palette on color, ATTR_INVERSE on mono), /MONO flag for forced monochrome rendering, 16-background-color mode enabled on EGA/VGA, CGA snow-safety gate on all VRAM writes. Adapter-tier waterfall documentation aligned with MS-DOS UI-UX research Part B. No menu bar, no dropdowns, no modal dialog system (deferred to 0.9.0).

Remaining milestones for 0.8.0 tag (per plan):

- **M4**: documentation parity (README full refresh, CERBERUS.md sections beyond §3 Status, session report for M1-M3 arc), release notes, tag `v0.8.0`.

## Why real hardware

DOSBox-X carries CERBERUS day-to-day. The iteration loop is seconds instead of minutes, and for the large class of bugs the emulator models correctly, there is no reason to boot a 486. But every real-iron validation session on the BEK-V409 / i486DX-2 / S3 Trio64 / Vibra 16S bench box surfaces bugs DOSBox-X cannot reproduce. From the 2026-04-18 session alone, five of them:

- **HIMEM.SYS intercepts INT 15h AX=E801h.** Symptom: extended memory reported as 0 KB on a 64 MB machine. Cause: once HIMEM is resident it claims the BIOS extended-memory interrupts, and CERBERUS was trusting the BIOS answer. Fix: acquire the XMS entry point via INT 2Fh AX=4310h and query AH=08h directly. (`eeba319`)
- **S3 Trio64 option ROM advertises "IBM VGA" before "S3"/"Trio64".** Symptom: S3 Trio64 identified as IBM VGA — vendor, chipset, and family all wrong. Cause: the card's option ROM carries an IBM-VGA compatibility string for real-mode fallback, and the substring scan matched the first hit. Fix: unlock S3 extended CRTC registers and read CR30 (0xE1 = Trio64, 0xE6 = Virge) ahead of any BIOS-string scan. (`eeba319`)
- **Vibra 16S DSP status is at base+0x0E, not base+0x0A.** Symptom: DSP version probe returned garbage on a known-working Vibra 16S. Cause: base+0x0A is the DSP DATA register, not STATUS; Creative's documented status port is base+0x0E. Fix: poll the right port. (`eeba319`)
- **OPL 0x388 mirror disabled by CTCM on Vibra 16 PnP.** Symptom: OPL3 undetected on a card that plays OPL in DOOM without issue. Cause: after CTCM's PnP init, the legacy 0x388 Adlib mirror can be switched off — OPL only answers at BLASTER-base+8. Fix: probe BLASTER-base+8 first, fall back to 0x388. (`eeba319`)
- **UMC491-integrated 8254 produces biased latch-race phantom wraps DOSBox never synthesizes.** Symptom: PIT-C2 vs BIOS-tick cross-check reports 49% divergence every run, repeatable to the microsecond. Cause: the integrated 8254's composite LSB/MSB misreads cluster at consistent mid-range values that a naive wrap detector treats as real counter transitions. Fix: upper-bound the wrap count, require a low-band→high-band shape for a valid wrap, and reject the whole measurement if post-hoc PIT/BIOS divergence exceeds 25% rather than reporting biased data. (`eeba319`, refined in `6c3a023`)

DOSBox-X is necessary for iteration speed and insufficient for correctness. Real-hardware validation is a non-negotiable gate on every version tag. The emulator is the dress rehearsal. The bench box is opening night.

## 4. Design Principles

These are non-negotiable and override scope expansion arguments.

- **Accuracy over spectacle.** Measure real behavior, not marketing-grade synthetic numbers.
- **Separation over aggregation.** Every subsystem measured independently. No composite score.
- **Transparency over black boxes.** Every result carries a confidence level and the method used.
- **Repeatability over one-off runs.** Calibrated mode runs each test N times and reports median plus variance.
- **Compatibility over optimization.** If a technique costs 8088 support, it costs too much.
- **Evidence over claims.** The hardware is whatever it behaves as, not whatever it identifies as.

## 5. The Three Heads

### Head I: DETECT

Hardware inventory. Identifies what is present without validating or measuring it.

| Subsystem | Method | Status |
|-----------|--------|--------|
| CPU       | Instruction probing (8088/86/V20/V30/286), CPUID (386+), vendor strings | Implemented |
| FPU       | FNINIT/FNSTSW presence test, control word read, 287 vs 387 vs integrated | Stubbed |
| Memory    | Conventional (INT 12h), extended (INT 15h AH=88h/E820), EMS/XMS driver probe | Partial |
| Cache     | Timed stride access pattern, size and line-width inference | Stubbed |
| Bus       | ISA/VLB/PCI detection, slot width, effective bus clock estimation | Stubbed |
| Video     | Adapter class (MDA/CGA/Hercules/EGA/VGA), chipset signature where possible | Stubbed |
| Audio     | PC speaker confirmed, AdLib/OPL2/OPL3 probe, Sound Blaster DSP version | Stubbed |
| BIOS      | Date string, copyright, INT 15h extensions, PnP header | Implemented |

### Head II: DIAGNOSE

Validation. For each detected subsystem, runs targeted correctness tests.

| Test | What It Catches |
|------|-----------------|
| ALU integrity | Stuck bits, failing flag logic, bad multiply/divide |
| Flag register | Missing or bogus flag behavior (common in clones and emulators) |
| Memory integrity | Pattern/address/moving-inversion test on conventional RAM |
| Cache coherence | Write-through vs write-back validation, stale-line detection |
| Video RAM | Direct VRAM walk, snow pattern on CGA, EGA/VGA plane consistency |
| FPU correctness | Known-answer tests for add/mul/div, transcendentals, precision control |
| System bus | Back-to-back I/O timing sanity, DMA channel liveness |

Diagnostic results feed the consistency engine. If detection says "486DX" but the FPU correctness test fails, that is a reportable fact, not a crash.

### Head III: BENCHMARK

Measurement. Produces repeatable numbers with explicit methodology.

| Benchmark | Unit | Method |
|-----------|------|--------|
| CPU integer | Dhrystones-equivalent MIPS | Fixed instruction mix, PIT-timed, no compiler optimization games |
| FPU | Whetstone-equivalent MFLOPS | x87 instruction mix, PIT-timed |
| Memory bandwidth | KB/s (read, write, copy) | REP MOVSW, REP STOSW, large buffer |
| Memory latency | ns (random access) | Pointer-chase through randomized buffer, cache-defeating |
| Cache bandwidth | KB/s (L1, L2 where present) | Stride tests at multiple working-set sizes |
| Video throughput | KB/s (VRAM write) | Direct write to text and graphics segments |
| Disk throughput | KB/s (sequential read) | INT 13h raw sector reads, optional |

All benchmarks run under the timing subsystem in either quick or calibrated mode.

## 6. Core Subsystems

### Timing

PIT Channel 2 gate-based measurement. Channel 2 is used because it can be started and stopped under software control without affecting Channel 0 (system tick). Resolution is approximately 838ns per PIT tick. Quick mode runs one pass. Calibrated mode runs N passes (default 5) and reports median and coefficient of variation. Any benchmark with CoV above a threshold is flagged as unstable.

### Display

Text-mode abstraction covering MDA, CGA, Hercules, EGA, and VGA. Detects adapter class at startup. On CGA, uses retrace-synced writes to avoid snow. All output is 80x25 compatible. Color is used where available but never load-bearing.

### Report

INI-format output, one key-value pair per line, grouped by section. Human-readable and machine-parseable. A system signature (SHA-1 prefix of a canonical subset of detected values) is included for deduplication when results are pooled.

Example output structure:

```ini
[cerberus]
version=1.0
date=2026-04-15
mode=calibrated
runs=5
signature=a3f9c21b

[cpu]
detected=80486DX
vendor=Intel
clock_mhz=33.0
clock_confidence=high
cpuid_available=yes

[fpu]
detected=integrated
diagnose=pass

[memory]
conventional_kb=640
extended_kb=15360
xms_driver=HIMEM.SYS 3.20

[bench.cpu]
dhrystones_median=7820
dhrystones_cov=0.012
mips_equivalent=12.1

[consistency]
486dx_fpu=pass (486DX reports integrated FPU)
fpu_diag_bench=pass (diag and bench agree on FPU liveness)
extmem_cpu=pass (extended memory consistent with CPU class)
timing_independence=pass (PIT C2 and BIOS tick agree within 15%)

[thermal]
cpu=pass (cpu bench: S=0, |S|<17, no significant trend at N=7)
cpu.s=0
cpu.direction=flat

[timing]
cross_check.pit_us=220214
cross_check.bios_us=219700
cross_check.status=ok
```

### Consistency Engine ("Truth Engine")

Cross-checks detection against diagnosis against benchmark. If a 486DX is detected but the integer benchmark reports 286-class numbers, that is flagged. If the FPU is detected but fails precision tests, that is flagged. The engine never silently overrides detection. It reports disagreement.

### Thermal Stability

For calibrated runs, tracks drift across passes. A subsystem whose per-pass timing trends monotonically UP across at least five passes (numbers climb = CPU slowing) is flagged as thermally unstable. Down-trending (warmup) is treated as benign. Uses the Mann-Kendall non-parametric trend test at α=0.05. Useful for detecting marginal voltage regulators, failing capacitors, and dusty heatsinks.

### Upload

When invoked with `/U` and the NetISA TSR is loaded, CERBERUS issues an HTTPS POST of the INI file to a community results endpoint. Upload is opt-in per run and never automatic. See Section 10.

## 7. Command-Line Interface

```
CERBERUS [/Q] [/C[:n]] [/D] [/B] [/O:file] [/U] [/?]

  /Q       Quick mode, single pass (default)
  /C[:n]   Calibrated mode, n runs per test (default 5)
  /D       Detection only (skip diagnose and benchmark)
  /B       Skip diagnostics (detect and benchmark only)
  /O:file  Write INI report to file (default CERBERUS.INI)
  /U       Upload results via NetISA
  /?       Show help
```

## 8. Target Hardware

| Class | CPU | Minimum RAM | Display | Bus |
|-------|-----|-------------|---------|-----|
| Floor | 8088 / 8086 / V20 / V30 | 256KB | MDA | ISA 8-bit |
| Common | 80286 / 386SX / 386DX | 640KB | CGA / Hercules / EGA | ISA 16-bit |
| Ceiling | 486SX / 486DX / DX2 / DX4, RapidCAD, Cyrix, AMD | 4MB+ | VGA | VLB / PCI |

Audio: PC Speaker, AdLib / OPL2 / OPL3, Sound Blaster family.

Explicitly supported oddities: NEC V20/V30 with extended instruction set, RapidCAD FPU behavior, Cyrix instruction timing quirks, AMD 5x86.

## 9. Build & Toolchain

Requires:

- [Open Watcom C/C++](http://open-watcom.github.io/) (v1.9 or v2.0 fork)
- [NASM](https://www.nasm.us/) for assembly modules
- DOS-compatible build environment (real DOS, DOSBox-X, or OpenWatcom on Windows/Linux cross-compiling)

Build:

```
wmake
```

Produces `CERBERUS.EXE`, DOS real-mode, large memory model. Target size under 64KB for diskette-friendly distribution.

## 10. NetISA Integration

CERBERUS is the first DOS application designed from day one to integrate with NetISA, the open-source TLS 1.3 networking card for vintage PCs. When the NetISA TSR is resident:

- `/U` triggers an HTTPS POST of the INI output to a public results endpoint
- Payload is the raw INI, prefixed with the system signature
- TLS 1.3 handshake and cert validation are handled by the NetISA card
- The host PC never sees a cryptographic primitive

This produces the ecosystem effect: a vintage PC can securely upload its own hardware profile over the open internet, from DOS, over WiFi. Aggregated, this becomes a public database of real-world vintage PC configurations and measured performance. No Windows, no modern driver stack, no man in the middle.

Upload is opt-in per invocation. CERBERUS never uploads without `/U`. The INI file is always written locally first.

## 11. Repository Structure

```
cerberus/
  Makefile                  Watcom wmake build file
  README.md                 Top-level project readme
  CERBERUS.md               This document
  LICENSE                   License file
  src/
    cerberus.h              Master header (types, constants)
    main.c                  Entry point, CLI, orchestration
    core/
      timing.h/c            PIT timing subsystem
      display.h/c           Text-mode display abstraction
      report.h/c            INI output, system signature
      consist.h/c           Consistency engine
      thermal.h/c           Thermal stability tracking
    detect/
      detect.h              Head I interface
      cpu.c                 CPU detection (8088-486, CPUID)
      cpu_a.asm             CPU detection ASM (NASM)
      fpu.c                 FPU detection
      mem.c                 Memory detection
      cache.c               Cache detection
      bus.c                 Bus detection
      video.c               Video detection
      audio.c               Audio detection
      bios.c                BIOS info
      detect_all.c          Detection orchestrator
    diag/
      diag.h                Head II interface
      diag_all.c            Diagnostics orchestrator
      (per-subsystem diagnostic modules)
    bench/
      bench.h               Head III interface
      bench_all.c           Benchmark orchestrator
      (per-subsystem benchmark modules)
    upload/
      upload.h              NetISA upload interface
      upload.c              Upload implementation
  docs/
    methodology.md          How each measurement works
    consistency-rules.md    Truth engine logic
    contributing.md         PR and commit conventions
```

## 12. Release & Distribution Plan

Aligned with the validated DOS scene distribution strategy used for TAKEOVER.

**Primary channels:**

- **GitHub**: source of truth, issue tracker, releases
- **itch.io**: player-facing channel, free download, screenshots, devlog
- **VOGONS**: announcement thread in the appropriate subforum, build-in-public updates
- **dosgame.club Mastodon**: progress posts during development

**Packaging:**

- Raw DOS build (CERBERUS.EXE plus README) as a zip
- DOSBox-X launcher bundle for modern systems, same binary plus preconfigured conf
- Source tarball for the archivists

**Exposure targets:**

- DOS Game Jam Demo Disc submission once v1.0 ships
- VCF East / VCF Midwest live demo with a period-correct machine
- Hackaday coverage pitch once hardware-side (NetISA) prototype is public

**Release cadence:**

Tagged releases at each milestone (v0.2 through v1.0). Nightly builds not distributed. No pre-release binaries without a corresponding tag.

## 13. Content Strategy (Barely Booting)

CERBERUS is not just a tool. It is filmable content for the Barely Booting YouTube channel, and the development arc is structured accordingly.

- **Process transparency.** Dead ends, measurement errors, and discovered hardware quirks are filmed and kept in, not edited out. Adrian's Digital Basement and Usagi Electric have both proven this is what the audience wants.
- **Episode arcs map to milestones.** "Detecting the 486" is one episode. "Catching a counterfeit Cyrix" is another. "Uploading from an 8088 over WiFi" is the capstone.
- **The slightly unbelievable factor.** "A real-mode DOS program on an 8088 just POSTed its hardware inventory to the internet over TLS 1.3" is the hook.
- **Community as collaborator.** VOGONS, dosgame.club, and the NetISA Discord are invited in early. Results database is a community artifact, not a Tony artifact.

## 14. AI Assistance Disclosure

CERBERUS follows the AI coding assistant policy adopted from the Linux kernel `coding-assistants.rst` across all Barely Booting projects:

- Every commit that used AI assistance carries an `Assisted-by: AGENT:MODEL` tag
- The human author bears full responsibility for every line committed
- AI tools do not author `Signed-off-by` lines
- "The AI wrote it" is never an acceptable explanation for a defect
- AI-generated code passes the same review and test gates as human-authored code
- AI usage is disclosed openly in commits, not hidden

This policy exists because the DOS scene values craft and honesty. Hiding AI assistance would be dishonest. Pretending AI did not help would be dishonest. Disclosing it and taking responsibility for the result is the only honest option.

## 15. Prior Art & References

CERBERUS stands on a long tradition of DOS system-information tools. The design borrows what works and deliberately avoids what does not.

- **ASTRA** (VOGONS community): closest modern peer in spirit. Active community project, diagnostics focus. CERBERUS differs by adding benchmarks and a consistency engine, and by uploading over NetISA.
- **CheckIt**, **QAPlus**, **AMI Diag**: commercial ancestors. Taught the industry what detection and diagnostics look like on DOS. CERBERUS is the open-source descendant.
- **Landmark Speed Test**, **Norton SI**, **3DBench**: benchmark ancestors. CERBERUS deliberately does not produce a single composite score in the Norton SI tradition, because doing so hides more than it reveals.
- **CPUID utilities**: modern CPUID-only tools are a useful reference but insufficient on their own for pre-586 hardware.

## 16. License

TBD. Candidates: MIT or BSD 2-clause. Decision before v1.0 tag.

(NetISA uses MIT for software and CERN-OHL-P for hardware. CERBERUS is software only, so MIT is the default unless a specific reason emerges to choose otherwise.)

## 17. Author

Tony Atkins, Barely Booting
GitHub: [@tonyuatkins-afk](https://github.com/tonyuatkins-afk)
YouTube: Barely Booting

---

*"What your hardware actually is, whether it's working, and where the bottlenecks are. Three heads. One answer."*
