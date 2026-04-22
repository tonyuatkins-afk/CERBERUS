# CERBERUS Measurement Methodology

This document records the measurement method, confidence rationale, and known failure modes for every result CERBERUS reports. Updated through v0.8.0-M3 (2026-04-22).

## Timing

- PIT Channel 2 gate-based measurement, ~838ns resolution
- `timing_ticks_to_us` uses `unsigned long` throughout to avoid the 16-bit overflow trap
- Rollover compensation: PIT counts DOWN, so `stop > start` indicates wrap
- XT-clone PIT C2 sanity probe runs at `timing_init()`; fallback to BIOS tick counter flags all results LOW confidence
- v0.7.1: `timing_stats_t` accumulator for repeat-with-jitter measurement; `timing_confidence_from_range_pct` maps jitter to CONF_HIGH/MEDIUM/LOW bands
- v0.7.1: RDTSC backend gated on CPUID leaf 1 EDX bit 4 via `cpu_has_tsc()`; `timing_emit_method_info()` writes `timing.method` and (on RDTSC paths) `timing.cpu_mhz` calibrated via a 4-BIOS-tick window

## Why Whetstone is not in 0.8.0 (added 2026-04-21)

Stock 0.8.0 binaries compile the Whetstone kernel (`bench_whet_fpu.asm` + `bench_whetstone.c`) but the dispatcher suppresses emit. `bench.fpu.whetstone_status=disabled_for_release` is written; no `bench.fpu.k_whetstones` row is produced. Build with `wmake WHETSTONE=1` to re-enable emit for research work.

**Justification.** The Curnow-Wichmann Whetstone kernel has never produced a value in the published reference range on CERBERUS's validation platform. Pre-asm Watcom C kernel (v0.4): ~109 K-Whet on BEK-V409 (486 DX-2-66). Published reference envelope for that CPU: 1,500-3,000 K-Whet. Gap: ~10-30x low. The v0.5.0 asm rework was expected to close the gap but has not been validated to do so; on real iron one Whetstone unit costs 50-100 ms where research estimated 1-3 ms, a 30-50x per-unit-cost anomaly that has not been root-caused across multiple investigation sessions (v0.5.0 initial asm, v0.7.0 revisit, v0.7.2 timeout cap).

A trust-first release cannot ship a value 10x or more out of range regardless of confidence label. The `CONF_MEDIUM` escape hatch does not save it; community users read numbers, not confidence. The homage research (`docs/research/homage/checkit-whetstone-version.md`) already eliminated cross-tool comparability with CheckIt's "Whetstones" (a custom synthetic, not a Curnow port). The authentic reference target is the published 1,500-3,000 K-Whet range, which CERBERUS has not yet hit.

**What replaces Whetstone in 0.8.0.** `bench_fpu.c` remains the aggregate FPU-throughput metric (emits `bench.fpu.ops_per_sec`, ~1.17M on BEK-V409, plausible and reproducible). The v0.7.1 FPU behavioral fingerprint (infinity mode, pseudo-NaN, FPREM1, FSIN) plus 0.8.0 M2 additions (FPTAN, rounding-control cross-check, precision-control cross-check, IEEE-754 edge-case suite, exception-flag roundtrip) cover FPU correctness and semantics orthogonally to throughput. The FPU story is complete without Whetstone.

**Why the kernel stays compiled.** The build flag gates emit, not compilation. Every build compiles `bench_whet_fpu.asm` and `bench_whetstone.c`, every build runs host tests against them. Removing the kernel would invalidate the issue #4 archaeological trail and discard the opcode-level FPU exercise (FSIN / FCOS / FPATAN / FSQRT / F2XM1 / FYL2X) no other code path covers. The cut is cleanly reversible in 0.9.0 if the per-unit-cost anomaly is root-caused.

**0.9.0 direction.** Replace Whetstone with per-instruction microbenchmarks (FADD / FMUL / FDIV / FSQRT / FSIN / FCOS cycle counts). Methodologically stronger than a single synthetic. Documented in 0.8.0 plan §5 "Deferred to 0.8.1 or 0.9.0".

## End-of-run crumb chain (EMM386 #05 mitigation)

