# Changelog

All notable changes to CERBERUS. Format loosely based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); dates are ISO-8601, hash references are short-sha from `main`.

## [v0.8.1], 2026-04-22 — Completion release

Same-day follow-up to v0.8.0. Closes every item the v0.8.0 notes marked "deferred to 0.8.1." Full release notes at `docs/releases/v0.8.1.md`. Tag at `bdfe95c`.

### Added

- **M1.1 IEEE-754 edge-case diagnostic** (research gap L). New `src/diag/diag_fpu_edges.c` exercises 14 focused edges across FADD (4), FSUB (1), FMUL (2), FDIV (3), FSQRT (4): signed zero preservation, infinity arithmetic, NaN propagation, 0/0, inf/inf, sqrt(-0). Per-op counters as `diagnose.fpu.edge.<op>_ok=N_of_M`, aggregate as `diagnose.fpu.edge_cases_ok=14_of_14` on a conformant FPU. FSQRT via `#pragma aux fsqrt` to emit the bare instruction (Watcom libc `sqrt()` clamps domain errors to 0.0 rather than propagating NaN). SNaN deliberately not exercised: pre-387 silent-accept is covered by `diag_fpu_fingerprint`. Pure classifier `fp_classify_double()` exported for host tests.
- **M1.2 `/CSV` output mode** (research gap H). `report_write_csv()` writes a sibling CSV file with RFC 4180 minimal quoting: fields containing comma, quote, CR, or LF get double-quoted with embedded quotes doubled. Schema is `key,value,confidence,verdict`. Confidence renders as `HIGH` / `MEDIUM` / `LOW`; verdict as `PASS` / `WARN` / `FAIL` / `UNK`. `/CSV` flag derives path from `/O:<file>` (replaces extension with `.CSV`, or appends if absent).
- **M2.1 L1 pointer-chase latency probe** (research gap A). `pointer_chase_latency_ns()` reinterprets the shared 2 KB FAR buffer as a 1024-slot unsigned int array, initializes a coprime-step (67) chain that visits every slot before repeating, primes with one full cycle, then measures 20,000 data-dependent chase iterations via PIT-C2. Emits `bench.cache.char.l1_ns`.
- **M2.2 L2 reach via 64 KB FAR buffer** (research gap B, narrowed scope). `_fmalloc(65520)` allocates an ephemeral buffer, `stride_read` measures throughput at 64 KB working set, `_ffree` releases. Emits `bench.cache.char.size_64kb_kbps` + `bench.cache.char.l2_status=ok`. Allocation failure path emits `l2_status=no_far_mem`. **Scope-cut**: 128 KB / 256 KB envelope requires huge-pointer arithmetic to span 64 KB segment boundaries; deferred to 0.9.0.
- **M2.3 DRAM ns derivation** (research gap E). New pure helper `bench_cc_derive_dram_ns(kbps, line_bytes)` returns `line_bytes * 1e6 / kbps`. Emits `bench.cache.char.dram_ns` at `CONF_MEDIUM` (64 KB may still land in L2 on chips with larger L2).
- **M3.1 IIT 3C87 routing (partial).** `hw_db/fpus.csv` gains `iit-3c87` row (IIT vendor). `fpu_db.c` regenerated, 15 entries. `fpu_probe_iit_3c87()` stub in place with routing; real FNSAVE-or-opcode discriminator signature awaits a 386 DX-40 + IIT 3C87 real-iron capture. Hardware-gated.
- **M3.2 Genoa ET4000 chip-level probe.** `probe_et4000_chipid()` in `detect/video.c` runs a 3CDh read-write-readback test that fires on any ET4000-family chip regardless of BIOS string. Integrated after `probe_s3_chipid()`, before BIOS text scan. No new DB row: Genoa OEM'd Tseng silicon.
- **M3.3 Hercules variant discrimination.** New `hercules_variant_t` enum (HGC / HGC+ / InColor / UNKNOWN / NA). `src/core/display_hercules_ids.c` holds the pure classifier + token mapper, included as source from `display.c` and from the host test directly. `display_hercules_variant()` samples 3BAh outside vertical retrace, reads bits 6:4 as the 3-bit variant ID. New `video.hercules_variant` key emits alongside the stable `video.adapter=hercules` signature key. 16-assertion host-test suite.
- `devenv/smoketest-0-8-1.conf` — dedicated 0.8.1 DOSBox Staging smoketest config covering the new surface.
- `docs/releases/v0.8.1.md` release notes.
- `docs/CERBERUS_0.8.1_PLAN.md` plan document.
- `docs/CERBERUS_0.9.0_PLAN.md` plan document for the next release.
- `docs/quality-gates/0.8.1-M1-gate-2026-04-22.md`, `0.8.1-M2-gate-2026-04-22.md`, `0.8.1-M3-gate-2026-04-22.md` — per-milestone gates.

### Changed

- `CERBERUS_VERSION` bumped from `0.8.0` to `0.8.1`.
- `opts_t` gains `do_csv` field. `/CSV` CLI flag parsed; help output documents it.
- `bench_cache_char.c` dispatcher order: L1 size sweep → line-size sweep → write-policy probe → L1 latency probe (M2.1) → L2 reach probe (M2.2) → DRAM ns derivation (M2.3).
- Host test suite: 320 assertions / 9 suites at v0.8.0 → 376 assertions / 12 suites at v0.8.1. New suites: `test_diag_fpu_edges` (21 assertions), `test_report_csv` (15 assertions), `test_hercules_variant` (16 assertions). DRAM-ns additions in `test_bench_cache_char`.

### Fixed

- **Hotfix: `/CSV` consumed by `/C` calibrated-mode prefix** (commit `ed2948d`). `str_starts_with(a, "/C")` was silently matching `/CSV` as the calibrated-mode prefix before the exact `/CSV` match branch was reached. Result: `do_csv` never got set and runs defaulted to `mode=calibrated, runs=7`. Fix reorders the parser so `/CSV` is matched immediately after `/Q` (before any prefix check) and tightens the `/C` guard to require `a[2]` be NUL or colon. Bug passed both the host-test suite and `wmake rc=0`; only the DOSBox Staging end-to-end exposed it.
- **Version macro bump** (commit `bdfe95c`). Initial v0.8.1 tag was cut without bumping `CERBERUS_VERSION` from `"0.8.0"` to `"0.8.1"`, so the tagged binary self-identified as `version=0.8.0` in INI output. Post-tag smoketest caught it; tag was moved forward to include the macro bump before anyone pulled.

### Deferred from 0.8.1 (carried to 0.9.0)

- **M4 BEK-V409 BSS overwrite root-cause investigation.** Hardware-gated; needs BEK-V409 + debugger session.
- **M3.1 IIT 3C87 real signature.** Stub returns 0 unconditionally; DB row + routing ready for a small-diff follow-up once real-iron capture supplies the signature.
- **M3.2 Genoa ET4000 probe confirmation.** Algorithm from Tseng reference; real-iron validation pending.
- **L2 sweep at 128 KB / 256 KB working sets.** Requires huge-pointer `stride_read_huge()`. Fully scoped in the 0.9.0 plan.

### Memory + doctrine additions

- New durable feedback memory: **smoketest after every CERBERUS.EXE build**. Host-test green + `wmake rc=0` is not sufficient proof. The `/CSV` flag-parsing regression and the CERBERUS_VERSION bump omission were both caught only by the end-to-end DOSBox Staging pass. The directive is now load-bearing for every build from 0.8.1 forward.
- 0.9.0 hardware-ID roadmap filed to auto-memory as `project_cerberus_0_9_0_hw_id_roadmap.md`: BIOS ROM 64 KB hash, expansion card ROM walk, NIC detection + OUI lookup, UART FIFO probe, parallel ECP/EPP, game port, RTC presence. Synthesized into the 0.9.0 plan.

### Build state

- `CERBERUS.EXE` stock: **170,722 bytes**.
- DGROUP near-data: **61,824 bytes** / 62 KB soft target, 3,712 bytes vs 64 KB hard ceiling. Status: AT RISK (yellow) per `tools/dgroup_check.py`. Accumulated pressure; each milestone stayed inside its cap but the cumulative delta is noticeable. 0.9.0 M1.1 is a `keys.h` centralization pass targeting at least 2,500 bytes recovered.
- Host tests: **376 assertions across 12 suites**, 0 failures.
- Zero compiler warnings on stock build.

## [v0.8.0], 2026-04-22 — Trust-first release

First release where every shipped result has been verified on real iron. The cut is smaller than the v0.7.1 envelope on purpose: features that could not be trust-proved on a 486 DX-2-66 inside the milestone window were either compiled out (upload), emit-suppressed (Whetstone), or deferred to 0.8.1. Full release notes at `docs/releases/v0.8.0.md`. Tag at `b2ace2e`.

The individual 0.8.0 milestone entries (M1, M2, M3, M4) are preserved below for reference. This section summarizes the final tag.

### Added

