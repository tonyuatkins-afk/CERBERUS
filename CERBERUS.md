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

The 2026-04-21/22 M1 + M2 + M3 sessions added five more that only real iron found:

- **Watcom libc teardown hangs the run on BEK-V409.** Symptom: pressing Q after UI summary leaves the machine hung, hard-reset required. Stock atexit / FPU-cleanup / stdio-close chain executes cleanly under DOSBox-X and DOSBox Staging and on the 386 bench box, but deadlocks on the BEK-V409 486 DX-2-66 + AMI 11/11/92 + DOS 6.22 + HIMEM + EMM386 stack. Fix: replace the final `return exit_code` with `crumb_exit(); _exit((int)exit_code);` so the run bypasses libc teardown entirely and calls INT 21h AH=4Ch directly. Safe because CERBERUS registers no atexit handlers and releases all its own FDs explicitly before the return. (M1.7, `src/main.c`)
- **"*** NULL assignment detected" BSS overwrite is BEK-V409-specific.** Symptom: Watcom's NULL-guard canary fires on every BEK-V409 run but never on the 386 bench box, DOSBox-X, or DOSBox Staging. Cause: some probe path writes near DGROUP offset 0 on that specific chipset. Localized empirically to OPL fallback / S3 Trio64 / UMC491-PIT probes; root cause still open. Documented and filed for 0.8.1 via removal-at-a-time protocol (`docs/methodology.md` M1.7 section). The M2 probes ship with full isolation from this defect (new code uses explicit BSS segments, not near-zero accidentally).
- **Em-dashes compiled into the EXE as ΓÇö on CP437 CGA displays.** Symptom: em-dash characters in source strings render as 3 garbled characters on BEK-V409's CGA composite output, invisible under DOSBox-X's default IBM font. Cause: em-dash is a 3-byte UTF-8 sequence (E2 80 94) that CP437 renders as three separate glyphs. Fix: swept the tree and replaced every em-dash in runtime strings with colons, commas, or parentheses as context demanded. Same lesson applies to every Unicode character that compiles into an 80x25 text-mode string: the emulator forgives, CP437 does not. (M1 follow-up)
- **Whetstone per-unit cost anomaly (10-30× low K-Whet reading on real 486).** Symptom: Curnow-Wichmann kernel consistently reports ~109 K-Whet on BEK-V409 where the published reference envelope is 1500-3000 K-Whet. DOSBox-X produces numbers close to the reference because it fakes the FPU instruction throughput against a modern host cycle clock. Real 486 silicon says the kernel genuinely takes 50-100 ms per unit where research estimated 1-3 ms. Fix for 0.8.0: suppress the emit, ship `bench.fpu.ops_per_sec` as the primary FPU metric (honest ~1.17M on BEK-V409), defer per-instruction microbenchmarks to 0.9.0. (`docs/methodology.md` "Why Whetstone is not in 0.8.0").
- **bench_cpu iters/sec measured 1.96M on BEK-V409, outside the DB anchor band of 4.7-10.5M.** Symptom: Rule 4b (cpu_ipc_bench) would report WARN on a perfectly healthy 486 DX-2-66 if it were active. Cause: the original anchor band was seeded from DOSBox-X measurements on modern hosts, not real iron. Fix: widened the 486 DX-2 iters_low bound to 1,500,000 in `hw_db/cpus.csv` to encompass the real-iron measurement, leaving headroom for slower 486 variants. (M1.5)

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
| CPU       | Instruction probing (8088/86/V20/V30/286), CPUID (386+), vendor strings, class normalization to family token | Implemented |
| FPU       | FNINIT/FNSTSW presence test, 4-axis behavioral fingerprint (infinity mode, pseudo-NaN, FPREM1, FSIN) + 5th axis FPTAN v0.8.0-M2, 14-entry DB | Implemented |
| Memory    | Conventional (INT 12h), extended (INT 15h AH=88h/E801h with HIMEM intercept handling via XMS AH=08h), EMS/XMS driver probe | Implemented |
| Cache     | Class-inference from CPU DB; throughput-sweep characterization (L1 size + line size + write policy); 6-stride sweep to 128 bytes for Pentium+ line inference v0.8.0-M2 | Implemented |
| Bus       | PCI BIOS probe + ISA fallback, VLB possibility heuristic, class token (isa8/isa16/vlb/pci) | Implemented |
| Video     | Adapter waterfall INT 10h AH=1Ah → AH=12h BL=10h → BDA → 3BAh toggle; 28-entry chipset DB (MDA/CGA/Hercules/EGA/VGA/SVGA); S3 chipset ID via CR30 past IBM-VGA string collision | Implemented. Hercules-variant discrimination deferred to 0.8.1+ |
| Audio     | PC speaker, AdLib/OPL2/OPL3 probe via port 388h or BLASTER-base+8, SB DSP version (port base+0Eh for status, base+0Ah for data), 31-entry DB with T-suffix + CT1745 mixer discriminator | Implemented |
| BIOS      | Date string, family/vendor DB (21 entries), copyright extraction, INT 15h extensions, $PnP header | Implemented |