CERBERUS's end-of-run path runs through a fixed sequence of `crumb_enter` / `crumb_exit` pairs around every significant post-consistency call (upload_populate_metadata, report_write_ini, upload_execute, unknown_finalize, ui_render_*, fflush, display_shutdown, return). These pairs exist for crash-recovery breadcrumb reasons AND for a second, load-bearing reason: they are the workaround for a real-iron EMM386 #05 "unrecoverable privileged operation" crash observed on BEK-V409 (486 DX-2-66 + AMI BIOS 11/11/92 + DOS 6.22 + HIMEM + EMM386) in the v0.7.1 → v0.7.2 session.

**What each pair costs in practice.** `crumb_enter` issues INT 21h AH=3Dh (open), AH=40h (write), AH=68h (`_dos_commit`, DOS 3.3+ explicit FS buffer flush), AH=3Eh (close). `crumb_exit` issues INT 21h AH=41h (unlink). The whole round trip is on the order of milliseconds on a 486-class machine.

**Working hypothesis.** One or more of these INT 21h calls (most likely `_dos_commit` via INT 21h AH=68h) resets V86-monitor state or drains a pending real-mode ↔ V86 transition that the end-of-run path otherwise accumulates beyond EMM386's tolerance. The precise triggering instruction that EMM386 flags as privileged has not been isolated. Without a BEK-V409 reproduction with KERNEL-mode debugging access to the V86 monitor, the root cause cannot be pinpointed.

**Removal-at-a-time investigation protocol (for a future session with BEK-V409 access).** Use a scratch branch so the crash is not accidentally re-introduced to main. Procedure:

1. Confirm baseline: run stock CERBERUS on BEK-V409 with the crumb chain intact. Expect clean exit to DOS. Any EMM386 #05 here is a regression unrelated to the chain.
2. Remove the pair around `upload_populate_metadata` only. Rebuild. Run. Cold reboot. Run again (the bug does not always surface on warm boot). If the crash returns, the pair is load-bearing for that site; restore. If not, the pair is a safety-belt; mark for optional removal.
3. Repeat step 2 individually for each other site: `report_write_ini`, `upload_execute`, `unknown_finalize`, `ui_render_*`, `fflush`, `display_shutdown`.
4. The site whose pair absence most consistently triggers the EMM386 #05 crash is the root cause. Instrument further around that site (single-step through the instructions EMM386 traps on, using the DOS debugger equivalent of `DEBUG.EXE` plus EMM386's LOG= output if available).
5. Replace the coincidental workaround with an explicit fix (a single `_dos_commit(1)` on stdout at the right point, or a specific INT 21h safety call) and re-run the full investigation.

**Why the coincidental workaround ships.** A machine-rebooting exit defect is a ship-blocker for a trust-first release. The crumb chain is independently justified (breadcrumb crash-recovery is a real value-add for flaky vintage hardware), and the fix is reliable on BEK-V409. Shipping 0.8.0 with a known-good but unexplained workaround is better than shipping either (a) a known-broken exit path, or (b) a delayed release waiting for root-cause that may take a full separate investigation session. The code is annotated with a do-not-remove warning (`src/main.c` "END-OF-RUN CRUMB CHAIN: LOAD-BEARING" comment) so no future refactor accidentally un-does the workaround. The removal-at-a-time investigation is filed for 0.8.1 or later.

## Final-exit `_exit` bypass (M1.7 resolution)

Even with the end-of-run crumb chain in place, the first M1 capture on BEK-V409 (2026-04-21 20:30) showed a post-UI hang: `ui_render_summary` rendered cleanly, the user pressed Q to quit the scrollable viewport, and the program then hung requiring hardware reset. The crumb trail retained `CERBERUS.LAS=main.return` — definitively localizing the hang to Watcom's libc teardown (the code that runs after `return exit_code` in `main`: atexit chain, FPU state cleanup, stdio close, `_NULL` signature check).

**Fix (`src/main.c`)**: the final `return exit_code` is replaced by:

```
crumb_enter("main.return");
crumb_exit();
_exit((int)exit_code);
```

`_exit()` in Watcom's stdlib.h calls INT 21h AH=4Ch directly, bypassing the libc teardown that was hanging. All resources this program owns are already released by explicit calls before this point: `fflush(stdout); fflush(stderr)` runs under the `main.flush` crumb, `display_shutdown()` runs under `main.display_shutdown`, `fopen`/`fclose` pairs in `report_write_ini` and `unknown_finalize` complete their own lifecycle. CERBERUS registers no atexit handlers; there is nothing useful for libc to do at exit.

