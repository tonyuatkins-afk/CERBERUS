# CERBERUS Measurement Methodology

This document records the measurement method, confidence rationale, and known failure modes for every result CERBERUS reports. Added to as each Phase 1 / 3 subsystem is implemented.

## Timing

- PIT Channel 2 gate-based measurement, ≈838ns resolution
- `timing_ticks_to_us` uses `unsigned long` throughout to avoid the 16-bit overflow trap
- Rollover compensation: PIT counts DOWN, so `stop > start` indicates wrap
- XT-clone PIT C2 sanity probe runs at `timing_init()`; fallback to BIOS tick counter flags all results LOW confidence

## Pending subsystems

- CPU detection (Task 1.1)
- FPU detection (Task 1.2)
- Memory detection (Task 1.3) — INT 15h AX=E820h deliberately excluded (ACPI-era, not on pre-Pentium BIOSes)
- Cache detection (Task 1.4)
- Bus detection (Task 1.5)
- Video detection (Task 1.6)
- Audio detection (Task 1.7)
- BIOS info (Task 1.8)

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
