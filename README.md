# CERBERUS

DOS-native hardware detection, diagnostic, and benchmark tool for real-mode IBM PC / XT / AT and 486-class machines. Single EXE under 64KB. Targets an 8088 with 256KB and an MDA card as the floor and scales up through a 486 with VGA.

Part of the Barely Booting / NetISA ecosystem — CERBERUS is the tool, NetISA is the hardware card for uploading results from DOS over TLS 1.3.

## Status

**Pre-alpha.** Detection code is in place but not yet validated on real hardware. Diagnostic and benchmark passes are stubbed. Do not treat current output as authoritative — identification is driven by a ~120-entry hardware database that will almost certainly miss your specific chip until someone submits it.

| Subsystem | Target version | Current state |
|---|---|---|
| Scaffold, timing, INI writer, dual signatures, crash breadcrumb | v0.1 | complete, tagged as `v0.1.1-scaffold` |
| Emulator detection + confidence clamping | v0.2 | complete |
| Detection: CPU / FPU / memory / cache / bus / video / audio / BIOS | v0.2 | code-complete, awaiting real-hardware gate |
| Hardware identification databases (CPU / FPU / video / audio / BIOS) | v0.2 | ~120 seed entries across 5 CSV files |
| Unknown-hardware submission path (`CERBERUS.UNK`) | v0.2 | complete |
| Summary UI with confidence meters | v0.2 | minimal text output; full three-pane box-draw pending |
| Diagnostic tests (ALU, memory integrity, FPU correctness, etc.) | v0.3 | stubbed |
| Benchmark suite (integer, FPU, memory bandwidth, cache, video) | v0.4 | stubbed |
| Consistency engine + thermal stability tracking | v0.5 | stubbed |
| NetISA upload | v0.6 | build-flag disabled by default |

See [CERBERUS.md](CERBERUS.md) for the full design document and [docs/plans/](docs/plans/) for the implementation plan.

## What it does today

Run `CERBERUS` (no flags, or `/?` for help). On a DOS or DOSBox-X system it will probe the installed hardware, write a `CERBERUS.INI` file with the results, and print a short summary. Each reported value carries a confidence indicator — **HIGH** when the probe is authoritative, **MEDIUM** when inferred, **LOW** when emulated or guessed. If the tool can't identify a chip, the unrecognized probe data is captured to `CERBERUS.UNK` and the summary suggests opening a GitHub issue with the capture attached.

The INI file carries two signatures:
- `signature` is a hash of the hardware-identity fields (CPU class, memory, video adapter, bus). Same machine, different run → same signature.
- `run_signature` is a hash of the full INI. Same machine, different run → different run_signature.

These are designed so that when CERBERUS starts producing benchmark numbers (v0.4), two runs from the same hardware can be compared for consistency even if the results diverge — the planned "counterfeit-CPU / remarked-chip" detection scenario.

## What it does NOT do yet

- No ALU, memory, or FPU correctness tests — that's v0.3.
- No benchmark numbers — that's v0.4.
- No consistency cross-checking between detection and benchmark — that's v0.5.
- No NetISA upload — default build returns an "upload disabled" exit code; enable at v0.6 with `wmake NETISA=1` once the NetISA card firmware is far enough along.

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

Produces `CERBERUS.EXE` — DOS real-mode, medium memory model, typically around 40–45KB.

Host-side unit tests (run on Windows / Linux / macOS dev box, exercise the pure-math and database-lookup paths with emulator-capped confidence and 80+ assertions):

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