### Head II: DIAGNOSE

Validation. For each detected subsystem, runs targeted correctness tests.

| Test | What It Catches | Status |
|------|-----------------|--------|
| CPU ALU integrity | Stuck bits, failing flag logic, bad multiply/divide | Implemented (diag_cpu) |
| Memory integrity | Walking-1s, walking-0s, address-in-address, checkerboard + inv-checkerboard (v0.8.0-M2 adjacent-cell coupling coverage) | Implemented (diag_mem) |
| Cache health | Stride-ratio timing test (2 KB small vs 32 KB large); classifier `ratio >= 40x PASS` / `20-40x WARN` / `<20x FAIL`; skipped on pre-486 floor | Implemented (diag_cache) |
| Video RAM | Direct VRAM walk, 4-pattern integrity, retrace-gated writes on CGA | Implemented (diag_video) |
| FPU correctness | Bit-exact known-answer tests for FADD/FSUB/FMUL/FDIV/compound | Implemented (diag_fpu) |
| FPU behavioral fingerprint | 5-axis probe: infinity mode (projective vs affine), pseudo-NaN handling, FPREM1 presence, FSIN presence, FPTAN pushes-1.0 (v0.8.0-M2) | Implemented (diag_fpu_fingerprint) |
| FPU control modes | Rounding-control cross-check (4 modes, canonical IEEE-754 table verification); precision-control cross-check (3 modes, bytewise distinct 1/3 results); exception-flag roundtrip (6 exceptions IE/DE/ZE/OE/UE/PE deliberately triggered) | Implemented (v0.8.0-M2) |
| DMA | 8237 count-register write+readback probe on channels 1/2/3/5/6/7; channels 0/4 safety-skipped; XT-class slave-skip path | Implemented (diag_dma) |
| IEEE-754 edge-case coverage | 9 operand classes x 5 ops | **Deferred to 0.8.1+** (research gap L, DGROUP budget pressure) |
| 8259A interrupt-controller functional probe | | **Deferred to 0.8.1+** (QA-Plus homage candidate) |

Diagnostic results feed the consistency engine. If detection says "486DX" but the FPU correctness test fails, that is a reportable fact, not a crash.

### Head III: BENCHMARK

Measurement. Produces repeatable numbers with explicit methodology.

