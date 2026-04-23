# CERBERUS

> **Status: `v0.8.1` released 2026-04-22. Active development paused 2026-04-23 while the maintainer focuses on NetISA. See [PARKING.md](PARKING.md) for resume notes. The tagged release is production-ready; nothing half-landed on `main`.**

DOS-native hardware detection, diagnostic, and benchmark tool for real-mode IBM PC / XT / AT and 486-class machines. Single EXE; v0.8.1 stock build is ~167 KB. Targets an 8088 with 256KB and an MDA card as the design floor (XT-class validation pending per the current claim hierarchy) and scales up through a 486 with VGA, proven on BEK-V409 real iron.

Part of the Barely Booting / NetISA ecosystem. CERBERUS is the tool; [barelybooting-server](https://github.com/tonyuatkins-afk/barelybooting-server) is the companion web app that ingests uploaded CERBERUS.INI runs at `barelybooting.com/cerberus/`.

![CERBERUS v0.4.0 three-pane UI on a 486 DX-2](docs/releases/v0.4.0/screenshots/three-pane-ui-closeup.jpg)

## Status

**`v0.8.1` tagged on `main` 2026-04-22.** Completion release on top of `v0.8.0`: IEEE-754 edge-case diagnostic, `/CSV` output, L1 pointer-chase latency, 64 KB L2 reach, DRAM ns derivation, IIT 3C87 DB routing, Genoa ET4000 chip-level probe, Hercules variant discrimination. Release notes: [`docs/releases/v0.8.1.md`](docs/releases/v0.8.1.md). Planned next release is `v0.9.0` per [`docs/CERBERUS_0.9.0_PLAN.md`](docs/CERBERUS_0.9.0_PLAN.md) when development resumes.

**`v0.8.0` tagged 2026-04-22** (same day, earlier). Fourth-milestone release of the 0.8.0 "trust and validation" arc. M1 closed credibility traps and established validation corpus; M2 added research-gap FPU probes and cache stride extension; M3 closed the interaction-axis gap with CUA-standard keybindings, Norton-style F-key legend, F1 help overlay, /MONO flag, 16-bg-color enable on EGA/VGA, and CGA snow-safety gate; M4 landed documentation parity + possible-causes narration on consistency rules. Release doctrine at [`docs/CERBERUS_0.8.0_PLAN.md`](docs/CERBERUS_0.8.0_PLAN.md); release notes at [`docs/releases/v0.8.0.md`](docs/releases/v0.8.0.md).

Real-hardware validation status: **Validated on 386 and 486. 286 and 8088 paths untested.** Per plan section 10 claim hierarchy, 286 and 8088/XT captures will upgrade the claim as hardware becomes accessible. Current captures archived in `tests/captures/`:
- BEK-V409 (Intel i486DX-2-66 + AMI 11/11/92 + S3 Trio64 + Vibra 16S + 63 MB XMS)
- 386 DX-40 + IIT 3C87 + Genoa ET4000 + Aztech ISA + AMI 02/02/91 + ~16 MB

### What M1, M2, and M3 changed (full detail in [CHANGELOG.md](CHANGELOG.md))

**M1 (trust-first cuts)**: Whetstone emit suppressed in stock builds (`wmake WHETSTONE=1` to re-enable); runtime upload compiled out of stock binaries (`wmake UPLOAD=1` to re-enable). Fixes: nickname buffer leak (issue #9), `cpu.class` normalization to family token, `bench_cpu` DB anchor widened, end-of-run `_exit` bypass. Infrastructure: DGROUP audit tool, real-hardware validation corpus, quality-gate framework.

**M2 (precision expansion)**: FPU research-gap probes land for FPTAN behavior (I), rounding-control cross-check (J), precision-control cross-check (K), exception-flag roundtrip (M). Cache stride sweep extended to 6 points including stride=128 (enables line=32/64 inference for Pentium+). Memory checkerboard + inv-checkerboard patterns added (catches adjacent-cell coupling faults). M2.5 IEEE-754 edge cases and M2.8 CSV output deferred to post-M2 under DGROUP budget pressure.

**M3 (CUA-lite interaction)**: F1 help overlay, F3 exit (CUA), Norton-style F-key legend replaces status bar on row 24 (Borland 0x30/0x3F palette on color, ATTR_INVERSE on mono). `/MONO` flag forces monochrome rendering regardless of adapter. `AX=1003h BL=00h` on EGA/VGA for 16-background-color mode. CGA snow safety: all VRAM writes gate on 3DAh retrace edge. Adapter-tier waterfall documentation aligned with MS-DOS UI-UX research Part B. No menu bar, no dropdown menus, no modal dialog system (deferred to 0.9.0).

Historical arc: `v0.1.1-scaffold` → `v0.2-rc1` → `v0.3-rc1` → `v0.4-rc1` → **`v0.4.0`** → **`v0.5.0`** → **`v0.6.0`** → `v0.6.1` → `v0.6.2` → `v0.7.0-rc1` → **`v0.7.0-rc2`** → **`v0.7.1`** → `v0.8.0-M1` → `v0.8.0-M2` → `v0.8.0-M3` → **`v0.8.0`** (current).

### Known issues at M1 close-out (carried to M2)

- BSS overwriter on BEK-V409 corrupts the first 32 bytes of DGROUP; Watcom's `*** NULL assignment detected` canary fires on every exit. On probe paths that spill past the 32-byte guard, adjacent CONST string data is corrupted (e.g. `[bios] dree=` instead of `date=`). Empirically localized to BEK-V409-specific hardware paths (S3 Trio64 probe, Vibra 16S DSP + OPL fallback, UMC491 PIT wrap-guard). Not reproducible on 386 real iron or DOSBox-X. M2 investigation targeted.
- IIT 3C87 mis-tagged as Intel 80387 on the 386 capture. FPU DB coverage gap; homage research (`docs/research/homage/checkit-fpu-detection.md`) anticipated this as a v0.5+ deferred capability. M2 candidate.
- Genoa ET4000 detected as generic `adapter=vga`. Video DB has Tseng ET4000 entries, but the Genoa OEM BIOS doesn't surface the expected signature. M2 probe-path investigation.
- Intermittent OPL detection on Vibra 16 PnP (issue #2, pre-existing).

**Subsystem state at v0.8.0:**

| Subsystem | Status |
|---|---|
| Scaffold, timing (PIT C2 + RDTSC backend), INI writer, dual signatures (identity + run), crash-recovery breadcrumb | Complete |
| Emulator detection + confidence clamping | Complete |
| Detection: CPU / FPU / memory / cache / bus / video / audio / BIOS | Complete. CPUID-capable paths + pre-CPUID instruction probing + normalized cpu.class family token (v0.8.0-M1) |
| Hardware identification databases (CPU 34 / FPU 14 / video 28 / audio 31 / BIOS 21 = 128 seeds) | Complete + regen via `wmake regen-<subsystem>-db` |
| Audio DB `mixer_chip` column (CT1745 discriminator for SB16 family) | Complete |
| Unknown-hardware submission path (`CERBERUS.UNK`) | Complete |
| Scrollable three-heads summary UI + directional head art | Complete |
| CUA-lite interaction (F1 help overlay, F3 exit, Norton-style F-key legend, Esc/Q aliases) | Complete v0.8.0-M3 |
| /MONO flag with MDA-valid attribute clamping | Complete v0.8.0-M3.5 |
| 16-background-color mode on EGA/VGA (INT 10h AX=1003h BL=00h) | Complete v0.8.0-M3.6 |
| CGA snow-safety gate (3DAh retrace sync on every VRAM write) | Complete v0.8.0-M3.1 |
| Diagnostic tests (ALU, memory walking-1s/0s/AinA + **checkerboard M2.7**, FPU bit-exact + 5-axis behavioral fingerprint + rounding/precision/exception probes M2, video RAM, cache-stride, DMA-controller) | Complete |
| Benchmark suite (integer, FPU aggregate, memory, cache, video, Dhrystone) | Complete |
| Whetstone kernel + Mandelbrot coda | Compiled but **stock-emit suppressed** per v0.8.0-M1 trust-first decision. `wmake WHETSTONE=1` re-enables for research |
| Cache characterization (L1 size + line size + write policy + stride=128 Pentium+) | Complete v0.8.0-M2.1 |
| Consistency engine (11 rules + possible-causes narration M4.1) and thermal stability (Mann-Kendall α=0.05, N≥5) | Complete |
| Timing self-check (PIT C2 vs BIOS tick cross-check, rule 4a) | Complete |
| Homage research lessons (17 DOS-era reference tools studied across Phase 1-3) | Complete |
| Upload (HTGET shell-out POST to barelybooting.com) | **Compiled out of stock builds** per v0.8.0-M1. `wmake UPLOAD=1` re-enables. 0.9.0 revival target |
| Real-hardware validation corpus | 486 + 386 archived; 286 + XT-class pending. `tests/captures/<class>-<id>-<date>/` |
| Quality-gate framework | `docs/quality-gates/M1-gate-*.md` through `M3-gate-*.md`, adversarial-review patterns documented |
| DGROUP audit tool | `wmake dgroup-report` / `tools/dgroup_check.py` |

See [CERBERUS.md](CERBERUS.md) for the full design document, [docs/CERBERUS_0.8.0_PLAN.md](docs/CERBERUS_0.8.0_PLAN.md) for release doctrine, and [docs/quality-gates/](docs/quality-gates/) for per-milestone gate outcomes.

## What it does today

Run `CERBERUS` (no flags, or `/?` for help). On a DOS or DOSBox-X system it will probe the installed hardware, write a `CERBERUS.INI` file with the results, and print a short summary. Each reported value carries a confidence indicator: **HIGH** when the probe is authoritative, **MEDIUM** when inferred, **LOW** when emulated or guessed. If the tool cannot identify a chip, the unrecognized probe data is captured to `CERBERUS.UNK` and the summary suggests opening a GitHub issue with the capture attached.

The INI file carries two signatures:
- `signature` is a hash of the hardware-identity fields (CPU class, memory, video adapter, bus). Same machine, different run → same signature.
- `run_signature` is a hash of the full INI. Same machine, different run → different run_signature.

These support the counterfeit-CPU and remarked-chip detection scenario: two runs from the same hardware should produce the same `signature` but different `run_signature` values. Divergent behavior under an identical claimed identity is the fingerprint the dual-signature scheme is designed to expose.

## What does not work yet (v0.8.0)

### Deferred to 0.8.1 or 0.9.0

- **Runtime upload to `barelybooting.com`.** Compiled out of stock 0.8.0 per plan §8. Server not yet deployed; unreachable-endpoint code path had unfixed stack-overflow risk. `wmake UPLOAD=1` enables for development.
- **Whetstone numeric emit.** Kernel compiled in every build; stock dispatcher suppresses emit. Plan §7 trust-first decision: 30-50x per-unit-cost anomaly on real iron never root-caused. `bench_fpu.ops_per_sec` is the shipping FPU throughput metric. `wmake WHETSTONE=1` re-enables for calibration work. 0.9.0 direction: replace with per-instruction FADD/FMUL/FDIV/FSQRT/FSIN/FCOS microbenchmarks.
- **IEEE-754 edge-case FPU diagnostic** (M2.5, research gap L). Nine operand classes × five ops. DGROUP budget pressure at M2 exit (3 KB headroom); needs const-reclaim work before landing.
- **CSV output mode (`/CSV` flag).** M2.8 deferred. Format-string DGROUP cost + new code surface. Post-M3 revival.
- **L2 cache detection via 64/128/256 KB working-set sweep** (research gap B). Requires FAR buffer work beyond the current 32 KB cap.
- **Pointer-chase cache L1 latency in nanoseconds** (research gap A).
- **DRAM latency derivation** (research gap E).
- **Per-instruction FPU microbenchmarks** (0.9.0 replacement for Whetstone).
- **Menu bar, dropdown menus, modal dialog system.** Explicitly out of 0.8.0 scope per plan §9 CUA-lite decision. 0.9.0 candidate.
- **Dashboard-default landing screen** (CheckIt "launch with SysInfo populated" pattern). 0.9.0.
- **Cyrix DIR-based pre-CPUID discrimination.** Cyrix 486DLC currently tags as generic `486-no-cpuid`. Candidate for any 0.8.x point release if Cyrix hardware enters the capture corpus.
- **IIT 3C87 discrimination** via undefined-opcode probe. Target machine present (386 DX-40 + IIT 3C87) but the M2+ CheckIt-style IIT probe is not yet wired up.
- **Hercules-variant discrimination** (HGC / HGC+ / InColor via 3BAh bits 6:4). All Hercules cards currently tag as generic `ADAPTER_HERCULES`.
- **Genoa ET4000 video chipset** detected as generic `adapter=vga`. Video DB has Tseng ET4000; probe path misses the Genoa OEM BIOS signature. Investigation candidate.
- **Disk I/O benchmarks** (INT 13h sequential read throughput). Wide validation surface; 0.9.0+.

### Known real-iron issues (investigation ongoing)

- **W4/W6 BSS overwrite on BEK-V409 specifically.** Watcom `*** NULL assignment detected` fires on every exit on BEK-V409 (486 DX-2-66 + S3 Trio64 + Vibra 16S + AMI 11/11/92 + EMM386). On certain probe paths (OPL fallback) the stomp spills past the 32-byte `_NULL` guard and corrupts the BIOS date string in CONST. Two-machine capture matrix + DOSBox-X absence empirically narrow the bug to BEK-V409-specific hardware-probe paths (S3 Trio64 CR30 read, Vibra 16S DSP + OPL fallback, UMC491 PIT wrap-guard). Generic CERBERUS code paths ruled out. M2+ investigation when the 486 is back in service.
- **Intermittent OPL detection on Vibra 16 PnP** (issue #2, pre-existing). Same binary, same box, different boot produces `opl=opl3` vs `opl=none`.
- **timing_self_check cross-check marked `measurement_failed` on both 486 and 386.** The v0.7.1 UMC491 wrap-guard rejects the PIT/BIOS cross-check across hardware classes; may be too aggressive for original 8253 PIT.

## What the consistency engine checks today

CERBERUS's signature feature: it cross-checks detection claims against benchmark reality. Eleven rules currently in force, each emitting a `consistency.*` row in the INI with PASS, WARN, or FAIL:

1. **`486dx_fpu`**. 486DX CPUs must report an integrated FPU (catches counterfeit 486SX-as-DX silkscreens).
2. **`486sx_fpu`**. 486SX CPUs must NOT report an integrated FPU (catches detection confusion).
3. **`386sx_bus`**. 386SX systems must have an ISA-16 or better bus (386SX on ISA-8 is electrically impossible).
4a. **`timing_independence`**. PIT Channel 2 timing must agree with BIOS-tick (PIT Channel 0) within 15% over a fixed interval (catches `timing.c` math bugs and TSRs that drop INT 8).
4b. **`cpu_ipc_bench`**. `bench.cpu.int_iters_per_sec` must fall inside the CPU-DB empirical range for the detected family/model. Catches thermal throttle, cache-disabled-in-BIOS, TSR storms, overclock, and counterfeit remarks. Observed on the 486 DX-2 bench box as a clean 8.4M passing vs a CTCM-loaded 2.2M WARN. The v0.4 narration extension consults `diagnose.cache.status` and produces a three-way split (cache exonerated vs cache implicated vs ambiguous) instead of a single ambiguous message.
5. **`fpu_diag_bench`**. FPU diagnostic PASS and FPU benchmark result must agree. One head works, the other cannot exercise it, so WARN.
6. **`extmem_cpu`**. Reported extended memory implies the CPU is 286 or later (extended memory on 8088 is impossible).
7. **`audio_mixer_chip`**. Audio DB `mixer_chip` column agrees with the hardware mixer probe (CT1745 discriminator for the SB16 and Vibra 16S family). WARN when DB lacks data and the probe sees a mixer; FAIL on explicit mismatch.
9. **`8086_bus`**. 8086-class CPUs must be on an ISA-8 bus (PCI on an 8088 is impossible).
10. **`whetstone_fpu`**. `bench_whetstone` completion state agrees with `detect_fpu` report. Catches detect under-reporting of socketed FPUs: if Whetstone produced a number, x87 executed, so a "no FPU" detection is wrong.
11. **`dma_class_coherence`**. XT-class CPUs (8086, 8088, NEC V20, NEC V30) shipped only the master 8237 DMA controller. Cross-checks `cpu.class` against `diagnose.dma.ch5_status`. XT plus responsive slave = contradiction WARN. Catches misidentified CPUs reported as XT when the box is AT, plus frankenboard XT with retrofitted slave DMA.

Plus `thermal.cpu`, a Mann-Kendall rank trend test on per-pass benchmark timings. Monotonic upward drift at α=0.05 emits WARN (possible thermal throttling, dusty heatsink, or failing regulator). Tracked separately from the numbered consistency rules; lives in `thermal.c` and emits `thermal.*` rows.

Rule 8 (`cache_stride_vs_cpuid_leaf2`) is reserved pending further cache-bench work. The slot stays preserved even though Rules 10 and 11 now live above it, following the same off-by-one pattern as Rule 7.

Every rule documents what it catches AND what it structurally cannot. See [`docs/consistency-rules.md`](docs/consistency-rules.md).

## Target hardware

| Class | CPU | RAM | Display | Bus |
|---|---|---|---|---|
| Floor | 8088 / 8086 / V20 / V30 | 256KB | MDA | ISA 8-bit |
| Common | 286 / 386SX / 386DX | 640KB+ | CGA / Hercules / EGA | ISA 16-bit |
| Ceiling | 486SX / DX / DX2 / DX4, AMD Am5x86, Cyrix 5x86/6x86 | 4MB+ | VGA / SVGA | VLB / PCI |

8088 support is a hard requirement, not an aspiration. Anything that breaks it is out.

## Build

Requires [Open Watcom C/C++ 2.0](http://open-watcom.github.io/) and [NASM 2.x](https://www.nasm.us/). From the project root:

```
wmake
```

Produces `CERBERUS.EXE`, DOS real-mode, medium memory model. v0.8.0 stock build is ~167 KB; DGROUP (near data) ~61 KB (59.5 KB), ~4.5 KB under the 64 KB hardware ceiling. Run `wmake dgroup-report` to audit near-data usage.

### Build flavours

```
wmake                             # stock 0.8.0 shipping build
wmake WHETSTONE=1                 # +Whetstone emit (research/issue-#4)
wmake UPLOAD=1                    # +HTTP upload via HTGET (barelybooting.com)
wmake WHETSTONE=1 UPLOAD=1        # research build, both flags enabled
```

Stock builds: no Whetstone emit (the kernel stays compiled, the dispatcher suppresses), no HTTP transmission (entire HTGET / upload_execute code compiled out). Research builds re-enable both for development and calibration work. See `docs/CERBERUS_0.8.0_PLAN.md` sections 7 + 8 for the trust-first rationale.

Host-side unit tests (run on Windows, Linux, or macOS dev box; exercise the pure-math and database-lookup paths with 201 assertions across timing, consist, thermal, diag_fpu, diag_cache, diag_dma, bench_cache):

```
cd tests/host
wmake run
```

Target-side smoke tests go in `tests/target/` and run inside DOSBox-X or on real iron.

## Hardware-identification databases

The CPU, FPU, video-chipset, audio-chipset, and BIOS-family identification tables live as human-editable CSVs in [`hw_db/`](hw_db/). A Python build script regenerates the corresponding C source. Contributing a new hardware row looks like:

1. Run CERBERUS on the machine in question.
2. If the subsystem shows up as "unknown" in the summary, `CERBERUS.UNK` will have the raw probe data.
3. Open a GitHub issue using the "hardware submission" template and paste the capture.
4. Or, if you know enough to add the entry directly: edit the appropriate `hw_db/*.csv`, run `python hw_db/build_<subsystem>_db.py`, commit both files.

The database is the lowest-friction extension point in the codebase. No C knowledge required to grow it.

## Known issues

- **[#4](https://github.com/tonyuatkins-afk/CERBERUS/issues/4)**. Whetstone calibration still needs multi-cold-boot BEK-V409 validation. v0.5.0 landed a hand-coded x87-asm kernel (`bench_whet_fpu.asm`) which should close most of the gap vs the prior ~109 K-Whet; the numbers now emit at CONF_HIGH but real-hardware anchor-to-reference is pending.
- **[#6](https://github.com/tonyuatkins-afk/CERBERUS/issues/6)**. `bench_video` measures ISA-range bandwidth on VLB hardware. Known measurement-scope limitation.

Issues #1 (pre-existing test_timing failures), #2 (OPL detection intermittency), #3 (UI hang) and #5 (diag_cache threshold) closed prior to v0.5.0.

## Changelog

[`CHANGELOG.md`](CHANGELOG.md) covers everything since `v0.1.1-scaffold`.

## License

MIT. See [LICENSE](LICENSE).

## Author

Tony Atkins, [@tonyuatkins-afk](https://github.com/tonyuatkins-afk)