The `crumb_exit()` before `_exit()` unlinks `CERBERUS.LAST` (`.LAS` on disk due to DOS 8.3 truncation) on successful exit so the next run's `crumb_check_previous` does not emit a spurious "previous run hung during main.return" notice.

**Verified on BEK-V409 2026-04-21 20:39**: clean exit to DOS prompt after Q-exit, no reset required, `CERBERUS.LAS` absent post-run. Full INI captured at `tests/captures/486-BEK-V409-2026-04-21-m1-exit/capture.ini`.

**Post-fix observation**: with the exit path now running cleanly, Watcom's libc NULL-assignment canary fires and prints "*** NULL assignment detected" above the DOS prompt. This is a real latent defect in CERBERUS (some code somewhere writes to a near-NULL address, corrupting the first 32 bytes of DGROUP's `_NULL` segment) that was previously masked by the hang. It is the same underlying defect as the intermittent `[bios] dree=` BSS stomp: both are caused by writes that land at or near DGROUP offset 0, and whether the stomp overflows into adjacent CONST (corrupting bios.date string literals) depends on the specific probe-path taken (`opl=opl3` path is shorter and stays within `_NULL`; `opl=none` fallback path is longer and overflows into CONST). DOSBox-X does not reproduce the symptom. Investigation is filed for M2 via `docs/quality-gates/M1-gate-2026-04-21.md` W6 + W4; the removal-at-a-time protocol from this section's crumb-chain investigation applies analogously to the NULL-write investigation.

## Cache characterization probes (M2.1)

The cache-sweep measures bytes/sec for a fixed-stride read over a working set. Six stride points (`32, 64, 128, 256, 512, 1024` bytes) sweep a 4 KB buffer kept resident in L1 on all target CPUs. The detector emits the smallest stride at which throughput transitions from "every access hits a new line" to "multiple accesses per line" as the inferred line size.

- **Stride=128 rationale**: Pentium-era parts split at 32-byte lines, while Pentium-Pro+ split at 64. Without a 128-byte stride the plateau between 64 and 256 is under-sampled and the detector cannot distinguish Pentium from later Intel. Adding stride=128 makes `line_bytes=32` detectable by the plateau signature `K(32) < K(64) ~= K(128)`; `line_bytes=64` shows `K(32) < K(64) < K(128) ~= K(256)`.
- **Inference bounds**: `bench_cc_infer_line_bytes()` accepts 5 or 6 stride points for backward compatibility with pre-M2 captures. When exactly 5 points are present (no stride=128), the detector falls back to the older heuristic and tags the output as `inferred_from_5point_sweep=1` so consistency rules can downgrade confidence.
- **Working set**: the 4 KB buffer fits in the smallest 486-class L1 (8 KB) with margin for code + working variables. Measuring at this buffer size means the sweep characterizes L1 throughput specifically; L2 detection via larger buffers is deferred to 0.8.1 (see `docs/research/Cache Test Research.md` for the 64/128/256 KB extension plan).

## FPU behavioral fingerprint (M2.2–M2.4, M2.6)

The fingerprint is a 5-axis probe that discriminates 8087 / 80287 / 80387 / 486-integrated / Pentium+ FPUs by exercising documented control-register and edge-case behaviors. Each axis is a one-instruction or short-sequence probe chosen so the observable differs across generations and cannot be inferred from CPUID alone.

### M2.2 FPTAN pushes 1.0 (research gap I)

FPTAN on 8087 and 80287 computes tan(angle) and leaves a single value on the stack. FPTAN on 387+ pushes tan(angle) AND a trailing 1.0 (the implicit denominator for a following FPATAN that would recover the original angle). The probe:

```
FLDPI                  ; push π
FLD1                   ; push 1.0
FDIV                   ; st(0) = π/4 * something... use a clean 0.5
FLD <angle=0.5>        ; simpler: push 0.5 directly
FPTAN
FCOMP <1.0>            ; compare st(0) to 1.0; on 387+ st(0) IS 1.0
FSTSW ax
test ah, 0x40          ; ZF bit of C3 == equal
```

`diagnose.fpu.fptan_pushes_one = yes` => 387 or later. `no` => 8087/287 (stored as the partial-reduction remainder, not 1.0). The probe is gated on `cpu_has_fpu` to skip no-FPU systems, and the angle=0.5 avoids the FPTAN input-range restriction (|angle| < π/4 on 8087/287).