| Benchmark | Unit | Method | Status |
|-----------|------|--------|--------|
| CPU integer | iters/sec (MIPS-equivalent) | Fixed instruction mix, PIT-C2-timed | Implemented (bench_cpu) |
| Dhrystone 2.1 | Dhrystones/sec | Full Weicker port, -od -oi compile flags, DCE-barrier via report_add_u32 | Implemented (bench_dhrystone) |
| Whetstone | K-Whetstones/sec | Curnow-Wichmann x87 asm kernel, DCE-barrier checksum | **Compiled in; emit suppressed in stock 0.8.0 builds** (v0.8.0-M1, plan §7). `wmake WHETSTONE=1` re-enables emit for research work. See `docs/methodology.md` "Why Whetstone is not in 0.8.0" |
| FPU aggregate | fpu.ops_per_sec | x87 mix (FADD/FMUL/FDIV/FSQRT), PIT-timed | Implemented (bench_fpu). Primary FPU throughput metric in 0.8.0 |
| Memory bandwidth | KB/s (read, write, copy) | REP MOVSW (copy), REP STOSW (write), REP LODSB (read), microsecond-scaled | Implemented (bench_memory) |
| Cache bandwidth | KB/s (small vs large buffer) | 2 KB and 32 KB FAR buffers, PIT-C2-timed | Implemented (bench_cache) |
| Cache characterization | L1 size + line size + write policy | Size sweep 2/4/8/16/32 KB (inflection = L1 size); stride sweep 4/8/16/32/64/128 B (plateau = line size); read-vs-write delta (wb/wt/unknown) | Implemented (bench_cache_char). 6-stride sweep added in v0.8.0-M2 |
| Video throughput | KB/s (text + mode 13h) | Direct VRAM writes to text segment and mode 13h graphics | Implemented (bench_video) |
| Mandelbrot visual coda | not measured; visual | VGA mode 13h, 320x200x256, per-pixel progressive render | Implemented (bench_mandelbrot). Gated in stock builds (piggybacked on Whetstone); research builds render it |
| Memory latency | ns random access | Pointer-chase through randomized buffer | **Deferred to 0.8.1+** (research gap A) |
| L2 cache detection | KB/s at 64/128/256 KB working sets | | **Deferred to 0.8.1+** (research gap B, needs DGROUP reclaim + FAR buffer work beyond current 32 KB cap) |
| DRAM latency derivation | ns | 1000000 / (largest-sweep-kbps / line-size-kb) | **Deferred to 0.8.1+** (research gap E) |
| Per-instruction FPU microbench | cycles/FADD, FMUL, FDIV, FSQRT | | **Deferred to 0.9.0** (replacement story for Whetstone) |
| Disk throughput | KB/s sequential read | INT 13h raw sector reads | **Deferred to 0.9.0+** (needs validation surface) |

All benchmarks run under the timing subsystem in either quick or calibrated mode. Calibrated mode runs N passes, computes median + coefficient of variation, feeds the thermal-stability tracker (Mann-Kendall α=0.05).

## 6. Core Subsystems

### Timing

PIT Channel 2 gate-based measurement. Channel 2 is used because it can be started and stopped under software control without affecting Channel 0 (system tick). Resolution is approximately 838ns per PIT tick. Quick mode runs one pass. Calibrated mode runs N passes (default 5) and reports median and coefficient of variation. Any benchmark with CoV above a threshold is flagged as unstable.

### Display

Text-mode abstraction covering MDA, CGA, Hercules, EGA, and VGA. Detects adapter class at startup via the research-aligned waterfall: INT 10h AH=1Ah → INT 10h AH=12h BL=10h → BDA equipment flag → 3BAh bit 7 toggle (MDA vs Hercules). On CGA, all VRAM writes gate on 3DAh retrace edge via `tui_wait_cga_retrace_edge()` to avoid snow (v0.8.0-M3.1). All output is 80x25 compatible. Color is used where available but never load-bearing.

The `/MONO` flag (v0.8.0-M3.5) forces monochrome rendering regardless of detected adapter. Attribute mapping per MS-DOS UI-UX research Tier 0: 07h body, 0Fh emphasis, 01h underline, 70h reverse, F0h blink-reverse, 00h/08h reserved-invisible. All rendering paths query `display_is_mono()` which unifies the check across /MONO-forced and native-mono adapters.

EGA/VGA color adapters get INT 10h AX=1003h BL=00h at startup (v0.8.0-M3.6) to switch attribute byte bit 7 from blink-enable to background-intensity, giving 16 background colors. Zero-cost quality win.

### UI interaction (CUA-lite, v0.8.0-M3)

The scrollable summary's row 24 is a Norton-style F-key legend: `1Help  3Exit  Up/Dn  PgUp/Dn  Home/End  ...  rows X-Y of N`. Borland TStatusLine palette on color (0x30 base black-on-cyan, 0x3F hotkey bright-white-on-cyan); ATTR_INVERSE on mono. F1 opens a static help overlay (any key returns). F3 and Esc exit; Q remains as legacy alias. Arrow keys + PgUp/PgDn + Home/End scroll the virtual-row table.

No menu bar, no dropdown menus, no modal dialog system. These are deferred to 0.9.0 per plan §9 CUA-lite decision.

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