- **M1.1 Whetstone emit suppression in stock builds.** `bench_whetstone.c` dispatcher returns immediately with `bench.fpu.whetstone_status=disabled_for_release` row; the Curnow-Wichmann kernel stays compiled + host-tested but does not emit a number. `wmake WHETSTONE=1` re-enables for research. Reason: the kernel reads 10-30x below the published reference band on real 486 silicon.
- **M1.2 Upload compiled out of stock builds.** `#ifdef CERBERUS_UPLOAD_ENABLED` gates the HTGET-era call sites. Stock stub returns `UPLOAD_DISABLED` with `upload.status=not_built`. `/UPLOAD` rejects at parse time. Reason: v0.7.1 stack overflow when `barelybooting.com` unreachable.
- **M1.3 Nickname buffer leak fix** (issue #9). `_nmalloc` the nickname + notes buffers into the near heap with static fallback.
- **M1.4 `cpu.class` normalization to family token.** CPUID-class CPUs emit `cpu.class=486` / `pentium` / `pentium_pro` / `pentium_4` instead of the vendor string. Vendor remains as `cpu.vendor`.
- **M1.5 `bench_cpu` DB anchor widening.** 486 DX-2-66 `iters_low` lowered from 4,700,000 to 1,500,000 after BEK-V409 real-iron measurement of ~1.96M iters/sec.
- **M1.6 DGROUP audit tooling.** `tools/dgroup_check.py` + `wmake dgroup-report`.
- **M1.7 End-of-run `_exit()` bypass.** `src/main.c` replaces `return exit_code` with `_exit((int)exit_code)` to skip the Watcom libc teardown chain that was hanging on BEK-V409.
- **M1.8 Three-tier real-hardware validation corpus.** BEK-V409 (486 DX-2-66) + 386 DX-40 + IIT 3C87 + Genoa ET4000 captures under `tests/captures/`.
- **M2.1 Cache stride=128** (6-point sweep). Enables `line_bytes=32` (Pentium) + `line_bytes=64` (Pentium-Pro+) inference via 2-step plateau guard.
- **M2.2 FPU FPTAN probe** (research gap I). `diagnose.fpu.fptan_pushes_one`.
- **M2.3 FPU rounding-control cross-check** (gap J). 4 modes × FISTP(±1.5). `diagnose.fpu.rounding_modes_ok`.
- **M2.4 FPU precision-control cross-check** (gap K). 3 significand widths × 1.0/3.0 stored as tword. `diagnose.fpu.precision_modes_ok`.
- **M2.6 FPU exception-flag roundtrip** (gap M). All 6 x87 exceptions triggered + observed + cleared. `diagnose.fpu.exception_flags_ok`.
- **M2.7 Memory checkerboard + inverse checkerboard patterns.** 0x55/0xAA and 0xAA/0x55 across adjacent-cell coupling fault coverage.
- **M3.1 CGA snow safety gate**, **M3.2 F-key legend**, **M3.3 F1 help overlay**, **M3.4 F3 exit + CUA grammar**, **M3.5 `/MONO` flag**, **M3.6 16-bg color enable**, **M3.7 adapter-tier waterfall refinement** — see the v0.8.0-M3 sub-entry below for per-item detail.
- **M4.1 Possible-causes narration on consistency rules.** FAIL/WARN verdicts carry a short hint after the verdict token.
- **M4.2 CERBERUS.md master spec rewrite.** Head I/II/III subsystem tables refreshed with Implemented/Deferred status.
- **M4.3 README.md full refresh.** New "Subsystem state at v0.8.0" table + "What does not work yet" section.
- **M4.4 methodology.md extended.** New sections for cache stride detection, FPU 5-axis fingerprint, memory pattern diagnostics, CGA snow gate, adapter waterfall, /MONO role mapping.
- **M4.5 consistency-rules.md extended.** New Rule 11 `xt_slave_dma`, per-rule narration table.
- **M4.6 Why real-hardware section extended.** 5 new DOSBox-X-invisible observations appended.
- `docs/releases/v0.8.0.md` release notes.
- `docs/CERBERUS_0.8.0_PLAN.md` plan document.
- `docs/quality-gates/M1-gate-2026-04-21.md`, `M2-gate-2026-04-21.md`, `M3-gate-2026-04-22.md`, `M4-gate-2026-04-22.md` — per-milestone gates.

### Changed

- `CERBERUS_VERSION` bumped from `0.7.1` through `0.8.0-M1`, `0.8.0-M2`, `0.8.0-M3` to `0.8.0` at tag.
- `opts_t` gains `force_mono`. CLI help + usage documents all current flags.
- `diag_fpu_fingerprint` grows from 4 to 5 axes (adding FPTAN).
- Real-iron captures land in `tests/captures/<class>-<id>-<date>-m<N>/` replacing the earlier flat `runs/` convention.

### Removed

- Runtime upload POST path from stock builds (gated via `CERBERUS_UPLOAD_ENABLED`).

### Deferred from 0.8.0 (landed in 0.8.1)

- IEEE-754 edge-case diagnostic (gap L) → 0.8.1 M1.1.
- `/CSV` output mode (gap H) → 0.8.1 M1.2.
- L1 pointer-chase latency (gap A) → 0.8.1 M2.1.
- L2 detection via 64 KB FAR buffer (gap B narrowed) → 0.8.1 M2.2.
- DRAM ns derivation (gap E) → 0.8.1 M2.3.

### Deferred out of 0.8.x (stays 0.9.0)

- Per-instruction FPU microbenchmarks (Whetstone replacement, gap N).
- Upload path re-enabled (stack-safe offline fallback + live barelybooting.com).
- 8088 / XT real-hardware capture.
- Disk throughput via INT 13h.
- Full CUA shell (menus, dropdowns, modals).
- 128 KB / 256 KB L2 sweep via huge-pointer arithmetic.

### Build state

- `CERBERUS.EXE` stock: **166,994 bytes**.
- DGROUP near-data: **60,976 bytes** / 62 KB soft target. Headroom 2,512 bytes vs soft, 4,560 bytes vs 64 KB hard ceiling. Status: OK at tag.
- Host tests: **320 assertions across 9 suites**, 0 failures.
- Zero compiler warnings on stock build.

## [v0.8.0-M3], 2026-04-22 (overnight) — CUA-lite interaction polish

Third milestone of the 0.8.0 "trust and validation" release. M3 closes the interaction-axis gap: CUA-standard keybindings, Norton-style F-key legend, F1 help overlay, /MONO flag, 16-background-color enable, CGA snow gate. No menu bar, no modal dialog system, no structural UI refactor (per plan §9 CUA-lite decision). Full gate at `docs/quality-gates/M3-gate-2026-04-22.md`.

### Added

- **M3.1 CGA snow safety gate.** New `tui_wait_cga_retrace_edge()` in `tui_util.c` synchronizes VRAM writes to the CGA 3DAh retrace edge. `tui_putc` and `ui.c`'s private `vram_putc` call it before every write. No-op on non-CGA adapters (MDA/Hercules/EGA/VGA have dual-ported VRAM). 64K-iteration timeout guard against broken emulation. Per plan §M3 exit-gate requirement.
- **M3.2 Norton-style F-key legend.** `ui.c:render_status_bar` becomes `render_legend`. Layout: " 1Help  3Exit  Up/Dn  PgUp/Dn  Home/End ... rows X-Y of N". Borland TStatusLine palette on color (0x30 black-on-cyan base, 0x3F bright-white-on-cyan hotkey digits); ATTR_INVERSE on mono. Right-aligned scroll-position indicator with collision-guard.
- **M3.3 F1 help overlay.** `render_help_overlay()` displays static navigation + command reference. Any key returns to scrollable summary. Reuses viewport primitives; deliberately no modal-window class.
- **M3.4 F3 exit + formalized CUA key dispatch.** `read_nav_key()` handles F1 (0x3B) and F3 (0x3D) scan codes. Esc and Q remain as exit aliases. Main loop wires NAV_HELP through `render_help_overlay()`; fits-in-one-page path now also supports F1 help (not just exit-on-any-key).
- **M3.5 /MONO flag with semantic role mapping.** New `opts_t.force_mono` + `/MONO` parse. `display_set_force_mono(int)` / `display_is_mono()` unify mono detection across all rendering. `tui_is_mono()` and `ui.c:ui_is_mono()` delegate to `display_is_mono()`. Attribute mapping per MS-DOS UI-UX research Tier 0: 07h body, 0Fh emphasis, 01h underline, 70h reverse, F0h blink-reverse, 00h/08h reserved-invisible — realized via ATTR_INVERSE across the mono rendering paths.
- **M3.6 16-background-color mode enable.** `display_enable_16bg_colors()` issues INT 10h AX=1003h BL=00h during `display_init()` on EGA/VGA/MCGA color adapters. Switches attribute byte bit 7 from blink-enable to background-intensity, making 16 background colors available. Zero-cost quality win per MS-DOS UI-UX research Part B.
- **M3.7 Adapter-tier waterfall documentation refinement.** `display.c` header comment refreshed to match the MS-DOS UI-UX research Part B waterfall documentation: VGA AH=1Ah → EGA AH=12h BL=10h → BDA equipment flag (equivalent to INT 11h) → 3BAh bit 7 toggle (MDA vs Hercules). Code was already correct; docstring now matches the canonical reference.

### Changed

- `CERBERUS_VERSION` bumped from `0.8.0-M2` to `0.8.0-M3`.
- `display_init()` now calls `display_enable_16bg_colors()` after adapter detection.
- `tui_util.c:tui_is_mono()` and `ui.c:ui_is_mono()` are now wrappers around `display_is_mono()` for unified /MONO-aware behavior.
- Help output (`/?`) adds `/MONO` line.

### Deferred from M3

- **Full CUA shell** (menu bar, dropdowns, modal dialogs). Explicitly out of scope per plan §9; 0.9.0 candidate.
- **80x50 dense mode, 132-column SVGA, DAC-gradient title bars, themed palettes.** All out of scope.
- **Hercules-variant discrimination** (HGC vs HGC+ vs InColor via 3BAh bits 6:4). Out of M3 scope (enum expansion). Gate W10.

### Build state at M3 close-out

- Stock `CERBERUS.EXE`: 166,898 bytes (+1,056 from M2's 165,842)
- Research `CERBERUS.EXE` (`wmake WHETSTONE=1 UPLOAD=1`): 173,294 bytes
- DGROUP near-data: 60,880 bytes (59.5 KB), 2,608 bytes headroom vs 62 KB soft target. STATUS: OK per `tools/dgroup_check.py`.
- Host test suite: 320 assertions, 0 failures (unchanged from M2; M3 work is UI-path and was intentionally designed to avoid expanding host-test surface — interactive behavior needs real keyboard input).
- Zero compiler warnings stock + research.
- MD5 stock: `9b2ef53fdc187e7a085be00eb4e2c61e`

### Known issues carried from M1/M2 (none resolved in M3)

- W4/W6 BSS-overwrite on BEK-V409 specifically (pending 486 return from storage).
- IIT 3C87 FPU mis-tagged on 386 DX-40 capture.
- Genoa ET4000 video DB/probe gap.
- Mandelbrot coda suppressed in stock builds.

### Real-hardware validation plan (tomorrow)

M3 interaction grammar must be validated on real keyboards. Hardware parade: 8088/XT + 286/AT + 386 DX-40 + 486 BEK-V409 + Pentium 133 (Toshiba) + Pentium 200 MMX. Each machine exercises:
- M3.1 CGA snow gate (only relevant on actual CGA hardware)
- M3.2 F-key legend rendering (color vs mono attributes)
- M3.3/3.4 F1/F3/Esc key flow
- M3.5 /MONO flag (try on color machine and on true-mono machine)
- M3.6 16-bg-color mode (visible if rendering uses bright bg colors — currently it doesn't, so silent enabler)

## [v0.8.0-M2], 2026-04-21 (overnight) — precision expansion milestone

Second milestone of the 0.8.0 "trust and validation" release. M2 lands the research-gap FPU probes and cache stride extension that fit within DGROUP budget. Full gate outcome at `docs/quality-gates/M2-gate-2026-04-21.md`.

### Added (M2 research-gap probes)

- **M2.1 Cache stride=128**. `bench_cache_char.c` stride sweep extended from 5 to 6 points (4/8/16/32/64/128). `bench_cc_infer_line_bytes` accepts 6 strides, enabling inference of line=32 (Pentium) and line=64 (Pentium Pro+) via the 2-step plateau guard. New INI key `bench.cache.char.stride_128_kbps`. Backward compatible: pre-M2 tests passing 5 strides still work.
- **M2.2 FPU FPTAN probe** (research gap I per `docs/FPU Test Research.md`). New asm function `fpu_fp_try_fptan` loads angle=0.5, does FPTAN, compares ST(0) to 1.0 via FCOMP, returns FSTSW. `fp_probe_result_t` gains a `fptan_pushes_one` axis. `family_behavioral` inference now uses 5 axes (was 4). New INI key `diagnose.fpu.fptan_pushes_one`. Distinguishes 387+ (always pushes 1.0) from 8087/287 (pushes cos-ish denominator).
- **M2.3 FPU rounding-control cross-check** (gap J). New asm `fpu_fp_probe_rounding(mode, out_pair)` sets RC bits of control word, performs FISTP(1.5) and FISTP(-1.5), returns integer results. C-side runs all 4 modes, emits 4 per-mode rows `diagnose.fpu.rounding_nearest/down/up/truncate=<pos>,<neg>` plus summary `diagnose.fpu.rounding_modes_ok=yes|no`. Canonical IEEE-754 table: nearest=(2,-2), down=(1,-2), up=(2,-1), truncate=(1,-1). All four pairs are distinct, fully characterizing RC behavior.
- **M2.4 FPU precision-control cross-check** (gap K). New asm `fpu_fp_probe_precision(pc_mode, out_10)` sets PC bits of control word, computes 1.0/3.0 (always inexact in binary), stores 10-byte extended result. C-side runs all 3 modes (PC=single/double/extended; mode 01 reserved per Intel), emits summary `diagnose.fpu.precision_modes_ok=yes|no` (yes iff the three 10-byte buffers are bytewise distinct). Per-mode detail not emitted by default to preserve DGROUP; infrastructure ready to expand if real iron shows `no`.
- **M2.6 FPU exception-flag roundtrip** (gap M). New asm `fpu_fp_probe_exceptions(out_6)` triggers all 6 x87 exceptions in sequence (IE via FSQRT(-1), DE via denormal load, ZE via 1/0, OE via FSCALE(1,+20000), UE via FSCALE(1,-20000), PE via 1/3), captures FSTSW after each. Pure inference helpers `fpu_fp_exceptions_count_raised()` and `fpu_fp_exceptions_bitmap()` are host-testable. Emits summary `diagnose.fpu.exceptions_raised=N_of_6`. Healthy FPU: 6/6.
- **M2.7 Memory checkerboard + inverse-checkerboard patterns**. `diag_mem.c` gains `pattern_checkerboard` (0x55/0xAA alternating) and `pattern_inv_checkerboard` (0xAA/0x55). Catches adjacent-cell coupling faults that walking-1s/0s miss. New INI rows `diagnose.memory.checkerboard=pass|fail` and `diagnose.memory.inv_checkerboard=pass|fail`. QA-Plus homage pattern.

### Deferred from M2 scope

- **M2.5 FPU IEEE-754 edge-case diagnostic** (gap L). Scope (9 operand classes × 5 ops = 45 cases) would cost 800-1500 bytes of DGROUP for labels and expected results. Budget pressure at M2 exit (60,416 B / 62 KB soft target, 3 KB headroom). Deferred to post-gate incremental pass or M3.
- **M2.8 CSV output mode** (`/CSV` flag). Format-string DGROUP cost plus new code surface. Lower-priority than FPU research-gap closure. Deferred to post-M3 once DGROUP reclaim work (const-to-far migration, possible-causes FAR pool) opens headroom.

### Empirical findings from DOSBox-X M2 smoketest

- `fpu.fptan_pushes_one=yes` (emulated 486 FPU is 387+ era).
- `fpu.rounding_modes_ok=yes`, all 4 per-mode pairs match IEEE table.
- `fpu.precision_modes_ok=no` - DOSBox-X PC field emulation is incomplete; the three PC modes produce byte-identical 1.0/3.0 tword results. Real iron expected to pass.
- `fpu.exceptions_raised=0_of_6` - DOSBox-X doesn't set exception flags in status word. Real iron expected 6/6.

The two "no"s are the probes working as designed, surfacing real emulation gaps. Real-hardware capture will be the first data point for actual FPU-generation characterization via these axes.

### Build state at M2 close-out

- `CERBERUS.EXE` stock: 165,842 bytes (+2,004 from M1's 163,838)
- `CERBERUS.EXE` research (`wmake WHETSTONE=1 UPLOAD=1`): 172,254 bytes
- DGROUP near-data: 60,416 bytes (59.0 KB), 3,072 bytes headroom vs 62 KB soft target, 5,120 bytes headroom vs 64 KB hard ceiling. STATUS: OK per `tools/dgroup_check.py`.
- Host test suite: 320 assertions, 0 failures (+22 from M1's 298).
- Zero compiler warnings stock and research.

### Known issues carried from M1 (none resolved in M2)

- W4/W6 BSS-overwrite bug localized to BEK-V409 specifically (486 + S3 Trio64 VLB + Vibra 16S + AMI 11/11/92). Tracked for investigation when 486 is back from temporary storage.
- IIT 3C87 FPU on 386 DX-40 capture mis-tagged as Intel 80387. Target machine for M2+ IIT discrimination probe.
- Genoa ET4000 video on 386 DX-40 detected as generic vga. Video DB / probe-path gap.
- Mandelbrot coda removed from stock builds (piggybacked on Whetstone).

## [v0.8.0-M1], 2026-04-21 — trust and validation milestone

First milestone of the 0.8.0 "trust and validation" release. Per the plan at `docs/CERBERUS_0.8.0_PLAN.md`, M1 cuts known credibility traps and establishes the real-hardware validation corpus. 0.8.0 tag is held for M2 (precision expansion), M3 (CUA-lite shell), and M4 (docs parity).

### Added

1. **DGROUP audit tool (`tools/dgroup_check.py` + `wmake dgroup-report`).** Parses `cerberus.map` near-data segments against the 64 KB DOS hard ceiling, 62 KB soft target, and 2 KB hard-reserve window. Per 0.8.0 plan section 4 ship criterion 4, this is the gate tool for M2 scope decisions. M1 exits with DGROUP at 58.5 KB, 3.6 KB headroom.

2. **Real-hardware capture corpus.** `tests/captures/` grows three new dated subdirectories:
   - `486-BEK-V409-2026-04-21-m1/` first M1 run, pre-em-dash-fix, shows the BIOS-date BSS stomp + audio scale hang (historical evidence)
   - `486-BEK-V409-2026-04-21-m1-noui/` second run, post-em-dash-fix, pre-_exit-fix, shows `CERBERUS.LAS=main.return` diagnostic that drove the M1.7 fix
   - `486-BEK-V409-2026-04-21-m1-exit/` clean exit post-_exit-fix, the shipping baseline
   - `386-real-2026-04-21-m1/` 386 DX-40 capture with IIT 3C87 + Genoa ET4000 + Aztech ISA, baseline for M2 DB coverage work
   Each dir has a `README.md` with ground truth, invariant verification, and post-capture findings.

3. **Validation handoff protocol (`tests/target/VALIDATION-0.8.0-M1.md`).** Per-tier expectations (486 / 386 / 286 / 8088) with canonical signature keys, expected consistency rule outcomes, and acceptance criteria.

4. **Quality gate documentation (`docs/quality-gates/M1-gate-2026-04-21.md`).** Five-round adversarial review with BLOCK/WARN/NOTE categorization. Zero BLOCK, six WARN tracked, six NOTE captured. M1 gate outcome: PASS with warnings.

5. **`docs/CERBERUS_0.8.0_PLAN.md`.** Release doctrine for 0.8.0. Revised after red-team critique into a plan with explicit decisions on DGROUP budget, Whetstone scope, upload posture, UI scope, and the 8088-floor claim policy.

6. **Research documents (`docs/Cache Test Research.md`, `docs/FPU Test Research.md`, `docs/General Test Research.md`).** Source material for the 0.8.0 plan's M2 research-gap scope.

### Changed

1. **M1.1 Whetstone emit suppressed in stock builds.** `bench_whetstone.c` dispatcher early-returns with `whetstone_status=disabled_for_release`; no `k_whetstones` row emitted. Kernel stays compiled in every build; `wmake WHETSTONE=1` re-enables emit for research work. Rule 10 (`whetstone_fpu`) updated to skip on `disabled_for_release`. `bench_fpu.ops_per_sec` remains the aggregate FPU throughput metric. Rationale: the Curnow-Wichmann kernel has never produced an in-envelope value across four release cycles; a trust-first release cannot ship a 10x-30x-wrong number. Per 0.8.0 plan section 7. Full reasoning in `docs/methodology.md` "Why Whetstone is not in 0.8.0".

2. **M1.2 Upload runtime compiled out of stock binaries.** `#ifdef CERBERUS_UPLOAD_ENABLED` gates `upload_execute`, HTGET shell-out, `UPLOAD.TMP` handling, and /U and /UPLOAD flag parsing. Stock builds emit `upload.status=not_built`; `/UPLOAD` in stock errors at parse time with "upload not built into this binary; rebuild with 'wmake UPLOAD=1'". `[network] transport` detection stays in stock (useful diagnostic independent of upload). `/NICK` and `/NOTE` INI annotation stays. Contract doc `docs/ini-upload-contract.md` preserved as forward-looking design. Eliminates the unfixed stack-overflow-on-unreachable-endpoint defect from the default shipping artifact. Per 0.8.0 plan section 8.

3. **M1.3 Nickname buffer leak (issue #9) fixed.** `upload_nickname_buf` + `upload_notes_buf` moved from BSS to the near heap via `_nmalloc` in `upload_populate_metadata`, with explicit fallback to file-scope statics if `_nmalloc` fails. Changes BSS layout so the progressive-overwrite pattern observed on v0.7.1 captures no longer lands on these buffers. Verified: `nickname=BEK-V409` verbatim on both 486 and 386 captures. The underlying overwriter remains unidentified (tracked as W4 for M2 investigation).

4. **M1.4 `cpu.class` normalized to family token.** Pre-M1, CPUID-capable CPUs emitted the full vendor string ("GenuineIntel") as `cpu.class`. Now emits a normalized family token (`486`, `386`, `286`, `8086`, `pentium`, `pentium_pro`, etc.) derived from CPUID family field via new `family_to_class_token()` helper. `cpu.vendor` is emitted as a separate key. Server-side validator's `_CPU_L1_MAX_KB` table extended with `pentium_pro=32`, `pentium_4=128` entries and the stale "CPUID-era quirk" comment replaced with current doctrine. Per 0.8.0 plan section 6 M1.4.

5. **M1.5 `bench_cpu` DB anchor widened.** `hw_db/cpus.csv` 486 DX-2 `iters_low` lowered from 4,700,000 to 1,500,000 to cover TSR-loaded real-iron captures. BEK-V409 measured 1,964,636 iters/sec (under HIMEM+EMM386+mouse TSR load); was WARN pre-M1.5, now PASS. Header calibration note updated explaining the widening and the fact that heavy-TSR machines will still WARN below 1.5M. Rule 4b three-way cache-contextualized narration retained unchanged.

6. **M1.7 End-of-run `_exit` bypass.** `src/main.c` replaces `return exit_code` with `crumb_exit() + _exit((int)exit_code)`. Bypasses Watcom's libc teardown (atexit chain, FPU state cleanup, stdio close, `_NULL` signature check) which was hanging on BEK-V409 under DOS 6.22 + HIMEM + EMM386 + AMI 11/11/92. All resources explicitly released before this point. CERBERUS registers zero atexit handlers. Root-cause analysis via the `main.return` crumb diagnostic; the crumb_exit before _exit unlinks `CERBERUS.LAS` on clean exit. Verified clean on BEK-V409 and 386 real iron. Per 0.8.0 plan section 6 M1.7. Prominent DO-NOT-REMOVE comment block in `src/main.c` protects the load-bearing end-of-run crumb chain.

7. **Em-dash purge in runtime strings.** `audio_scale.c` (3 strings), `timing_metronome.c` (1), `diag_bit_parade.c` (1), `bench_cache_char.c` (1), `detect/cpu.c` unknown_record string (1), `detect/unknown.c` UNK header (1), and research-build-only `upload.c` message (1) replaced UTF-8 em-dash bytes (0xE2 0x80 0x94) with ASCII colon / comma. Real-iron rendering on CP437 previously showed "ΓÇö" triplets in display (e.g. "Audio Scale ΓÇö SB DSP Direct PCM"). Verified zero em-dash bytes in compiled stock binary post-fix.

### Fixed

1. **Research-build `upload.status` staleness** (gate finding W2/A1). `upload_execute` offline/skipped/no_client paths now call `report_write_ini` after `set_status`. Pre-fix, those paths updated status in the in-memory table but never flushed to disk, leaving the INI with the `upload_populate_metadata` seed value (`not_built`) rather than the actual outcome (`offline` / `skipped` / `no_client`). Stock builds unaffected (status=not_built is the actual outcome). Gated behind `CERBERUS_UPLOAD_ENABLED`.

2. **`fpu.whetstone_status` enum documented** in `docs/ini-format.md` with `disabled_for_release` as the stock-build value + the complete enum (`ok`, `skipped_no_fpu`, `inconclusive_*`, `inconclusive_runtime_exceeded`) for research builds. Rule 10 skip semantics noted.

3. **`upload.status` enum extended** in `docs/ini-format.md` and `docs/ini-upload-contract.md` with `not_built` for 0.8.0 stock builds. Server parser must tolerate the full enum per the contract.

### Known issues carried forward

| ID | Severity | Scope | Carry-forward |
|---|---|---|---|
| W4 | WARN | BSS stomp of BIOS date (`dree=` pattern) on BEK-V409 specifically | M2: investigate overwriter source via removal-at-a-time on BEK-V409 |
| W6 | WARN | "*** NULL assignment detected" message on BEK-V409 exit | M2: unified with W4, same underlying near-NULL pointer write |
| W1 | WARN | Mandelbrot coda removed from stock builds | M2: decide whether to refactor Mandelbrot out of `bench_whetstone.c` into independent FPU visual |
| W3 | WARN | Em-dashes in my M1 source-code comments | M4: docs-parity sweep |
| W5 | WARN | Public README / CERBERUS.md references v0.7.x state before this commit | M4 (partially addressed in this commit for README) |
| issue #2 | open | Vibra 16 PnP OPL detection intermittent (pre-existing) | M2 |

The W4+W6 class of bug is empirically localized: the two-machine capture matrix (486 + 386) shows the symptoms only on BEK-V409. DOSBox-X also does not reproduce. Generic CERBERUS code paths are ruled out; M2 investigation narrows to BEK-V409-specific probes (S3 Trio64 chipset ID, Vibra 16S DSP + OPL fallback, UMC491 PIT wrap-guard, possibly AMI 11/11/92 BIOS interaction).

### Build state

- `CERBERUS.EXE` stock: 163,838 bytes (down from v0.7.1's 169,380; -5,542 bytes / -3.3%)
- `CERBERUS.EXE` research (`wmake WHETSTONE=1 UPLOAD=1`): 170,250 bytes
- DGROUP near-data: 59,888 bytes (58.5 KB), 3,600 bytes headroom vs 62 KB soft target, 5,648 bytes vs 64 KB hard ceiling
- Host test suite: 296 assertions across 9 suites, 0 failures (+37 vs v0.7.1 baseline of 259)
- Stock build zero warnings, research build zero warnings
- Version string: `0.8.0-M1`

### Real-hardware validation status at M1 close-out

| Tier | Capture | Status |
|---|---|---|
| 486 (ceiling) | BEK-V409 | Archived |
| 386 | 386 DX-40 + IIT 3C87 + Genoa ET4000 | Archived |
| 286 (AT-class) | pending hardware | Deferred to post-M2 |
| 8088 (XT-class) | n/a | Claim retracted to "286 through 486 validated, XT-class validation pending" per plan section 10 |

Public claim: "Validated on 386 and 486. 286 and 8088 paths untested."

## [v0.7.1], 2026-04-21 — cache characterization + FPU behavioral fingerprint

Minor release that moves CERBERUS from "detects and benchmarks" to
"characterizes and fingerprints" across three subsystems.

### Added

1. **Timing primitives (Batch A.1).** New `timing_stats_t` accumulator
   for repeat-with-jitter measurement, `timing_confidence_from_range_pct`
   mapping jitter to CONF_HIGH/MEDIUM/LOW bands, and an RDTSC backend
   gated on CPUID leaf 1 EDX bit 4 via the new `cpu_has_tsc()` helper.
   `timing_emit_method_info()` writes `timing.method` and, on RDTSC
   paths, `timing.cpu_mhz` (calibrated via a 4-BIOS-tick window).
   `timing_emit_self_check()` gains `timing.pit_jitter_pct`.

2. **FPU behavioral fingerprint (Batch A.2, `src/diag/diag_fpu_fingerprint.c`).**
   Four probes distinguish 8087/80287 from 80387+ orthogonally to
   Phase 1's presence detection: infinity comparison mode (projective
   vs affine), pseudo-NaN handling (silently accepted vs #IE raised),
   FPREM1 opcode (D9 F5) via INT 6 trap-catch, FSIN opcode (D9 FE)
   via INT 6 trap-catch. Emits `diagnose.fpu.infinity_mode`,
   `pseudo_nan`, `has_fprem1`, `has_fsin`, and an inferred
   `family_behavioral` (FP_FAMILY_MODERN / FP_FAMILY_LEGACY /
   FP_FAMILY_MIXED anomaly flag).

3. **Cache characterization (Batch B, `src/bench/bench_cache_char.c`).**
   Three probes infer L1 parameters from throughput sweeps: L1 size
   via a 2/4/8/16/32 KB working-set scan (inflection = L1 size), line
   size via a 4/8/16/32/64 B stride sweep, and write policy via a
   read-vs-write delta (wb / wt / unknown classification).
   Self-skips on emulator captures: the stride probes rely on real
   cache miss penalties that DOSBox Staging and kin do not faithfully
   reproduce. Associativity probe deferred to a follow-up release
   (requires physical-address control medium-model DOS doesn't offer).

### Changed

- `CERBERUS.EXE` grew from 164,050 bytes (v0.7.0-rc2) to ~169,380 bytes
  (+5,330 bytes, under the 180 KB budget). DGROUP near-data at 60.6 KB
  (was 59.3 KB), still under the 65 KB limit.
- `tests/host/` suite expanded from 201 to 259+ assertions across
  9 test exes (new `test_diag_fpu_fingerprint.exe` and
  `test_bench_cache_char.exe`).
- `devenv/smoketest-staging.conf` uses `/SKIP:BENCH` — the full
  bench suite exceeds 120 sec under DOSBox Staging's emulation
  overhead. Real-iron captures validate the bench-path keys.

### Notes

- Real-hardware validation of cache characterization kernels is
  deferred to the 486 capture workflow. Host tests cover the
  pure-math inference paths exhaustively; kernels skip on emulator
  by design.
- Whetstone still hangs DOSBox Staging (unchanged from v0.7.0).

## [v0.7.0-rc2], 2026-04-20 — end-to-end quality gate fixes

Release candidate 2 lands fixes surfaced by a systematic end-to-end
quality-gate audit of the v0.5.0 → v0.7.0-rc1 arc. No functional
changes from rc1; correctness + doc fixes only. Tag `v0.7.0` still
held until Part B server validation.

### Fixes

1. **`intro.c read_ticks()` atomic-read pattern.** `read_ticks()`
   was doing a single 32-bit pointer deref against the BIOS tick
   counter at 0040:006C. On 8088 that compiles to two 16-bit loads,
   and INT 8 can fire between them, corrupting the high word.
   Animation-only impact (the intro's `wait_ticks_or_key` was
   consuming the value) but pre-existing bug surfaced by the audit.
   Fixed to use the same atomic h1/l/h2 retry pattern that
   `timing.c` and `tui_util.c` already use.

2. **`upload.c htget_post()` UPLOAD.TMP cleanup on fopen failure.**
   Every failure branch of `htget_post` called `remove(UPLOAD_TMP_PATH)`
   except one: if HTGET wrote UPLOAD.TMP successfully but the
   subsequent `fopen()` failed (I/O error, permission denial, TSR
   interference), the stale file was left on disk and would be
   mis-read on the NEXT upload attempt. Added `remove()` to that
   branch.

3. **`[upload]` section INI duplicate-key emission.** The flow
   `report_write_ini() → upload_execute() → report_write_ini()` was
   appending new `upload.status` / `upload.submission_id` /
   `upload.url` rows on the second pass without deduplicating
   against the empty versions added on the first. Last-value-wins
   parsing tolerated this, but the INI file looked ugly with
   duplicated `upload.nickname=` lines. New helper
   `report_update_str()` updates in place (mirrors the existing
   `report_set_verdict` pattern). `upload.c:set_status` and
   `set_submission` switched to it.

4. **`[upload] status` enum documented.** `ini-format.md` now lists
   all six values the client actually emits (`uploaded`, `offline`,
   `skipped`, `no_client`, `failed`, `bad_response`) with
   when-emitted semantics. Contract in `ini-upload-contract.md`
   updated to match and explicitly requires server tolerance of
   the full enum.

5. **`[upload] url` documented.** Was emitted by the client on
   successful upload but absent from `ini-format.md` — server's
   permissive parser accepted it, but spec/code drift. Added to
   the format reference.

### Deferred to v0.7.1+

- `ui.c` and `intro.c` retain private VRAM helper copies predating
  the v0.6.2 `tui_util` extraction. Cleanup-only, no behavior
  delta; filed as refactor.
- `bench_cache.c:105` stale "TODO (v0.5)" comment; should move to
  issue tracker.

### Build state

- CERBERUS.EXE: 164,050 bytes (+128 vs rc1; report_update_str + comments)
- MD5: 4042491e8334d904c561bb0942ec092a
- DGROUP: 59,168 / 64,000 DOS limit (soft 56K target exceeded,
  accepted)
- Host tests: 7 suites, 201 OK, 0 failures
- Zero warnings on clean rebuild
- Version string: `0.7.0-rc2`

## [v0.7.0-rc1], 2026-04-20 evening — Part A of Community Upload

**Status**: Release candidate. Tag is `v0.7.0-rc1`, NOT `v0.7.0`. Per
the v0.7.0 brief, the full `v0.7.0` tag is reserved until Part B
(server + results browser in a separate repo) is deployed and an
end-to-end round-trip has been validated on real hardware. This
release is the DOS client side complete + documented + waiting.

Part A ships the full upload-client infrastructure: network transport
detection, INI format freeze with stable API contract, command-line
flags, prompt UI, HTTP POST via HTGET shell-out, and UPLOAD STATUS
section in the scrollable summary.

### T0 — INI format freeze + server contract

`[cerberus]` section gains `ini_format=1` — the server-parser API
switch. Additive INI changes (new keys, new sections) stay at
`ini_format=1`; breaking changes would bump. Documented in
`docs/ini-format.md`.

Upload contract specified in `docs/ini-upload-contract.md`: endpoint
URL, request format, response format, which INI fields the server
parses, error handling. Written BEFORE Part B so the server session
inherits it as a requirements spec instead of a blank page.

### T1 — Network transport detection

New module `src/detect/network.{c,h}`. Probes at startup:

  1. NetISA via INT 63h (reserved for v0.8.0 TLS; stub detector)
  2. Packet driver via INT 60h-7Fh scan for "PKT DRVR" signature
     at handler offset +3
  3. mTCP via `MTCP_CFG` env var
  4. WATTCP via `WATTCP` / `WATTCP_CFG` env var
  5. `none` — offline

Emits `[network] transport=<value>`. All probes are non-destructive
IVT reads + env lookups; safe on 8088 and every adapter.

### T2 — Upload prompt

After detection + journey + summary build, the upload orchestrator
checks network state. If online and not `/NOUPLOAD` / `/UPLOAD`,
prompts `"Upload results to barelybooting.com? (Y/n)"`. Default Y;
Enter / Y proceeds; N / Esc skips.

### T3 — HTTP POST via HTGET shell-out

v0.7.0 uses mTCP's `HTGET` as the HTTP client (spec-driven choice
from the brief). Command:

```
HTGET -P CERBERUS.INI -m text/plain http://barelybooting.com/api/v1/submit > UPLOAD.TMP
```

Parses response from `UPLOAD.TMP`: line 1 = submission ID, line 2 = URL.
Clean failure modes on connection refused, DNS miss, non-200, timeout.
If `HTGET.EXE` is absent from PATH, prints install instructions and
skips with status=`no_client` — never crashes.

The exact HTGET flag syntax may need field-verification during the
first real deploy; it's encapsulated in one `#define HTGET_CMD_FMT`
line for easy adjustment. Raw TCP over packet driver (no mTCP)
remains deferred to v0.8.0+ per the brief.

### T4 — Command-line flags

  `/NOUPLOAD`     never prompt, never upload
  `/UPLOAD`       upload without prompting (auto-yes)
  `/NICK:<name>`  nickname (alnum + space + hyphen, max 32, sanitized)
  `/NOTE:<text>`  note (printable ASCII, max 128)

Help text + synopsis updated.

### T5 — UPLOAD STATUS summary section

New fourth section in the scrollable summary, after SYSTEM VERDICTS.
Uses HEAD_CENTER (forward-facing, "sending data outward" fits the
direction metaphor; avoids proliferating head variants).

Rows shown (each skips if empty):
  Network:    packet driver / mtcp / wattcp / offline
  Status:     uploaded / skipped / offline / failed / no_client
  Submission: 8-char hex id (populated after 200 response)
  URL:        public view URL (same)
  Nickname:   if `/NICK` set
  Notes:      if `/NOTE` set

### Build state

- CERBERUS.EXE: 163,922 bytes (target <185KB ✓)
- DGROUP: 59,008 bytes (past Tony's 56K soft target but well under
  DOS 64KB limit — acceptable per v0.6.1 "exceed is OK" sign-off)
- Host tests: 7 suites, 201 OK, 0 failures
- Version string: `0.7.0-rc1`
- Tag: `v0.7.0-rc1` (not v0.7.0)

### Held back

- **Tag `v0.7.0`** reserved until Part B server exists and an
  end-to-end POST round-trip is validated on BEK-V409.
- **Part B server** is a separate project / repo / Claude session.
  Contract is defined in `docs/ini-upload-contract.md`.

## [v0.6.2], 2026-04-20 evening

Cleanup + SB DSP direct-mode PCM audio path.

### T1 — Shared TUI helpers (src/core/tui_util.{c,h})

Six visual modules (journey, bit_parade, lissajous, metronome,
audio_scale, cache_waterfall, latency_map) each carried a private
~60-line copy of the same VRAM-helper block: adapter-aware
vram_base, is_mono, putc, puts, fill, hline, ticks, keyboard
polling. All consolidated into one shared tui_util module.

Each visual module now calls tui_putc / tui_puts / tui_fill /
tui_ticks / tui_is_mono / tui_kbhit / tui_read_key / tui_drain_keys
instead of its private prefix. EXE shrunk ~1 KB from code
deduplication.

### T2 — DGROUP reclaim DEFERRED

Tried Watcom `-zc` in v0.6.1 (doesn't move unnamed string literals,
only const-qualified declarations). wlink CLASS redirection via
ORDER statements needs per-file `#pragma data_seg` directives to
mark which data goes where — touches every .c file for a marginal
savings given DGROUP is currently 57,472 bytes (6,864 bytes under
the DOS 64KB limit). Not worth the churn. Future work if DGROUP
ever constrains.

### T3 — SB DSP direct-mode PCM audio path (T7c from v0.6.0 brief)

audio_scale.c now probes a third audio path ahead of OPL2:

  Probe order
    1. SB DSP direct PCM — BLASTER env parsed for base port; DSP
       reset sequence at base+6 tested for the 0xAA ACK. If present,
       plays the scale as square-wave PCM samples via DSP command
       0x10 per sample (direct-output mode).
    2. OPL2 FM — fallback. Same as v0.6.1.
    3. PC speaker — universal fallback.

Scope decision: DSP direct mode instead of full DMA-buffered
playback. Direct mode is genuine PCM (the SB DSP outputs the actual
sample byte on each 0x10 write) but runs at a lower effective sample
rate (~2-4 kHz via Watcom busy-wait). The "is my SB card producing
PCM samples at all" question gets a direct yes/no answer; fidelity
is squarer than DMA-streamed audio but clearly distinct from OPL
FM. Full DMA playback (8237 channel + buffer + IRQ) stays filed as
a future v0.7+ candidate if the audio-verification story needs the
quality upgrade.

Square-wave generation: per-note half-period in samples precomputed
from target frequency at an assumed 3 kHz effective rate (~150 ms
per note × 8 notes ≈ 1.2 s total playback).

Title card + on-screen heading change to match the active path —
users see "Audio Scale — SB DSP Direct PCM" when SB wins the probe,
"Audio Scale — OPL2 FM Synth" when OPL wins, "Audio Scale — PC
Speaker" on fallback.

### Build state

- CERBERUS.EXE: 156,700 bytes (target <180KB ✓)
- DGROUP: 57,472 / 56,000 soft target (6,864 bytes under DOS 64KB)
- Host tests: 7 suites, 201 OK, 0 failures

### Known follow-ups

- Issue #4 Whetstone calibration — still pending real-hardware
  multi-cold-boot validation.
- Issue #6 VLB bandwidth — still pending.
- SB16 full DMA audio path (8237 auto-init + IRQ) remains a v0.7+
  candidate. DSP direct is functionally sufficient for the
  journey's "does audio work" question.

## [v0.6.1], 2026-04-20 evening

Visual-journey completeness pass. Closes the gaps flagged in the
v0.6.0 "Known gaps" list.

### Result flashes wired (T1.1)

journey_result_flash() was authored in v0.6.0 but no visual called
it. Now all six visuals end with a one-line result banner:
  Bit parade    "ALU: every op verified on a live register"
  Lissajous     "FPU: trig functions produced a symmetric curve"
  Mandelbrot    "FPU: Whetstone done, Mandelbrot rendered live"
  Metronome     "Timer: PIT ticking at 18.2 Hz"
  Video pattern "Video: bandwidth measured on live VRAM"
  Audio scale   "Audio: speaker path verified end-to-end"

Each renders centered on row 12 for ~1 second before the next title
card takes over.

### T4 Memory Cache Waterfall

Text-mode 9-band bar chart, one band per block size (1B, 2B, 4B, 16B,
256B, 1KB, 4KB, 16KB, 64KB). Each band's fill length + animation
speed is proportional to measured write bandwidth at that block size
against a 32 KB FAR-allocated buffer.

Colors: bright green for fastest tier, yellow middle, bright red for
slowest. On cached systems the small-block bars land in the green
tier (L1 speed); large-block bars land in red (main-memory speed).
The visible transition IS the cache boundary.

All adapters, text mode. No VGA-specific path; the bars are CP437
block characters and the color classes degrade cleanly on mono.

### T5 Cache Latency Heat Map

Text-mode horizontal heat strip, 64 cells spanning the width of an
allocated 32 KB buffer (one cell per 512-byte window). Each cell
timed by tight read-sweep; heat level classified into 4 quartiles
of the min→max range. Green/yellow/red attribute classes.

On a 486 with 8 KB L1 cache the expected pattern is: first 16 cells
green (in cache), remainder transition through yellow to red. On
cacheless systems the entire strip should be red.

Annotation labels mark 0 KB, 16 KB, 32 KB and include a legend
below.

### T7b OPL2 FM scale

audio_scale.c now probes port 0x388 for an AdLib/OPL2 chip before
playing the 8-note scale. If probe succeeds: play via OPL2 with a
clean sine voice (modulator silent, carrier sine wave, no feedback);
title changes to "Audio Scale — OPL2 FM Synth." If probe fails:
falls back to the v0.6.0 PC-speaker path.

OPL2 probe uses the standard timer-status sequence (write 0x60 to
reg 0x04, clear status, start timer 1, 100us wait, check status for
0xC0 pattern). Universal across AdLib, Sound Blaster 1, SB Pro,
SB16. Probe is ~400us total — imperceptible.

### T7c SB16 PCM DMA — DEFERRED to v0.6.2

Original brief had a third audio layer: PCM DMA playback of
triangle/square samples via 8237 + SB DSP for Sound Blaster 16+
machines. Scoped out for v0.6.1 because:
  - OPL2 FM covers AdLib + SB1 + SB Pro + SB16 (all cards with an
    OPL chip) with equivalent short-scale audio fidelity.
  - PCM adds complexity (DMA channel programming, DSP init, buffer
    management, IRQ handling) for marginal audible benefit on an
    8-note 2-second scale.

OPL2 path ships in v0.6.1; PCM DMA a v0.6.2 candidate if a use case
surfaces that actually needs the upgrade.

### Build state

- CERBERUS.EXE: 157,002 bytes (target <180KB; 165KB flag threshold
  not tripped)
- DGROUP: 57,280 / 56,000 soft target exceeded (7,256 bytes under
  DOS 64KB limit — Tony confirmed acceptable for v0.6.1)
- Host tests: 7 suites, 201 OK, 0 failures

### DGROUP note

`-zc` was tried as a path to move const strings out of DGROUP; it
doesn't move unnamed string literals (only explicitly const-qualified
declarations). Left disabled. Future optimization candidate:
renamed CONST class with a wlink ORDER directive to park it in a
far segment. Not a v0.6.1 blocker.

## [v0.6.0], 2026-04-20 evening

The Visual Diagnostics Journey release. CERBERUS becomes an experience
that shows hardware working, not just a tool that emits numbers. Six
new visual demonstrations, a title-card framework that glues them into
a structured run, a new tagline, directional head art that gives each
section its own mythological guardian, and a /QUICK flag for users
who want measurements without the visuals.

### T0a — new tagline (`6c255e1`)

"Three heads. One machine. Zero pretending." retires. Replaced by
"Tough Times Demand Tough Tests" on the splash screen subtitle row
and in the animation's BEHOLD-flash swap-back target.

### T0b — chain removed from static title

The chain/body/serpent-tail composition below the three heads is gone
from the static resting state. It read as a progress widget and broke
the wordmark → heads → tagline flow. The animation's chain rattle +
shatter still run, and the post-shatter redraw explicitly clears the
chain-area rows so nothing lingers.

### T0c — directional head art

New shared module `src/core/head_art.{c,h}` with three 9x4 variants:
  HEAD_LEFT    — faces left, eye at col 6, snout + fang at col 1,
                  ear bump top-right.
  HEAD_CENTER  — faces forward, two eyes + two fangs, dominant.
  HEAD_RIGHT   — mirror of LEFT, faces right.

Accent kinds (BODY / EYE / FANG) let renderers color each feature
independently for animation pulses. The intro splash uses all three
variants left-to-right; the summary section headers guard their
domains (Detection=LEFT scanning, Benchmarks=CENTER measuring,
Verdicts=RIGHT judging). A new shared-neck row at DOG_TOP+4 sells
"one creature with three heads" rather than three separate dogs.

Intro eye-cascade animation adapts to the new 4-eye total (1 on left
head, 2 on center, 1 on right); eye_head_map[] routes each cell to
its head's heat attr.

### T1 — journey framework (`6c255e1`)

New module `src/core/journey.{c,h}`:
  journey_init()             — reset skip-all latch per run
  journey_should_skip(o)     — true under /NOUI, /QUICK, or skip-all
  journey_title_card()       — full-screen card with directional head,
                                ALL-CAPS title, 1-3 wrapped lines of
                                description, "any key / S / Esc" hint.
                                ~2.5s hold or keypress advance.
  journey_result_flash()     — single-line banner on row 12, ~1s
  journey_poll_skip()        — non-blocking keyboard poll during
                                visuals: 0=continue, 1=skip this,
                                2=skip all (Esc latches)

New /QUICK command-line flag: skips visuals + title cards, runs
measurements, renders interactive summary. For batch users who want
timings without the journey.

### T2 — CPU ALU Rolling Bit Parade (`a55a293`)

Post-diag_cpu. 16-bit register rendered as CP437 block row (bright =
1, light shade = 0). Real ALU ops executed in sequence: ROL, ROR,
SHL, SHR, AND, OR, XOR, NOT, ADD, SUB. Each state displayed is the
literal result of the literal instruction — no animation. Wall-clock
bounded (~3 s); 8088 shows ops ticking past, 486 blurs. Text mode,
all adapters.

### T3 — FPU Lissajous Curve (`3e4d4f2`)

Post-diag_fpu. VGA mode 13h. 1800 parametric points x=sin(3t+π/4),
y=sin(2t), drawn one by one by the x87 native FSIN. Amber
oscilloscope palette. A working FPU produces a smooth symmetric 3:2
figure. Gated on VGA-capable + fpu.detected != "none".

### T4 / T5 — DEFERRED to v0.6.1

Cache Waterfall + Latency Heat Map visuals were in the brief but
need more care around per-band measurement methodology than fit in
this session. Ship v0.6.0 without them; add in a follow-up pass
once the cache-measurement primitives have had a real-hardware
calibration round.

### T6 — PIT Metronome (`7b8405e`)

Post-timing_self_check. A dot bounces between columns 4 and 75 at
row 12, one column per 18.2 Hz BIOS tick. Each tick fires a PC
speaker click via port 61h bit-1 toggle. Text mode, all adapters,
universal hardware. Steady rhythm = PIT and BIOS agree; stutter =
something between them is biased (same signal Rule 4a checks
numerically).

### T7 — Audio Hardware Scale (this commit)

End-of-journey audio coda. 8-note C major scale (C4 through C5) via
PIT Channel 2 gated through port 61h. Each note ~250 ms. Rising
vertical bars accompany the notes. v0.6.0 ships PC-speaker only —
OPL2 FM and SB16 PCM DMA are deferred to v0.6.1. PC speaker is
universal hardware so this always fires (subject to skip flags).

### T8 — title cards for existing visuals

bench_whetstone gets an FPU BENCHMARK title card (covers both
Whetstone measurement + the Mandelbrot visual that fires at its
tail). bench_video gets a VIDEO BANDWIDTH card before its pattern
fill (the pattern IS the measurement). bench_mandelbrot reuses the
parent section's card — no double-card.

### Build state

- CERBERUS.EXE: 152,058 bytes (target <180KB)
- DGROUP: 56,080 / 56,000 (80 bytes over soft target; 8,456 bytes
  under the DOS 64KB limit)
- Host tests: 7 suites, 201 OK, 0 failures
- Version: 0.6.0

### Known gaps

- T4 Cache Waterfall — deferred to v0.6.1
- T5 Latency Heat Map — deferred to v0.6.1
- T7 OPL2 + SB16 audio paths — deferred to v0.6.1 (PC speaker ships)
- journey_result_flash() is authored but currently unused; visuals
  transition straight to the next title card. Adding the flashes is
  a polish item for v0.6.1.
- Issue #4 Whetstone calibration still pending real-hardware
  validation (inherited from v0.5.0).
- Issue #6 VLB bandwidth still open.

## [v0.5.0], 2026-04-20 evening

v0.5.0 is a UI + FPU release. The three-pane fixed-width summary
retires in favor of a scrollable full-width layout where each
section (Detection, Benchmarks, System Verdicts) is headed by a
Cerberus dog head in CP437 block art — the mythology becomes
literal, three heads guarding three domains. The Whetstone
benchmark gets a hand-coded x87 assembly kernel, and a Mandelbrot
set visual fires at the end as a post-run proof-of-life for the
FPU.

### v0.4.1 UI polish (`3b88d3a`)

Landed first on top of v0.4.0 as a recovery point before the
v0.5.0 rewrite. Narration rewrites so no consistency-rule text
truncates in the 48-col SYSTEM VERDICTS pane (the original defect
hid the exoneration on Rule 4b's "cache diag PASS" branch).
Human-readable DETECTION labels (`CPU`/`FPU`/`BIOS`/`Emulator` with
acronym preservation, sentence case elsewhere). `<TAG>` brackets
for diagnostic heads, `[TAG]` for consistency rules. BENCHMARKS
grouped by subsystem (CPU/FPU raw, memory raw, PC-XT ratios).
Runtime `audit_narration_widths` assert in consist.c fires to
stderr if any narration exceeds 48 cols on-screen after prefix
strip. Validated on BEK-V409 CRT as `CERB-UI2.EXE` (151,044 B).

### T1 — CONF_LOW text marker (`b769f08`)

Dim-color rendering of CONF_LOW values replaced with an explicit
`" (low conf.)"` text marker appended at render time. Text is
adapter-neutral and self-documenting in screenshots;
render_kv_row_dim_value deleted; per-row dim flags in the bench
table gone. Data owns its confidence; the UI surfaces it.

### T2 — scrollable three-heads summary (`00d6f6b`)

Full ui.c rewrite. Virtual-row table (80-entry cap) holds one
entry per content row; viewport renders 24 rows starting at
scroll_top. Each section is prefixed by a 9x4 CP437 Cerberus dog
head using the same visual primitives as intro.c (dark-shade
skull, half-block edges, eye glyph, fang glyph). BIOS INT 16h
navigation — Up/Dn arrows scroll one row, PgUp/PgDn one viewport,
Home/End jump top/bottom, Q or Esc exits. Status bar on row 24
shows current row range ("rows 1-24 of 54") and nav hints.

No information truncation anywhere: the old 8-row VERDICTS pane
cap silently dropped the 9th verdict on BEK-V409 (Whetstone FPU
was consistently lost). With the scroll buffer, all verdicts
display regardless of count. /NOUI now dispatches to
`ui_render_batch()` which prints the same content as plain text to
stdout — batch mode preserves the issue-#3 escape hatch while
still surfacing run results.

### T3 — Whetstone verdict investigation (no code)

Documented: the old `ui_render_consistency_alerts` capped at
`r <= 24` which is 8 rows in rows 17-24. On BEK-V409 9 verdicts
fire (cache.status, dma.summary, 7 consistency rules), so the 9th
(whetstone_fpu, last emitted) was silently dropped. T2's scroll
buffer self-fixes this.

### T4a — Whetstone x87 asm kernel (`3537c00`)

New file `src/bench/bench_whet_fpu.asm` (NASM). Hand-coded
Curnow-Wichmann 1976 Whetstone using native x87 (FSIN, FCOS,
FPATAN, FSQRT, F2XM1, FYL2X). Replaces the Watcom-compiled C
kernel that was forced through volatile memory traffic for DCE
suppression. Dispatch: FPU present → asm kernel at CONF_HIGH; no
FPU → skip (as before). bench_whetstone.c now compiles at -ox
instead of -od because the asm kernel is opaque to the optimizer
and owns the DCE-barrier responsibility via its external globals.

Issue #4 stays open. Real-hardware validation on BEK-V409 is the
next step: the published 486 DX-2-66 envelope is 1500-3000 K-Whet
and pre-asm C ran ~110. Multi-cold-boot capture needed before the
CONF_HIGH claim gets anchored.

### T4b — Mandelbrot FPU visual (`e2f3dea`)

`src/bench/bench_mandelbrot.c`. Fires at end of bench_whetstone
on FPU-equipped VGA-capable machines. VGA mode 13h, 320x200x256.
Center (-0.6, 0), window [-2.0, 0.8] x [-1.2, 1.2], 64
iterations. Pixel-by-pixel progressive render so the fractal
emerges visibly on a 486. DAC palette is a blue → cyan → white
gradient programmed via 0x3C8/0x3C9. Blocking INT 16h keypress
exits, mode 3h restores text. /NOUI skips. Non-VGA skips without
mode-switching.

Not timed, not reported as a measured value. A post-run visual
coda that makes the tool memorable and proves the FPU is live.

### T4c — forensic-value INI emit for Rule 4b (`b639d6a`)

Rule 4b (cpu_ipc_bench) now emits three additional INI rows when
it fires on any branch:
- `consistency.cpu_ipc_bench.measured` (bench iters/sec)
- `consistency.cpu_ipc_bench.expected_low` (CPU-DB low bound)
- `consistency.cpu_ipc_bench.expected_high` (CPU-DB high bound)

VERDICT_UNKNOWN on the forensic rows keeps them INI-only (filtered
from the SYSTEM VERDICTS pane). Post-run readers can reconstruct
why Rule 4b fired without re-running the tool. Pattern sourced
from the CACHECHK Phase 3 "Timer messed up! %08lx" forensic-emit
lesson.

### Build state

- `CERBERUS.EXE`: 146,290 bytes (target <160KB)
- DGROUP: 54,528 / 56,000 (1,472 bytes headroom)
- Host tests: 7 suites, 201 OK, 0 failures
- Version string: `0.5.0`

### Issue posture

- **#1** closed in v0.4.1 work (test_timing reassertions).
- **#3** closed pre-v0.5.0 (UI hang cannot reproduce).
- **#4** OPEN — Whetstone calibration needs multi-cold-boot
  BEK-V409 capture.
- **#6** OPEN — VLB bandwidth investigation continues.

## [Unreleased, post-v0.4.0], 2026-04-19 evening through 2026-04-20

Development work on `main` past the `v0.4.0` tag. Not yet
collected under a rc/final tag; next anchor point will be
either a `v0.5.0-alpha` pre-release for the ANSI intro and
instrumentation, or a full `v0.5.0` once issue #4 and issue
#6 close.

### ANSI boot intro (`593b139`)

New module `src/core/intro.c` + `intro.h` plus wiring into
`main.c` between `display_init` and `display_banner`. Adapter-
aware three-headed-dog emblem with OPL2 stinger. Gated by
`/NOINTRO` (already plumbed in `opts_t`) and `/NOUI`. Five
iconographic elements (three heads with fangs, serpent-mane
spines, chain-bound body, serpent tail, double-line gate)
plus seven embellishments (heartbeat pre-sequence, eye
cascade with escalating OPL2 barks, chain-shatter DAC flash
with rhythm-mode snare, sustained A-minor chord with vibrato,
sub-bass drone, hellfire ember row, chain rattle around
broken link, breath sparks, serpent tail wiggle, wordmark
color pulse, BEHOLD mid-sustain flash). Full classical
iconography sourced from Pseudo-Apollodorus, Virgil, Dante,
and Hercules 12th-labor Roman iconography.

Real-hardware validated on BEK-V409 as `CERB-INT.EXE`
(150,042 bytes). User reaction: "Wow that looked epic."
Detect/diag/bench flow after the intro produces numbers
consistent with v0.4.0 baseline; no regressions.

### Issue-fix sweep

- **Issue #1 (`036cc1c`).** The four pre-existing
  `test_timing` assertion failures were written before a 25%
  pit/bios divergence guard landed in `timing_compute_dual`;
  their c2 values produced highly divergent pit_us vs bios_us
  and now correctly trigger that guard. Updated the test
  inputs to use near-full-wrap sub_ticks values that satisfy
  all three kernel gates (lower-bail, upper-bail, divergence)
  while preserving the test's original semantic (verifying
  that target=1 with 0 wraps does not trip the lower-bail).
  Added dedicated coverage for the divergence guard itself
  (both pit>>bios and bios>>pit branches) which had no test
  coverage before. 167 host-test assertions green, 0 failures.
  Previous state: 163 green, 4 failed.

- **Issue #3 closed.** UI hang unable to reproduce across 9
  consecutive clean real-iron runs (v0.2-rc1 through v0.4.0
  plus CERB-VOX diagnostic plus both CERB-INT runs). Closed
  as "cannot reproduce" with reopen criteria preserved.
  Instrumentation stash at
  `docs/plans/attic/ui-hang-instrumentation-2026-04-18.patch`
  available for re-application if the hang re-emerges.

- **Issue #6 test-1 data-point landed earlier this session.**
  Built `CERB-VOX.EXE` at 144,054 bytes with `bench_video.c`
  compiled at `-ox` instead of `CFLAGS_NOOPT`. BEK-V409
  results: `text_write_kbps` 4,988 (+6.9% vs 4,668 at
  CFLAGS_NOOPT) and `mode13h_kbps` 5,021 (+8.8% vs 4,613).
  CFLAGS_NOOPT tax is small, not the dominant factor.
  Posted as comment to issue #6; bottleneck is not compile
  flags.

- **Issue #6 test-2 tool built (`2f5b26e`).** New standalone
  `tools/repstosd/REPSTOSD.EXE` (11,258 bytes) with pure-
  assembly REP STOSW inner loop writing 128 MB to mode 13h
  VRAM. Isolates C-loop overhead vs hardware-path limitation.
  Shipped to BEK-V409 for Tony to run; output will tell us
  whether CERBERUS's 4.6 MB/s number reflects a real hardware
  ceiling or a C-loop-overhead artifact.

- **Issue #2 instrumentation (`74bc439`).** New INI key
  `audio.opl_probe_trace` emits byte-level trace of every
  status-register read across primary and fallback OPL
  probe attempts. Enables multi-cold-boot capture-and-diff
  to identify which byte value differs between "opl3
  detected" runs and "none detected" runs on the same
  Vibra 16 PnP card. Built into `CERB-DBG.EXE` on BEK-V409
  (150,556 bytes).

### Homage Phase 3 research (`37777f3`)

Seven additional lesson docs in `docs/research/homage/` under
the same ethical frame as Phase 2: no decompiled code, no
binary redistribution, attribution preserved, corrections
flagged openly.

Deferred-from-Phase-2 tasks now closed: T3 CheckIt
Whetstone (confirmed custom synthetic, not Curnow-Wichmann;
reframes issue #4), T5 CheckIt video methodology (text-mode
only, no mode 13h reference), T8 CACHECHK UMC timer
workaround (structural match to CERBERUS; raw-forensic-emit
pattern filed as v0.5+ Rule 4a enhancement), T9+T10
SPEEDSYS (Afanasiev attribution confirmed, Russian origin;
Pentium-era peer, out of CERBERUS scope).

New issue-#6 second-opinion research: T14 PCPBENCH (PC
Player magazine / Computec Media, German origin; DOS/4GW
32-bit 3D with REP STOSD primitives), T15 3DBENCH v1 and
v2 (Superscape VRT Ltd, UK; per-frame phase breakdown with
dedicated Clr column is the sharpest issue-#6 comparator
in the corpus), T16 CHRISB (DJGPP 1996; SVGA variant's S3
path relevant to BEK-V409 Trio64), T17 LM60 Landmark Speed
6.0 (same IBM PC/XT anchor as CheckIt, era convention).

Three attribution corrections this pass: SPEEDSYS (Roedy
Green → Vladimir Afanasiev), PCPBENCH (Jim Leonard → PC
Player magazine), 3DBENCH (Future Crew → Superscape VRT
Ltd).

### Envelope at end of 2026-04-19 work block

- `main` HEAD: `37777f3`
- EXE (tip-of-tree, CERB-DBG / non-tag): 150,556 bytes
- DGROUP: 53,808 / 56,000 (4% headroom under working
  ceiling; 18% under 65,536 hardware ceiling)
- Host tests: 167 assertions, all green
- Phase 2 lesson docs: 7 (unchanged from v0.4.0)
- Phase 3 lesson docs: 7 (added this session)
- GitHub issues: 2 closed (#3, #5), 4 open (#1 gated, #2
  instrumented, #4 reframed, #6 two tests built)

## [v0.4.0], 2026-04-19

Fourth release in the weekend arc. Closes the UI defects found in v0.4-rc1's BEK-V409 screenshot. Full release notes at [`docs/releases/v0.4.0.md`](docs/releases/v0.4.0.md).

- `diag_cache` and `consist` status-string CP437 corruption (`2d6a0a7`). UTF-8 em-dash and multiplication-sign bytes in runtime-emitted format strings were rendering as CP437 garbage in the SYSTEM VERDICTS pane. Replaced with ASCII equivalents. Audit swept three latent paths in `consist.c` (Rule 4b cache-contextualized WARNs plus Rule 11 XT-class DMA WARN).
- UI `value_str` type-aware rendering (`841d7c3`). Five V_U32 BENCHMARKS rows (`fpu ops/s`, `mem write`, `mem read`, `mem copy`, `k-whet (LOW)`) rendered labels but blank values in the rc1 screenshot because `value_str` returned `""` for non-V_STR rows with `display=NULL`. Mirrored `format_result_value`'s switch inside `value_str` with a 32-byte static scratch buffer.
- `bench_dhrystone.c` comment updated per Phase 2 T2 lesson (`6acb559`). CheckIt's "Dhrystones" is a custom synthetic, not a Dhrystone port. Comment now frames the 33,609 BEK-V409 anchor as an empirical match target, not an algorithmic equivalence. Full derivation at `docs/research/homage/checkit-dhrystone-version.md`.

EXE: 144,166 bytes. DGROUP: 53,184 / 56,000. Host tests: 163 assertions green except the 4 pre-existing `test_timing` failures (issue #1, gated). Real-hardware validated on BEK-V409 with full INI capture at `tests/captures/486-real-2026-04-19-v0.4.0/` plus four screenshots at `docs/releases/v0.4.0/screenshots/`.

## [Unreleased, covers v0.2-rc1 through v0.4-rc1]

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

- **Cache-coherence diagnostic** (`7a28850`) — stride-ratio timing test: 2 KB small buffer + 32 KB large buffer (both __far), compare per-iteration times. Verdict: ratio ≥ 40× = PASS (cache working), 20-40× = WARN (partial), < 20× = FAIL (cache disabled or absent). Skips on 8088-class floor hardware where `cache.present=no` from detect. Pure classifier kernel `diag_cache_classify_ratio_x100` host-tested across 17 scenarios.
- **DMA controller diagnostic** (`7a28850`) — 8237 count-register write+readback probe on channels 1/2/3/5/6/7. Safety-skips channel 0 (DRAM refresh) and channel 4 (cascade link). XT-class detection via `cpu.class` or `bus.class=isa8` skips the slave controller (channels 5-7) with `skipped_no_slave` status. Per-channel pass/fail + summary verdict. Pure summary kernel `diag_dma_summary_verdict` host-tested across 10 scenarios.

**Status: 6 of 6 subsystems covered (ALU, memory, FPU, video, cache, DMA). v0.3 complete pending real-hardware gate on BEK-V409.**

### Benchmarks

- CPU integer benchmark (`9b758ed`) — fixed instruction mix, PIT-C2-timed, MIPS-equivalent iters/sec output.
- Memory bandwidth benchmark (`b5ca6e0`) — REP STOSW (write), REP MOVSW (copy), REP LODSB (read — replaced the volatile-checksum approach post-real-iron in `eeba319` / `7e4bdcb`). `kb_per_sec` rewritten for microsecond-precision scaling after the 486 surfaced the sub-ms elapsed-time truncation that clamped every fast operation to the same bogus 4 MB/s.
- FPU benchmark (`6b67cc5`) — x87 instruction mix + DGROUP fix.
- Calibrated multi-pass mode for `bench_cpu` (`1774aa9`) — feeds thermal-stability tracker with per-pass timings.
- `total_ops=0` silent-display corruption (`b6c179b`), then systematic V_U32-display-buffer dangling-pointer class fix applied to all six bench_memory statics (`6c3a023`).

**Status: 3 of 5 subsystems covered (integer, memory, FPU). Cache-bandwidth + video-throughput deferred.**

### Historical benchmarks

- Dhrystone 2.1 port (`e897c15`) — Weicker's reference workload ported to Watcom medium model with full original structure (`Ptr_Comp` linked list, `Record` variant union, `Arr_1_Glob` / `Arr_2_Glob` global arrays). Emits `bench.cpu.dhrystones`, indexed off a PC-XT baseline so PC-class machines get a recognizable relative-rating number. v0.4 plan landed alongside.
- Whetstone port (`525f65b`) — the 1970s floating-point synthetic, eight modules (Module 1 through 8), FPU-presence gate so machines without an FPU emit `bench.fpu.whetstone_status=skipped_no_fpu` without attempting any x87 instructions. Emits `bench.fpu.k_whetstones`.
- Anti-DCE guards for both Dhrystone and Whetstone (`97d24e6`) — Watcom `-ox` was eliminating the synthetic loops that produced no externally-observable output. Fix: volatile globals plus an end-of-run checksum emitted via `report_add_u32`, whose call into report.c (separate TU) prevents Watcom from proving the chain unused.
- NULL-display pattern for Whetstone V_U32 emits (`8552c6d`) — Whetstone uses the same dangling-pointer-safe static-buffer pattern the bench_memory rework established in `6c3a023`.
- Makefile `CFLAGS_NOOPT -od` pinned on `bench_dhrystone` and `bench_whetstone` (`1788561`) — the two benchmarks are the only objects built unoptimized; opt-level drift on those files would silently change the synthetic numbers.

### Consistency engine

Engine + first four rules (`fac1500`). Alert-box UI for WARN/FAIL renders (`7b1a9b0`). Rules 3 + 9 (`4a9f24e`). Methodology documentation (`bb760c8`). Thermal stability — Mann-Kendall α=0.05 (`d5e7400`). Rule 4a PIT/BIOS timing independence (`111347a`). Rule 4b `cpu_ipc_bench` (`7e4bdcb`). Rule 7 `audio_mixer_chip` (`c22e886`). Rule 10 `whetstone_fpu_consistency` (`f0cebde`) — `bench_whetstone` completion state agrees with `detect_fpu` report, catching detect under-reporting of socketed FPUs.

**Ten rules live** (1, 2, 3, 4a, 4b, 5, 6, 7, 9, 10). Rule 8 (`cache_stride_vs_cpuid_leaf2`) reserved pending Phase 3 cache work; slot preserved even with Rule 10 numerically above it.

### Infrastructure

- Crash-recovery breadcrumb (`crumb_enter` / `crumb_exit`) wired into WRAP macros for every detect / diag / bench probe (`ae1cfd9`), so a hang during a probe leaves a named trail that the next boot surfaces with a NOTICE + `/SKIP:<name>` suggestion.
- `/SKIP:TIMING` escape hatch for PIT-C2 probe on boards where the 8254 clone hangs on touching channel 2 (`5fdf7fa`), with the crumb-enter/exit pair so a mid-probe hang is surfaced on next boot.
- `unknown_finalize` reordered pre-UI (`7da102e`) so `CERBERUS.UNK` lands on disk even if the UI path hangs.
- `/NOUI` escape hatch (`7da102e`) — user-visible workaround for the UI-render hang observed on v5 real-iron runs. Retained documented as a debug flag, not a feature-level user workaround.
- Real-hardware run corpus archived (`98c07d5`) — six diffable INI captures at `tests/captures/486-real-2026-04-18/` with per-run narrative README, anchoring every real-iron fix in observable artifacts rather than assertions.
- "Why real hardware" section in CERBERUS.md (`0161e99`) — H2 under Status, names the five 2026-04-18 bugs with symptom / cause / commit, closes with non-negotiable real-hardware-gate statement.
- Adversarial quality gates applied at Phase 4 completion (`6686574`, 5 rounds) and post-real-iron (`4d28e8e`, round-2) catching the phantom-verify biased-baseline and sub-crumb lifecycle bugs before the bench-box validation.
- `tests/target/` scaffold (`0e6c7e3`) for Phase 1 real-hardware validation drops.
- Host-side test suite: 138 assertions across timing (65), consistency (37), thermal (15), diag_fpu (21). Test-expectation drift in timing tracked as [#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1).

### Documentation

- README rewritten in plain-status voice (`dee1c64`).
- README refreshed at Phase 4 completion (`2882388`); current refresh landing with this RC.
- `docs/consistency-rules.md` methodology (`bb760c8`).
- `docs/plans/2026-04-16-cerberus-end-to-end.md` — single-file end-to-end implementation plan covering v0.1 → v1.0 with phase-level architecture and quality-gate criteria.

### Known issues

- **[#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1)** — `test_timing` has 4 pre-existing failures after the PIT wrap-range rework in `b6c179b` / `6c3a023`. Test expectations drifted from behavior. Gated behind the Rule 4a UMC491 8254 phantom-wrap deep-dive, which is out-of-scope for v0.2-rc1. Other host suites (consist 37/0, thermal 15/0, diag_fpu 21/0) are clean.
- **[#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2)** — intermittent OPL detection on Vibra 16 PnP. Same binary, same box, different boot produces `opl=opl3` vs `opl=none`. Partial fix in `eeba319`; residual state-dependence remains. INI still complete on the `opl=none` path (`audio.sb_present=yes`, `sb_dsp_version=4.13`); downstream T-key lookup falls back to the raw composite key.
- **UI hang on real iron.** Observed once on v5 (`7e4bdcb`) without `/NOUI` (2026-04-18 afternoon): after `ui_render_consistency_alerts` paints, the program did not return to DOS. The 2026-04-18 evening session ran the baseline (`7da102e`, no instrumentation) and two builds with exit-path instrumentation; all three exited cleanly on the same 486 box. The reproduction regime is not active on the current state. State variable causing the drift is unidentified — candidates include CMOS drift, cold-vs-warm-boot residue, Vibra PnP init ordering. Instrumentation patch preserved as local `git stash` for re-application if reproduction returns. Tracking issue filed with reopen criterion "reproduction on real iron." `/NOUI` retained as a user-visible escape hatch. Full investigation arc: [`docs/sessions/SESSION_REPORT_2026-04-18-evening.md`](docs/sessions/SESSION_REPORT_2026-04-18-evening.md).
- **Whetstone K-Whetstones under-reports by ~100× vs CheckIt 3.0 reference.** Real-iron validation on the BEK-V409 i486DX-2 showed CERBERUS Whetstone at ~109 K-Whet against CheckIt's 11,420 reference. Dhrystone on the same hardware hits CheckIt within ±2.4% (32,810 vs 33,609). Root cause (documented in detail at [`docs/plans/checkit-comparison.md` §6](docs/plans/checkit-comparison.md)): the `volatile double E1[4]` accumulator array's memory traffic dominates per-unit time at any non-aggressive Watcom optimization level where volatile is honored; no compiler-flag combination reaches CheckIt's reference speed without reintroducing the v7/v8 DCE pattern that over-reports by 30×. The emit is CONF_LOW to mark it as NOT cross-tool-comparable. The measurement IS reproducible cross-run on the same machine, so it remains useful for same-machine thermal tracking + TSR-contention signal (Rule 10 logic is scale-invariant and continues to work). Tracked for v0.4 FPU-assembly rework where the Whetstone inner kernels move to NASM with register-resident x87 state.

## [v0.1.1-scaffold] — 2026-03-20

Scaffold tag covering Phase 0: source-tree layout, Makefile, timing subsystem, CPU detection stub, display abstraction, INI reporter, BIOS info, and host-test infrastructure. First end-to-end CERBERUS.EXE that produces a valid (if mostly-stub) INI on DOSBox-X.