### M2.3 Rounding-control cross-check (research gap J)

All x87 parts honor the RC field of the control word (bits 10:11 of FCW), but the exact rounding-to-integer behavior differs subtly for negative numbers. The probe stores two fixed test values (`1.5` and `-1.5`) as 16-bit integers via FISTP at all four rounding modes (00=nearest-even, 01=down, 10=up, 11=truncate-toward-zero) and tabulates the 8 results.

Canonical 387/486/Pentium result table:

| mode     | FISTP(1.5) | FISTP(-1.5) |
|----------|-----------:|------------:|
| nearest  |          2 |          -2 |
| down     |          1 |          -2 |
| up       |          2 |          -1 |
| truncate |          1 |          -1 |

Any deviation => `diagnose.fpu.rounding_modes_ok=no` and the 8 raw integers are emitted as sub-keys (`rc_near_pos`, `rc_near_neg`, …) for investigation. 8087/287 are identical to 387 for this probe so it does not discriminate generation, but it catches emulator stubs and damaged FPUs.

### M2.4 Precision-control cross-check (research gap K)

The PC field (bits 8:9 of FCW) selects 24-bit (single), 53-bit (double), or 64-bit (extended) significand for arithmetic. The probe stores 1.0/3.0 as a 10-byte tword (FSTP tbyte) at each precision, so the low mantissa bits differ. The three tword results must be distinct: if they match, PC is ignored or clamped (typical of software emulators). `diagnose.fpu.precision_modes_ok=yes|no` with the three 10-byte hex strings as sub-keys.

### M2.6 Exception-flag roundtrip (research gap M)

Each of the 6 x87 exceptions is triggered deliberately, observed in the status word, and cleared via FCLEX:

| exception | trigger                           | status bit |
|-----------|-----------------------------------|-----------:|
| IE (invalid)  | FSQRT(-1.0)                    | 0 |
| DE (denormal) | FLD denormal then FADD 1.0     | 1 |
| ZE (zerodiv)  | FLD 1.0 / FLD 0.0 / FDIV       | 2 |
| OE (overflow) | FLD MAX_FLOAT / FMUL MAX_FLOAT | 3 |
| UE (underflow)| FLD MIN_DENORM / FMUL 0.5      | 4 |
| PE (precision)| FLD 1.0 / FLD 3.0 / FDIV       | 5 |

Before each probe: `FCLEX` (clear SW). After each probe: `FSTSW AX` and check the relevant bit; then `FCLEX` again to avoid cross-contamination. All 6 bits must round-trip for `diagnose.fpu.exception_flags_ok=yes`. Failures often indicate a 287 with an external 387 emulator that swallows some exception classes, or a Weitek coprocessor responding on partial FPU port ranges.

Control-word mask bits (IM/DM/ZM/OM/UM/PM, bits 0–5) are cleared via FLDCW before each probe so exceptions are not masked silently; the mask is restored to 0x037F (default-masked-all) after the roundtrip so subsequent benches are not sensitive to our probe state.

## Memory pattern diagnostics (M2.7)

`diag_mem` runs a pattern-write, readback, comparison loop over a configurable-size buffer (default 64 KB conventional, extended to XMS when available). Two new patterns in M2.7:

- **Checkerboard (0x55 / 0xAA)**: alternating nibbles within each byte exercise adjacent bit-cells in opposite directions. A stuck-short between adjacent DQ lines reads back as 0x00 or 0xFF; a stuck-at fault reports a consistent bit offset across all addresses.
- **Inverse checkerboard (0xAA / 0x55)**: same pattern offset by one byte. Running both catches address-decode faults where the byte at offset N gets the pattern intended for N+1.

Both patterns complement the existing walking-ones (`0x01`, `0x02`, `0x04`, …) and walking-zeros (`0xFE`, `0xFD`, …) patterns. Pattern execution is always paired: write-P, read-verify, write-~P, read-verify, in that order, so a stuck-at-0 fault reports a single P failure while a true data-line short reports mirror-image errors on both passes.

Deferred to 0.8.1 and tracked against `docs/research/Memory Test Research.md`: March-C+ algorithm, parity-error deliberate-injection via INT 2 NMI path, and XMS/EMS boundary stress.

## CGA snow safety gate (M3.1)