**v0.8.0: runtime upload is compiled out of stock binaries.** Per plan §8 (upload decision), the network-transmission code path is gated behind `#ifdef CERBERUS_UPLOAD_ENABLED`; stock builds contain no HTTP client, no HTGET shell-out, no crash-on-unreachable-endpoint code. Only the `[network]` transport detection and `/NICK` / `/NOTE` local INI annotation remain in stock.

Research builds (`wmake UPLOAD=1`) re-enable the full upload flow for development work: HTGET shell-out POST to `barelybooting.com/api/v1/submit`, response parsing, re-write of the INI with post-upload state. Endpoint must be deployed for the upload to succeed; contract at `docs/ini-upload-contract.md`.

Upload is on the 0.9.0 path, gated on: (a) server deployed and red-teamed, (b) at least one non-BEK-V409 machine round-tripped successfully, (c) circuit-breaker or health-check added to the client to survive endpoint unavailability gracefully.

## 7. Command-Line Interface

```
CERBERUS [/Q] [/C[:n]] [/ONLY:<h>] [/SKIP:<h>] [/O:file]
         [/NOCYRIX] [/NOINTRO] [/QUICK] [/NOUI] [/MONO]
         [/NICK:<name>] [/NOTE:<text>] [/NOUPLOAD] [/?]

  /Q              Quick mode (default)
  /C[:n]          Calibrated mode, n runs (default 7)
  /ONLY:DET|DIAG|BENCH   Run only that head
  /SKIP:DET|DIAG|BENCH   Skip that head
  /SKIP:TIMING    Skip PIT/BIOS timing self-check
  /SKIP:<module>  Skip a named module caught by a previous hang crumb
  /O:file         Output INI path (default CERBERUS.INI)
  /NOCYRIX        Skip Cyrix DIR probe (port 22h safety)
  /NOINTRO        Skip ANSI splash
  /NOUI           Skip summary + consistency UI; plain-text stdout batch
  /QUICK          Skip visual demonstrations (bit parade, Lissajous, etc.)
  /MONO           Force monochrome rendering regardless of adapter (v0.8.0-M3)
  /NICK:<name>    Nickname for INI annotation (alnum+space+hyphen, max 32)
  /NOTE:<text>    Note for INI annotation (printable ASCII, max 128)
  /NOUPLOAD       No-op in stock builds (upload not compiled in)
  /UPLOAD         Rejected in stock builds; requires wmake UPLOAD=1
  /U              Alias for /UPLOAD; same rejection in stock
  /?              Show help
```

## 8. Target Hardware

Claim hierarchy (v0.8.0 after M1-M3):

| Tier | CPU | RAM | Display | Bus | Validation status |
|------|-----|-----|---------|-----|-------------------|
| Floor | 8088 / 8086 / V20 / V30 | 256KB | MDA / CGA | ISA 8-bit | **Code paths present, XT-class real-iron validation pending** |
| 286 | 80286 / 80287 | 640KB | CGA / EGA | ISA 16-bit | **Code paths present, AT-class real-iron validation pending** |
| 386 | 80386DX / SX / Am386 / Cyrix 486DLC | 1-16 MB | EGA / VGA | ISA 16-bit | **Validated** on 386 DX-40 + IIT 3C87 + Genoa ET4000 (2026-04-21) |
| 486 | 486SX / 486DX / DX2 / DX4, AMD Am5x86 | 4-64 MB | VGA / SVGA | VLB / PCI | **Validated** on BEK-V409 (Intel i486DX-2-66 + S3 Trio64 + Vibra 16S + AMI 11/11/92) |
| Pentium | P5/P54C/P55C/Pentium MMX/Pentium Pro/II/III | 8+ MB | VGA / SVGA | PCI | **Code paths inherited, real-iron validation in flight** |

Current public claim: "Validated on 386 and 486. 286 and 8088 paths untested." Ship-valid per plan §4 three-tier path; upgrades as 286 and 8088 captures land.

Audio: PC Speaker, AdLib / OPL2 / OPL3, Sound Blaster family (DSP 1.x through 4.x including CT1745-equipped SB16 / Vibra 16S / Vibra 16).

