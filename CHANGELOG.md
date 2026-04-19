# Changelog

All notable changes to CERBERUS. Format loosely based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); dates are ISO-8601, hash references are short-sha from `main`.

## [Unreleased — approaching v0.2-rc1]

### Detection

- Emulator / environment confidence-clamping path (`d8529d3`). All downstream detection rows carry an explicit confidence that gets clamped to MEDIUM when the environment is identified as an emulator, so a DOSBox-X run and a bench-box run look distinguishable in the INI.
- CPU class detection covering 8086 / 8088 / V20 / V30 / 286 / 386SX / 386DX / 486SX / 486DX / CPUID-capable (`cb9d588`), with a 34-entry CPU database (`0934486`) and CPUID vendor/family/model/stepping extraction.
- FPU detection (`872657c`) with a 14-entry database: integrated-486, 287, 387, RapidCAD, no-FPU, and the V20/V30 8087-socket path.
- Memory detection (`6b9b06c`): INT 12h conventional, INT 15h AH=88h / AX=E801h extended, XMS driver presence via INT 2Fh AX=4310h, EMS via INT 67h.
- Cache detection (`8fc9d9b`) — minimum-viable inference-from-CPU-class; real cache-stride detection deferred to Phase 3.
- Bus detection (`b145a57`) — PCI BIOS probe + ISA inference fallback, VLB "possible on 386DX+/486" heuristic.
- Video detection (`3b159e0`) with a 28-entry chipset database covering MDA / CGA / Hercules / EGA / VGA classes and S3 / Tseng / ATI / Trident / Cirrus / Paradise chipset identification.
- Audio detection (`80b5fec`) with a 24-entry database covering PC speaker / AdLib / OPL2 / OPL3 and the Sound Blaster DSP-version family (original through AWE64).
- BIOS info + family database (`e90a316`) — date-string scan, copyright extraction, PnP header detection.
- Unknown-hardware submission path (`18a5601`) — `CERBERUS.UNK` capture + end-of-run summary card inviting a GitHub issue with the probe data.
- Post-detection summary UI (`d6cd223`) — formatted text panel with CP437 confidence meters, minimum-viable for v0.2 (three-pane polish deferred).

#### Real-iron fixes from the 2026-04-18 bench-box gate (`eeba319`)

All five surfaced on contact with an actual BEK-V409 / i486DX-2 / S3 Trio64 / Vibra 16S box. None reproduce in DOSBox-X.