CGA video RAM at B800:0000 is dual-ported: the CRTC reads it during active scan, and any CPU write during active scan produces visible snow (the CPU's bus cycle wins, corrupting what the CRTC emits for the current character cell). The fix is to write only during vertical-retrace or horizontal-retrace windows, detected via the 3DAh status port bit 0 (1 = retrace in progress).

`tui_wait_cga_retrace_edge()` in `src/core/tui_util.c` polls 3DAh until it observes a retrace-inactive → retrace-active transition (the "edge"). Two sequential reads guarantee we land inside a fresh retrace window rather than catching the tail of the previous one. Non-CGA adapters (MDA, EGA, VGA) short-circuit the wait because they are not dual-ported in the same way and do not snow.

Adapter tier is established once at `display_init()` via the adapter-tier waterfall (M3.7 below); all row-24 writes and full-screen updates gate on `tui_wait_cga_retrace_edge()` when `display_adapter_tier == CGA`. The cost on CGA is ~1 ms per call (one vertical-retrace interval at 60 Hz), which is invisible for the row-24 legend refresh and acceptable for help-overlay show/hide.

## Adapter-tier detection waterfall (M3.7)

Detection runs four probes in order and stops at the first unambiguous answer:

1. **INT 10h AH=1Ah (Get Display Combination Code)**: VGA-era BIOS returns a BL/BH pair identifying the active and alternate adapters. AL=1Ah on return means the call is supported; any other AL means the BIOS is pre-VGA and we fall through. When supported, BL directly maps to our tier (07h/08h = VGA, 04h/05h = EGA, 01h = MDA, 02h = CGA).
2. **INT 10h AH=12h BL=10h (Get EGA Info)**: EGA BIOS returns BH (monitor type) and BL (memory size) in a way that distinguishes EGA from CGA. Unused registers stay unchanged across the call on non-EGA BIOS; we preload a sentinel pattern and detect no-response by pattern-persistence.
3. **BIOS Data Area 40:49h (video mode) + 40:10h (equipment byte)**: the equipment byte bits 4:5 give the initial video mode (00=EGA/VGA-deferred, 01=CGA-40, 10=CGA-80, 11=MDA-80). Current mode at 40:49h cross-checks this.
4. **3BAh toggle probe**: writing to 3BAh and reading back discriminates MDA (port responds) from CGA (port does not). Final fallback.

Result is cached in `display_adapter_tier` for the rest of the run; all subsequent CGA snow gating, mono-attribute mapping, and 16-background-color probing reads from the cache.

## /MONO attribute mapping + 16-background-color probe (M3.5, M3.6)

CERBERUS uses a 5-role semantic palette: NORMAL, EMPHASIS, DIM, HIGHLIGHT, WARNING. Each role maps to a hardware attribute byte depending on display tier.

### Mono / /MONO-forced mapping

| role      | attribute | effect on mono    |
|-----------|----------:|-------------------|
| NORMAL    |      0x07 | white on black    |
| EMPHASIS  |      0x0F | bright white      |
| DIM       |      0x01 | underline         |
| HIGHLIGHT |      0x70 | reverse video     |
| WARNING   |      0xF0 | bright reverse    |

The 0x01 underline attribute is MDA-specific (the hardware literally underlines the character); CGA displays it as blue-on-black. For CGA composite monitors the distinction disappears. `/MONO` forces this table regardless of detected tier so users with amber/green-phosphor monitors attached to color cards get the semantic-correct rendering rather than color-coded rendering their phosphor cannot express.

### 16-background-color enable on EGA/VGA

By default EGA/VGA put the top attribute-byte bit in the "blink" role (character blinks). `INT 10h AX=1003h BL=00h` reassigns that bit to "background-intensity" instead, giving 16 background colors (0x00–0xF0) rather than 8 + blink. CERBERUS issues this at `display_init()` on EGA and VGA tiers so WARNING's 0xF0 renders as bright-bg rather than blinking-bg. The call is a no-op on MDA and CGA (CGA's blink semantics are immutable), so we gate it on tier.

Known caveat: certain SVGA BIOSes (notably some ET4000 variants and early S3 Trio boards) silently drop AX=1003h and keep blink semantics. Detection would require reading back the internal attribute-controller register, which is not portable. Accepted as documented limitation; users seeing blinking WARNING can pass `/MONO` as a workaround.