Explicitly supported oddities: NEC V20/V30 with extended instruction set, RapidCAD FPU behavior, Cyrix instruction timing quirks, AMD 5x86, IIT 3C87 (capture present; DB discrimination via undefined-opcode probe deferred to 0.8.1+).

## 9. Build & Toolchain

Requires:

- [Open Watcom C/C++](http://open-watcom.github.io/) (v1.9 or v2.0 fork)
- [NASM](https://www.nasm.us/) for assembly modules
- Python 3 for the hardware-DB CSV → C regenerator and DGROUP audit tool
- DOS-compatible build environment (real DOS, DOSBox-X, or OpenWatcom on Windows/Linux cross-compiling)

Build variants:

```
wmake                             # stock 0.8.0 shipping build
wmake WHETSTONE=1                 # research: re-enable Whetstone emit
wmake UPLOAD=1                    # research: re-enable upload
wmake WHETSTONE=1 UPLOAD=1        # both enabled
wmake dgroup-report               # audit near-data budget against 64 KB ceiling
```

Produces `CERBERUS.EXE`, DOS real-mode, medium memory model (code >64 KB allowed, near data ≤64 KB). v0.8.0-M3 stock build: 166,898 bytes. DGROUP near-data: 60,976 bytes (59.5 KB), 2.4 KB headroom vs 62 KB soft target. Host-side unit test suite (run on Windows/Linux dev box, exercises pure-math + inference paths): 320 assertions across 9 suites.

## 10. Upload ecosystem (0.9.0+)

The upload client to `barelybooting.com/cerberus/` is **compiled out of stock 0.8.0 binaries per plan §8**. Research builds can re-enable via `wmake UPLOAD=1`. Full runtime path + server deployment + circuit-breaker + round-trip validation are the 0.9.0 scope.

The long-term ecosystem ambition (TLS 1.3 HTTPS POST via NetISA TSR, aggregated public hardware database, no Windows / no modern driver stack / no man in the middle) remains the 1.0.0 target. 0.8.0 preserves the infrastructure (contract doc, INI schema, network transport detection, /NICK + /NOTE annotation) so the 0.9.0+ revival is a compile-flag flip once the server is ready.

## 11. Repository Structure

```
cerberus/
  Makefile                  Watcom wmake build file
  README.md                 Top-level project readme
  CERBERUS.md               This document
  LICENSE                   License file
  src/
    cerberus.h              Master header (types, constants, opts_t)
    main.c                  Entry point, CLI parse_args, orchestration, _exit bypass (M1.7)
    core/
      timing.{h,c,_a.asm}   PIT timing subsystem, RDTSC backend gate, stats accumulator
      display.{h,c}         Adapter waterfall + attribute primitives + /MONO + 16-bg enable
      tui_util.{h,c}        Shared text-mode UI primitives + CGA snow gate (M3.1)
      report.{h,c}          INI output, system signature, dual-signature scheme
      sha1.{h,c}            Hash for signatures
      consist.{h,c}         Consistency engine (11 rules + possible-causes narration)
      thermal.{h,c}         Mann-Kendall α=0.05 trend test
      crumb.{h,c}           Crash-recovery breadcrumb (CERBERUS.LAST via DOS commit)
      ui.{h,c}              Scrollable three-heads summary + CUA-lite legend + F1 help
      intro.{h,c}           ANSI boot splash (v0.5.0 three-headed-dog)
      head_art.{h,c}        Three-heads directional CP437 art
      journey.{h,c}         Visual journey framework + title cards
      timing_metronome.c    PIT metronome visual
      audio_scale.{h,c}     End-of-journey audio scale (PC speaker / OPL2 / SB DSP)
      cache_buffers.{h,c}   Far buffer allocation for cache probes
    detect/
      detect.h              Head I interface
      detect_all.c          Detection orchestrator
      env.{h,c}             Emulator detection + confidence clamping
      unknown.{h,c}         Unknown-hardware capture to CERBERUS.UNK
      cpu.{h,c,_a.asm}      CPU detection (instruction probing + CPUID + family token)
      cpu_db.{h,c}          Generated from hw_db/cpus.csv (34 entries)
      fpu.{h,c,_a.asm}      FPU detection (FNINIT/FNSTSW sentinel)
      fpu_db.{h,c}          Generated from hw_db/fpus.csv (14 entries)
      mem.{h,c,_a.asm}      Memory detection (INT 12h, INT 15h AH=88h/E801h, XMS AH=08h)
      cache.{h,c}           Cache class-inference from CPU DB
      bus.{h,c}             PCI + ISA + VLB-possibility bus detection
      video.{h,c}           Adapter detection, S3 CR30 chipset ID, 28-entry DB
      video_db.{h,c}        Generated from hw_db/video.csv
      audio.{h,c}           PC speaker + OPL + SB DSP probes, 31-entry DB with mixer_chip column
      audio_db.{h,c}        Generated from hw_db/audio.csv
      bios.{h,c}            BIOS date + family + $PnP scan, 21-entry DB
      bios_db.{h,c}         Generated from hw_db/bios.csv
      network.{h,c}         Transport detection (NetISA/pktdrv/mTCP/WATTCP)
    diag/
      diag.{h,c,_a.asm}     Head II interface + shared types
      diag_all.c            Diagnostics orchestrator
      diag_cpu.c            ALU + MUL + shift integrity probes
      diag_mem.c            Walking-1s/0s + addr-in-addr + checkerboard + inv-checkerboard (M2.7)
      diag_fpu.c            FPU bit-exact correctness tests
      diag_fpu_fingerprint.{c,_a.asm}   5-axis fingerprint + rounding/precision/exception probes (M2)
      diag_video.c          VRAM walk + 4-pattern integrity
      diag_cache.c          Stride-ratio health probe
      diag_dma.c            8237 count-register probe (channels 1/2/3/5/6/7)
      diag_bit_parade.c     Journey visual: CPU ALU bit parade
      diag_lissajous.c      Journey visual: FPU Lissajous
      diag_latency_map.c    Journey visual: cache latency heatmap
    bench/
      bench.h, bench_all.c  Head III interface + orchestrator
      bench_cpu.c           Integer throughput benchmark
      bench_memory.c        REP MOVSW/STOSW/LODSB bandwidth
      bench_fpu.c           Aggregate FPU mix (primary FPU throughput metric in 0.8.0)
      bench_cache.c         Small/large buffer throughput (2 KB vs 32 KB)
      bench_cache_char.c    L1 size + line size + write policy characterization (6-stride v0.8.0-M2)
      bench_video.c         Text-mode + mode 13h VRAM throughput
      bench_dhrystone.c     Dhrystone 2.1 port (Weicker)
      bench_whetstone.c     Curnow-Wichmann Whetstone (compiled, stock-emit suppressed)
      bench_whet_fpu.asm    x87 asm kernel for Whetstone
      bench_mandelbrot.c    VGA mode 13h visual coda (gated with Whetstone)
      bench_cache_waterfall.c   Journey visual: 9-band memory cache waterfall
    upload/
      upload.{h,c}          Upload client (runtime path #ifdef'd out in stock builds)
  docs/
    CERBERUS_0.8.0_PLAN.md  Release doctrine for 0.8.0
    methodology.md          How each measurement works
    consistency-rules.md    Consistency engine logic
    ini-format.md           INI schema reference
    ini-upload-contract.md  Client/server API contract for upload
    contributing.md         PR and commit conventions
    quality-gates/          Per-milestone adversarial gate outcomes
    sessions/               Development session reports
    research/homage/        Ethical homage research on 17 DOS-era reference tools
    Cache Test Research.md  Source document for M2 cache-probe scope
    FPU Test Research.md    Source document for M2 FPU-probe scope
    General Test Research.md  Source document for broader scope items
  tests/
    host/                   OpenWatcom Win32 console host tests (320 assertions, 9 suites)
    target/                 Real-hardware validation protocols (VALIDATION-486.md, VALIDATION-0.8.0-M1.md)
    captures/               Archived real-hardware INI captures, per-machine READMEs
  tools/
    dgroup_check.py         Near-data budget auditor (wmake dgroup-report)
    repstosd/               Issue #6 VRAM-throughput standalone test
  hw_db/
    cpus.csv, fpus.csv, video.csv, audio.csv, bios.csv   Human-editable hardware DBs
    build_*.py              CSV-to-C generators (wmake regen-cpu-db etc.)
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
