# CERBERUS

DOS-native hardware detection, diagnostic, and benchmark tool for real-mode IBM PC / XT / AT and 486-class machines. Single EXE; v0.7.0-rc1 is 163,922 bytes. Targets an 8088 with 256KB and an MDA card as the floor and scales up through a 486 with VGA.

Part of the Barely Booting / NetISA ecosystem. CERBERUS is the tool; [barelybooting-server](https://github.com/tonyuatkins-afk/barelybooting-server) is the companion web app that ingests uploaded CERBERUS.INI runs at `barelybooting.com/cerberus/`.

![CERBERUS v0.4.0 three-pane UI on a 486 DX-2](docs/releases/v0.4.0/screenshots/three-pane-ui-closeup.jpg)

## Status

**`v0.7.0-rc1` tagged 2026-04-20.** Release-candidate for the community-upload feature. Eleven tags across the arc: `v0.1.1-scaffold` → `v0.2-rc1` → `v0.3-rc1` → `v0.4-rc1` → **`v0.4.0`** → **`v0.5.0`** → **`v0.6.0`** → `v0.6.1` → `v0.6.2` → **`v0.7.0-rc1`**. The full `v0.7.0` tag is held until the companion server is deployed and an end-to-end upload round-trip validates on real hardware (see [`docs/ini-upload-contract.md`](docs/ini-upload-contract.md)).

**What's in each milestone:**
- **v0.4.0** (2026-04-19) — All six subsystems (detect, diagnose, benchmark, consistency engine, thermal tracker, UI) real-hardware-validated on BEK-V409.
- **v0.5.0** — Scrollable three-heads summary UI; Whetstone x87-asm kernel; Mandelbrot FPU visual demo.
- **v0.6.0** — Visual Diagnostics Journey: new tagline "Tough Times Demand Tough Tests"; directional head art (left / center / right); CPU ALU bit parade; FPU Lissajous; PIT metronome; journey framework with title cards + `/QUICK` flag.
- **v0.6.1** — Memory Cache Waterfall; Cache Latency Heat Map; OPL2 FM audio scale; result flashes wired.
- **v0.6.2** — Shared TUI helpers module; SB DSP direct-mode PCM audio (third audio path).
- **v0.7.0-rc1** — Community Upload (Part A): network transport detection, `[upload]` INI section, `/NOUPLOAD`/`/UPLOAD`/`/NICK`/`/NOTE` flags, HTGET shell-out for POST, UPLOAD STATUS summary section, INI format frozen at `ini_format=1`, server API contract documented at [`docs/ini-upload-contract.md`](docs/ini-upload-contract.md).

BEK-V409 (Intel i486DX-2-66, AMI BIOS 11/11/92, S3 Trio64, Vibra 16S, 63 MB XMS) is the validation workstation. 386 and 8088 validation are the next-platforms work. Hardware identification has 128 seed entries across 5 databases; your chip may not be present. `CERBERUS.UNK` captures get submitted as GitHub issues and added to the CSVs.

| Subsystem | Target version | Current state |
|---|---|---|
| Scaffold, timing, INI writer, dual signatures, crash breadcrumb | v0.1 | complete, tagged `v0.1.1-scaffold` |
| Emulator detection + confidence clamping | v0.2 | complete, tagged `v0.2-rc1` |
| Detection: CPU / FPU / memory / cache / bus / video / audio / BIOS | v0.2 | complete. 486 gate passed. 386 and 8088 pending. |
| Hardware identification databases (CPU 34 / FPU 14 / video 28 / audio 31 / BIOS 21) | v0.2 | 128 seed entries |
| Audio DB `mixer_chip` column (CT1745 discriminator for SB16 family) | v0.2 | complete |
| Unknown-hardware submission path (`CERBERUS.UNK`) | v0.2 | complete |
| Summary UI, three-pane direct-VRAM color renderer | v0.3 | complete; polished in v0.4.0 with CP437 corruption and V_U32 blank-row fixes |
| Diagnostic tests (ALU, memory walking-1s/0s/AinA, FPU bit-exact, video RAM, cache-stride, DMA-controller) | v0.3 | **6 of 6 complete**, tagged `v0.3-rc1` |
| Benchmark suite (integer, FPU, memory, cache, video, Dhrystone, Whetstone) | v0.4 | 5 of 5 benchmark modules live; Whetstone FPU-asm rework [#4](https://github.com/tonyuatkins-afk/CERBERUS/issues/4) deferred |
| Consistency engine (11 rules) and thermal stability (Mann-Kendall, α=0.05, N≥5) | v0.5 | quality-gated clean |
| Timing self-check (PIT C2 vs BIOS tick cross-check, rule 4a) | v0.5 | complete |
| Homage Phase 2 research lessons (7 tasks across CheckIt, CACHECHK, FASTVID, CHKCPU, TOPBENCH) | v0.4 | complete |
| NetISA upload | v0.6 | build-flag disabled by default |

See [CERBERUS.md](CERBERUS.md) for the full design document and [docs/plans/](docs/plans/) for the implementation plan.

## What it does today

Run `CERBERUS` (no flags, or `/?` for help). On a DOS or DOSBox-X system it will probe the installed hardware, write a `CERBERUS.INI` file with the results, and print a short summary. Each reported value carries a confidence indicator: **HIGH** when the probe is authoritative, **MEDIUM** when inferred, **LOW** when emulated or guessed. If the tool cannot identify a chip, the unrecognized probe data is captured to `CERBERUS.UNK` and the summary suggests opening a GitHub issue with the capture attached.

The INI file carries two signatures:
- `signature` is a hash of the hardware-identity fields (CPU class, memory, video adapter, bus). Same machine, different run → same signature.
- `run_signature` is a hash of the full INI. Same machine, different run → different run_signature.

These support the counterfeit-CPU and remarked-chip detection scenario: two runs from the same hardware should produce the same `signature` but different `run_signature` values. Divergent behavior under an identical claimed identity is the fingerprint the dual-signature scheme is designed to expose.

## What it does NOT do yet

- No NetISA upload. The default build returns an "upload disabled" exit code. Enable at v0.6 with `wmake NETISA=1` once the NetISA card firmware is far enough along.
- No cache write-back vs write-through reporting ([v0.5+ candidate](docs/research/homage/chkcpu-lessons.md)). Bench output makes the distinction visible through the small/large throughput comparison, but `detect.cache.l1_mode` is not yet a reported key.
- No Cyrix DIR-based pre-CPUID discrimination ([v0.5+ candidate](docs/research/homage/chkcpu-lessons.md)). A Cyrix 486DLC currently tags as generic `486-no-cpuid`.
- No automatic bar-graph comparison UI. Numbers emit as absolute rates; comparison against CheckIt or community baselines is manual today.

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

Produces `CERBERUS.EXE`, DOS real-mode, medium memory model. v0.7.0-rc1 builds to 163,922 bytes; DGROUP (near data) 59,008 bytes, well under the 65,536-byte hardware ceiling.

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
