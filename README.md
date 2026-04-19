# CERBERUS

DOS-native hardware detection, diagnostic, and benchmark tool for real-mode IBM PC / XT / AT and 486-class machines. Single EXE; current tip-of-tree is 80,044 bytes. Targets an 8088 with 256KB and an MDA card as the floor and scales up through a 486 with VGA.

Part of the Barely Booting / NetISA ecosystem — CERBERUS is the tool, NetISA is the hardware card for uploading results from DOS over TLS 1.3.

## Status

**Pre-alpha, approaching v0.2-rc1.** All five subsystems (detect, diagnose, benchmark, consistency engine, thermal tracker) are code-complete and adversarially quality-gated. The 486 DX-2 real-hardware gate passed on 2026-04-18 (five bugs found and fixed — see [CERBERUS.md § Why real hardware](CERBERUS.md) and [`tests/captures/486-real-2026-04-18/`](tests/captures/486-real-2026-04-18/)); 386 and 8088 validation still outstanding. Hardware identification has 121 seed entries across 5 databases; your specific chip is likely not yet present — `CERBERUS.UNK` captures get submitted as GitHub issues and added to the CSVs.

| Subsystem | Target version | Current state |
|---|---|---|
| Scaffold, timing, INI writer, dual signatures, crash breadcrumb | v0.1 | complete, tagged `v0.1.1-scaffold` |
| Emulator detection + confidence clamping | v0.2 | complete |
| Detection: CPU / FPU / memory / cache / bus / video / audio / BIOS | v0.2 | code-complete, 486 gate passed, 386 + 8088 gates pending |
| Hardware identification databases (CPU 34 / FPU 14 / video 28 / audio 31 / BIOS 21) | v0.2 | 128 seed entries |
| Audio DB `mixer_chip` column (CT1745 discriminator for SB16 family) | v0.2 | complete — 3 seeded rows, 28 `unknown` pending real-hardware |
| Unknown-hardware submission path (`CERBERUS.UNK`) | v0.2 | complete |
| Summary UI with confidence meters + consistency alert box | v0.2 | complete (three-pane polish + bar-graph comparison — see [checkit-comparison.md](docs/plans/checkit-comparison.md) — deferred behind UI-hang resolution) |
| Diagnostic tests (ALU, memory walking-1s/0s/AinA, FPU bit-exact, video RAM) | v0.3 | 4 of 6 complete (cache + DMA deferred per plan) |
| Benchmark suite (integer / FPU / memory, calibrated multi-pass for thermal) | v0.4 | 3 of 5 complete (cache + video bandwidth deferred; Dhrystone 2.1 + Whetstone adoption planned — see [checkit-comparison.md](docs/plans/checkit-comparison.md)) |
| Consistency engine (9 rules) + thermal stability (Mann-Kendall, α=0.05, N≥5) | v0.5 | quality-gated clean, 7 rounds adversarial review |
| Timing self-check (PIT C2 vs BIOS tick cross-check, rule 4a) | v0.5 | complete |
| NetISA upload | v0.6 | build-flag disabled by default |

See [CERBERUS.md](CERBERUS.md) for the full design document and [docs/plans/](docs/plans/) for the implementation plan.

## What it does today

Run `CERBERUS` (no flags, or `/?` for help). On a DOS or DOSBox-X system it will probe the installed hardware, write a `CERBERUS.INI` file with the results, and print a short summary. Each reported value carries a confidence indicator — **HIGH** when the probe is authoritative, **MEDIUM** when inferred, **LOW** when emulated or guessed. If the tool can't identify a chip, the unrecognized probe data is captured to `CERBERUS.UNK` and the summary suggests opening a GitHub issue with the capture attached.

The INI file carries two signatures:
- `signature` is a hash of the hardware-identity fields (CPU class, memory, video adapter, bus). Same machine, different run → same signature.
- `run_signature` is a hash of the full INI. Same machine, different run → different run_signature.

These are designed so that when CERBERUS starts producing benchmark numbers (v0.4), two runs from the same hardware can be compared for consistency even if the results diverge — the planned "counterfeit-CPU / remarked-chip" detection scenario.

## What it does NOT do yet

- No cache-coherence or DMA diagnostic — deferred until real-hardware gate surfaces failure modes worth codifying.
- No cache-bandwidth or video-bandwidth benchmarks — deferred; the first three benchmarks (integer / FPU / memory) cover the most-asked-about numbers.
- No NetISA upload — default build returns an "upload disabled" exit code; enable at v0.6 with `wmake NETISA=1` once the NetISA card firmware is far enough along.

## What the consistency engine checks today

CERBERUS's signature feature: it cross-checks detection claims against benchmark reality. Ten rules currently in force, each emitting a `consistency.*` row in the INI with PASS / WARN / FAIL:

1. **`486dx_fpu`** — 486DX CPUs must report an integrated FPU (catches counterfeit 486SX-as-DX silkscreens).
2. **`486sx_fpu`** — 486SX CPUs must NOT report an integrated FPU (catches detection confusion).
3. **`386sx_bus`** — 386SX systems must have an ISA-16 or better bus (386SX on ISA-8 is electrically impossible).
4a. **`timing_independence`** — PIT Channel 2 timing must agree with BIOS-tick (PIT Channel 0) within 15% over a fixed interval (catches `timing.c` math bugs and TSRs that drop INT 8).
4b. **`cpu_ipc_bench`** — `bench.cpu.int_iters_per_sec` must fall inside the CPU-DB empirical range for the detected family/model (catches thermal throttle, cache-disabled-in-BIOS, TSR storms, overclock, counterfeit remarks — observed on the 486 DX-2 bench box as a clean 8.4M passing vs CTCM-loaded 2.2M WARN).
5. **`fpu_diag_bench`** — FPU diagnostic PASS and FPU benchmark result must agree (one head works, the other can't exercise it = WARN).
6. **`extmem_cpu`** — reported extended memory implies the CPU is 286 or later (extended memory on 8088 is impossible).
7. **`audio_mixer_chip`** — audio DB `mixer_chip` column agrees with the hardware mixer probe (CT1745 discriminator for SB16 / Vibra 16S family; WARN when DB lacks data and probe sees a mixer, FAIL on explicit mismatch).
9. **`8086_bus`** — 8086-class CPUs must be on an ISA-8 bus (PCI on an 8088 is impossible).
10. **`whetstone_fpu`** — `bench_whetstone` completion state agrees with `detect_fpu` report (catches detect under-reporting of socketed FPUs — if Whetstone produced a number, x87 executed, and a "no FPU" detection is wrong).

Plus `thermal.cpu` — Mann-Kendall rank trend test on per-pass benchmark timings (monotonic upward drift at α=0.05 ⇒ WARN — possible thermal throttling / dusty heatsink / failing regulator). Tracked separately from the numbered consistency rules; lives in `thermal.c`, emits `thermal.*` rows.

Rule 8 (`cache_stride_vs_cpuid_leaf2`) is reserved pending Phase 3 cache-bench work (slot preserved even though Rule 10 now lives above it — same off-by-one pattern as Rule 7).

Every rule documents what it catches AND what it structurally cannot — see [`docs/consistency-rules.md`](docs/consistency-rules.md).

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

Produces `CERBERUS.EXE` — DOS real-mode, medium memory model. Current build is 80,044 bytes; DGROUP (near data) 48,688 / 65,536 bytes (26% headroom on the hard ceiling, under the 50,000-byte internal working-ceiling).

Host-side unit tests (run on Windows / Linux / macOS dev box, exercise the pure-math and database-lookup paths with 124 assertions across timing / consistency / thermal / diag_fpu):

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

- **[#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1)** — `test_timing` host test has 4 pre-existing failures after the PIT wrap-range rework (`b6c179b`, `6c3a023`). Test expectations drifted from behavior. Gated behind the Rule 4a UMC491 8254 phantom-wrap deep-dive, out of scope for v0.2-rc1. The other host suites (consist, thermal, diag_fpu) are clean.
- **[#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2)** — intermittent OPL detection on Vibra 16 PnP. Same binary + same box + different cold boot can produce `opl=opl3` vs `opl=none`. Partial fix in `eeba319` (BLASTER-base+8 primary, 0x388 fallback) made detection work *sometimes*; residual state-dependence remains. INI is still complete on the `opl=none` path — `audio.sb_present` and `sb_dsp_version` stay populated; the composite T-key lookup falls back to the raw `none:<dsp>` form.
- **UI hang on real iron** — observed once on the 486 DX-2 bench box on commit `7e4bdcb` (2026-04-18 afternoon): without `/NOUI`, after `ui_render_consistency_alerts` paints, the program did not return to DOS without a hard reboot. Reproduction regime is not active on the current 486 state — the 2026-04-18 evening session ran the baseline (`7da102e` tree, no instrumentation) and two builds with exit-path instrumentation; all three exited cleanly. State variable causing the drift is unidentified. Instrumentation patch preserved as a local `git stash` entry for re-application if the hang returns. Tracking issue to be filed alongside this README update with reopen criterion "reproduction on real iron." `/NOUI` retained as a user-visible escape hatch. Full investigation arc: [`docs/sessions/SESSION_REPORT_2026-04-18-evening.md`](docs/sessions/SESSION_REPORT_2026-04-18-evening.md).

## Changelog

[`CHANGELOG.md`](CHANGELOG.md) covers everything since `v0.1.1-scaffold`.

## License

MIT. See [LICENSE](LICENSE).

## Author

Tony Atkins — [@tonyuatkins-afk](https://github.com/tonyuatkins-afk)