- **HIMEM.SYS intercepts INT 15h AX=E801h.** `detect_mem` now acquires the XMS entry point via INT 2Fh AX=4310h and calls AH=08h directly. `extended_kb=0` → `extended_kb=63076` on the 64 MB bench box.
- **S3 Trio64 option ROM carries "IBM VGA" string.** The substring scan matched the first hit. Added `probe_s3_chipid()` — unlocks S3 extended CRTC (write 0x48 to CR38) and reads CR30. `0xE1` → Trio64, `0xE6` → Virge. Runs ahead of the BIOS-string scan.
- **Vibra 16S DSP status port is base+0x0E, not base+0x0A.** Creative's DATA register lives at 0x0A, STATUS at 0x0E. Fix: poll the right port; DSP v4.13 now detected.
- **OPL 0x388 mirror disabled by CTCM on Vibra 16 PnP.** OPL3 undetected on a demonstrably-working card. Fix: probe BLASTER-base+8 first, fall back to 0x388. Partial: residual intermittency tracked in [#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2).
- **UMC491 8254 latch race produces biased phantom wraps.** PIT-C2 vs BIOS-tick cross-check reported 49% divergence every run. Fix: upper-bound the wrap count, require low-band→high-band shape for a valid wrap, and reject measurements at >25% post-hoc PIT/BIOS divergence (`measurement_failed` instead of biased data). Refined in `6c3a023` after the biased-misread pattern was identified.

#### Audio disambiguation (`7e4bdcb`)

- `audio.csv` composite match-key now tries `"<opl>:<dsp>:T<n>"` first (using the BLASTER T token), falls back to the bare `"<opl>:<dsp>"` on miss. Splits the DSP-4.13 family that was collapsing all onto a single wrongly-labeled AWE64 row. New disambiguated rows: `opl3:040D:T6` = SB16 / Vibra 16S, `opl3:040D:T8` = AWE32, `opl3:0410:T9` = AWE64 (CT4500). DSP-version table for 0x040B/0C/10/11 corrected against Creative's programmer's reference (AWE64 lives at 4.16/4.17, not 4.13).

#### Audio mixer-chip probe (`c22e886`)

- CT1745 mixer discriminator at BLASTER-base+4 / +5 reg 0x80 (Interrupt Setup). Returns `CT1745` / `none` / `unknown`. `audio.csv` grows a `mixer_chip` column. CT1745 seeded for SB16 CT2230 / CT2290 / Vibra 16S T6 family; all other 28 rows `unknown` pending real-hardware verification. Consumed by new consistency Rule 7.

### Diagnostics

- ALU + memory-pattern diagnostics (`a565a05`) — walking-1s / walking-0s / address-in-address patterns, flag-register correctness, stuck-bit detection.
- FPU correctness diagnostic (`cfd8ffd`) — known-answer bit-exact tests for add / sub / mul / div / compound expressions.
- Video-RAM diagnostic (`667d7a3`) — direct VRAM walk, plane consistency on EGA / VGA.
- Cache-coherence + DMA diagnostics deferred (`3ca0d7e`) — documented in plan, no stubs left in code.

**Status: 4 of 6 subsystems covered (ALU, memory, FPU, video). Cache + DMA deferred per plan.**

### Benchmarks

- CPU integer benchmark (`9b758ed`) — fixed instruction mix, PIT-C2-timed, MIPS-equivalent iters/sec output.
- Memory bandwidth benchmark (`b5ca6e0`) — REP STOSW (write), REP MOVSW (copy), REP LODSB (read — replaced the volatile-checksum approach post-real-iron in `eeba319` / `7e4bdcb`). `kb_per_sec` rewritten for microsecond-precision scaling after the 486 surfaced the sub-ms elapsed-time truncation that clamped every fast operation to the same bogus 4 MB/s.
- FPU benchmark (`6b67cc5`) — x87 instruction mix + DGROUP fix.
- Calibrated multi-pass mode for `bench_cpu` (`1774aa9`) — feeds thermal-stability tracker with per-pass timings.
- `total_ops=0` silent-display corruption (`b6c179b`), then systematic V_U32-display-buffer dangling-pointer class fix applied to all six bench_memory statics (`6c3a023`).

**Status: 3 of 5 subsystems covered (integer, memory, FPU). Cache-bandwidth + video-throughput deferred.**

### Consistency engine

Engine + first four rules (`fac1500`). Alert-box UI for WARN/FAIL renders (`7b1a9b0`). Rules 3 + 9 (`4a9f24e`). Methodology documentation (`bb760c8`). Thermal stability — Mann-Kendall α=0.05 (`d5e7400`). Rule 4a PIT/BIOS timing independence (`111347a`). Rule 4b `cpu_ipc_bench` (`7e4bdcb`). Rule 7 `audio_mixer_chip` (`c22e886`).

**Nine rules live** (1, 2, 3, 4a, 4b, 5, 6, 7, 9). Rule 8 (`cache_stride_vs_cpuid_leaf2`) reserved pending Phase 3 cache work.

### Infrastructure

- Crash-recovery breadcrumb (`crumb_enter` / `crumb_exit`) wired into WRAP macros for every detect / diag / bench probe (`ae1cfd9`), so a hang during a probe leaves a named trail that the next boot surfaces with a NOTICE + `/SKIP:<name>` suggestion.
- `/SKIP:TIMING` escape hatch for PIT-C2 probe on boards where the 8254 clone hangs on touching channel 2 (`5fdf7fa`), with the crumb-enter/exit pair so a mid-probe hang is surfaced on next boot.
- `unknown_finalize` reordered pre-UI (`7da102e`) so `CERBERUS.UNK` lands on disk even if the UI path hangs.
- `/NOUI` escape hatch (`7da102e`) — user-visible workaround for the UI-render hang observed on v5 real-iron runs. Retained documented as a debug flag, not a feature-level user workaround.
- Real-hardware run corpus archived (`98c07d5`) — six diffable INI captures at `tests/captures/486-real-2026-04-18/` with per-run narrative README, anchoring every real-iron fix in observable artifacts rather than assertions.
- "Why real hardware" section in CERBERUS.md (`0161e99`) — H2 under Status, names the five 2026-04-18 bugs with symptom / cause / commit, closes with non-negotiable real-hardware-gate statement.
- Adversarial quality gates applied at Phase 4 completion (`6686574`, 5 rounds) and post-real-iron (`4d28e8e`, round-2) catching the phantom-verify biased-baseline and sub-crumb lifecycle bugs before the bench-box validation.
- `tests/target/` scaffold (`0e6c7e3`) for Phase 1 real-hardware validation drops.
- Host-side test suite: 124 assertions across timing (54), consistency (30), thermal (15), diag_fpu (21). Test-expectation drift in timing tracked as [#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1).

### Documentation

- README rewritten in plain-status voice (`dee1c64`).
- README refreshed at Phase 4 completion (`2882388`); current refresh landing with this RC.
- `docs/consistency-rules.md` methodology (`bb760c8`).
- `docs/plans/2026-04-16-cerberus-end-to-end.md` — single-file end-to-end implementation plan covering v0.1 → v1.0 with phase-level architecture and quality-gate criteria.

### Known issues

- **[#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1)** — `test_timing` has 4 pre-existing failures after the PIT wrap-range rework in `b6c179b` / `6c3a023`. Test expectations drifted from behavior. Gated behind the Rule 4a UMC491 8254 phantom-wrap deep-dive, which is out-of-scope for v0.2-rc1. Other host suites (consist 30/0, thermal 15/0, diag_fpu 21/0) are clean.
- **[#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2)** — intermittent OPL detection on Vibra 16 PnP. Same binary, same box, different boot produces `opl=opl3` vs `opl=none`. Partial fix in `eeba319`; residual state-dependence remains. INI still complete on the `opl=none` path (`audio.sb_present=yes`, `sb_dsp_version=4.13`); downstream T-key lookup falls back to the raw composite key.
- **UI hang on real iron.** Observed once on v5 (`7e4bdcb`) without `/NOUI` (2026-04-18 afternoon): after `ui_render_consistency_alerts` paints, the program did not return to DOS. The 2026-04-18 evening session ran the baseline (`7da102e`, no instrumentation) and two builds with exit-path instrumentation; all three exited cleanly on the same 486 box. The reproduction regime is not active on the current state. State variable causing the drift is unidentified — candidates include CMOS drift, cold-vs-warm-boot residue, Vibra PnP init ordering. Instrumentation patch preserved as local `git stash` for re-application if reproduction returns. Tracking issue filed with reopen criterion "reproduction on real iron." `/NOUI` retained as a user-visible escape hatch. Full investigation arc: [`docs/sessions/SESSION_REPORT_2026-04-18-evening.md`](docs/sessions/SESSION_REPORT_2026-04-18-evening.md).

## [v0.1.1-scaffold] — 2026-03-20

Scaffold tag covering Phase 0: source-tree layout, Makefile, timing subsystem, CPU detection stub, display abstraction, INI reporter, BIOS info, and host-test infrastructure. First end-to-end CERBERUS.EXE that produces a valid (if mostly-stub) INI on DOSBox-X.
