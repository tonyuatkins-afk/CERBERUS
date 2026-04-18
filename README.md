# CERBERUS

DOS-native hardware detection, diagnostic, and benchmark tool for real-mode IBM PC / XT / AT and 486-class machines. Single EXE; typically 72–76 KB. Targets an 8088 with 256KB and an MDA card as the floor and scales up through a 486 with VGA.

Part of the Barely Booting / NetISA ecosystem — CERBERUS is the tool, NetISA is the hardware card for uploading results from DOS over TLS 1.3.

## Status

**Pre-alpha, approaching v0.2.0.** All five subsystems (detect, diagnose, benchmark, consistency engine, thermal tracker) are code-complete and adversarially quality-gated. Awaiting real-hardware validation on a 486 / 386 / 8088 matrix before the `v0.2.0` tag ships. Hardware identification has 121 seed entries across 5 databases; your specific chip is likely not yet present — `CERBERUS.UNK` captures get submitted as GitHub issues and added to the CSVs.

| Subsystem | Target version | Current state |
|---|---|---|
| Scaffold, timing, INI writer, dual signatures, crash breadcrumb | v0.1 | complete, tagged `v0.1.1-scaffold` |
| Emulator detection + confidence clamping | v0.2 | complete |
| Detection: CPU / FPU / memory / cache / bus / video / audio / BIOS | v0.2 | code-complete, awaiting real-hardware gate |
| Hardware identification databases (CPU 34 / FPU 14 / video 28 / audio 24 / BIOS 21) | v0.2 | 121 seed entries |
| Unknown-hardware submission path (`CERBERUS.UNK`) | v0.2 | complete |
| Summary UI with confidence meters + consistency alert box | v0.2 | complete (three-pane polish still pending) |
| Diagnostic tests (ALU, memory walking-1s/0s/AinA, FPU bit-exact, video RAM) | v0.3 | 4 of 6 complete (cache + DMA deferred per plan) |
| Benchmark suite (integer / FPU / memory, calibrated multi-pass for thermal) | v0.4 | 3 of 5 complete (cache + video bandwidth deferred) |
| Consistency engine (7 rules) + thermal stability (Mann-Kendall, α=0.05, N≥5) | v0.5 | quality-gated clean, 7 rounds adversarial review |
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

CERBERUS's signature feature: it cross-checks detection claims against benchmark reality. Seven rules currently in force, each emitting a `consistency.*` row in the INI with PASS / WARN / FAIL:

1. 486DX CPUs must report an integrated FPU (catches counterfeit 486SX-as-DX silkscreens)
2. 486SX CPUs must NOT report an integrated FPU (catches detection confusion)
3. 386SX systems must have an ISA-16 or better bus (386SX on ISA-8 is electrically impossible)
4a. PIT Channel 2 timing must agree with BIOS-tick (PIT Channel 0) within 15% over a fixed interval (catches `timing.c` math bugs and TSRs that drop INT 8)
5. FPU diagnostic PASS and FPU benchmark result must agree (one head works, the other can't exercise it = WARN)
6. Reported extended memory implies the CPU is 286 or later (extended memory on 8088 is impossible)
7. `thermal.cpu` — Mann-Kendall rank trend test on per-pass benchmark timings. Monotonic upward drift at α=0.05 ⇒ WARN (possible thermal throttling / dusty heatsink / failing regulator)
8. 8086-class CPUs must be on an ISA-8 bus (PCI on an 8088 is impossible)

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

Produces `CERBERUS.EXE` — DOS real-mode, medium memory model. Current build is 74,948 bytes; DGROUP (near data) 45,600 / 65,536 bytes (30% headroom on the hard ceiling).

Host-side unit tests (run on Windows / Linux / macOS dev box, exercise the pure-math and database-lookup paths with 98 assertions across timing / consistency / thermal):

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

## License

MIT. See [LICENSE](LICENSE).

## Author

Tony Atkins — [@tonyuatkins-afk](https://github.com/tonyuatkins-afk)
