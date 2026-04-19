# Session Report — 2026-04-18 Evening

**Operator:** Tony Atkins
**Duration:** ~4 hours (afternoon real-iron gate continued into evening)
**Scope:** UI-hang root-cause investigation, audio DB mixer extension, v0.2-rc1 prep.

---

## Commits landed (full day, six total)

### Afternoon

- **`eeba319`** — Real-iron Task 1.10 gate, five real-hardware bugs fixed from the BEK-V409 / i486DX-2 / S3 Trio64 / Vibra 16S bench box:
  1. HIMEM.SYS intercepting INT 15h AX=E801h (ext_mem=0 → 63076 via XMS entry point).
  2. S3 Trio64 BIOS "IBM VGA" string collision (CR30 chip-ID probe authoritative).
  3. Vibra DSP status port at base+0x0E, not base+0x0A.
  4. OPL 0x388 mirror gating under CTCM (BLASTER-base+8 primary, 0x388 fallback).
  5. UMC491 8254 latch-race biased phantom wraps (upper-bound + low→high shape check + 25% post-hoc rejection).
- **`7e4bdcb`** — Phase 4 Rule 4b (`cpu_ipc_bench`), audio DB T-key split (splits opl3:040D into T6 SB16/Vibra, T8 AWE32, etc.), bench_memory kb_per_sec precision fix, REP LODSB read helper.
- **`7da102e`** — `/NOUI` escape hatch + `unknown_finalize` moved pre-UI so `CERBERUS.UNK` lands even if the UI path hangs. Surfaced tonight's UI-hang symptom; not a root-cause fix.

### Evening

- **`98c07d5`** — `tests/captures/486-real-2026-04-18/` — six diffable INI captures (v1 through v6-noui) with a narrative README covering bench hardware, the iteration arc, and a worked v1→v3 diff that anchors the HIMEM fix in observable artifacts.
- **`0161e99`** — CERBERUS.md "Why real hardware" section naming the five 2026-04-18 bugs by specific symptom / cause / commit hash, closing with the non-negotiable real-hardware-gate statement. Unnumbered H2 to avoid renumbering §4–§17 and breaking the "Section 10" cross-reference.
- **`c22e886`** — CT1745 mixer-chip probe at BLASTER-base+4/+5 reg 0x80, `audio.csv` gains a `mixer_chip` column (CT1745 seeded for SB16 CT2230/CT2290 and Vibra 16S T6 family; all others `unknown`), new consist Rule 7 (`audio_mixer_chip`) PASS/WARN/FAIL. Host tests +5 scenarios; 4 pre-existing timing failures unchanged and tracked as issue #1.

Outstanding uncommitted tree: `Makefile`, `src/core/ui.c`, `src/main.c` — UI-hang instrumentation, deliberately held pending baseline-control resolution.

---

## UI hang investigation — full arc

### Hypothesis (afternoon, commit 7da102e body)

> "A future session should add crumb_enter/exit around each ui_render_* call so the sub-phase hang is pinpointable."

Frame offered: either a `getch` / `_getch` / `bioskey(0)` blocking-input call, or an `atexit` handler referencing a DOS handle already released.

### Refutation (evening, code review)

Inventory of the entire post-`thermal_check` exit path via direct Read + Grep:

- **Zero keyboard-input calls in the source tree.** No `getch`, `_getch`, `bioskey`, `kbhit`, `getchar`, `scanf`, `int 16h`, or `int 21h` AH=01/07/08 anywhere.
- **Zero `atexit` / `onexit` registrations.** Watcom's C runtime performs its own implicit-exit cleanup but no user-supplied handlers are wired.
- `display_shutdown()` is a single assignment (`current_attr = ATTR_NORMAL`) — no I/O, no interrupt dispatch, no hook unwind.
- Only stdio-flushes in the exit path: two unconditional `fflush(stdout)` / `fflush(stderr)` calls that run regardless of `/NOUI` state, so they cannot discriminate between the two paths.

Conclusion: **code review produces no candidate** for the stated hypothesis. Picking one to "fix" arbitrarily would violate the no-guessing quality-gate posture. Surfaced this to the operator; approved deviation from the step-4 "pick and fix" literal order to instrument-first.

### Instrumentation (uncommitted)

Chain-style crumbs threaded through the exit path — each `crumb_enter()` overwrites the previous file so the last-written name identifies the phase that didn't complete:

- `exit.consist_check.done` → `exit.thermal_check.done` → `exit.report_write_ini.done`
- `exit.unknown_finalize.enter` / `.done`
- `exit.ui.summary.enter` / `.row.N` (per-row) / `.done`
- `exit.ui.alerts.enter` / `.box.N.start` / `.box.N.rendered` / `.done`
- `exit.upload.start` / `.done` (conditional)
- `exit.fflush.stdout.pre` / `exit.fflush.stderr.pre`
- `exit.display_shutdown.pre` / `.post`
- `exit.return.imminent`

Final `crumb_exit()` before `return` wipes `CERBERUS.LAST` on clean exit. File-present-after-run ⇒ hang location == contents.

EXE grew from 78,494 to 79,184 bytes (+690). DGROUP 45,600 → 47,984 (under the 50,000 cap). Deterministic build verified post-stash-pop.

### Masking (two clean runs)

**Run 1** (2026-04-18 evening):
- Audio state: `opl=opl3`, `detected=Sound Blaster 16 or Vibra 16S` — OPL detected, full T-key lookup.
- Result: `CERBERUS.LAST` absent → clean `return` from `main`.

**Run 2** (same session, different cold boot):
- Audio state: `opl=none`, `detected=none:040D` — **identical** to v5-hung (the canonical hanging capture).
- Same two consistency WARNs render (`timing_self_check`, `cpu_ipc_bench`) → same UI workload.
- Result: `CERBERUS.LAST` absent → clean `return` from `main`.

Run 2 is the controlled comparison. Same alert-box workload as v5-hung, instrumentation present, clean exit. Strong evidence the crumb path's forced DOS FAT flush (via `_dos_commit`, ~30–90 ms across ~15 exit-path crumbs) masks whatever buffer-state interaction triggered the hang.

### Baseline control — Outcome B

`CERBBASE.EXE` (7da102e tree, no instrumentation, no mixer, md5 `1a6383b12656cc287c44dff8142ac7a0`, 78,494 bytes) ran once on the 486 after cold boot and restarted FTPSRV. Pulled artifacts:

- `CERBERUS.LAST` **absent** → final `crumb_exit()` unlinks ran, clean `return` from `main` without instrumentation.
- `CERBERUS.INI` complete, 2,229 bytes, `run_signature=b8d9da282ba17e48`.
- Audio state: `opl=opl3`, `detected=Sound Blaster 16 or Vibra 16S` (OPL detected fully this boot — issue #2 landed on its happy path).
- Consistency WARNs: `timing_self_check` + `cpu_ipc_bench`, identical to v5-hung's two-WARN-box UI workload.

**Conclusion: the hang is not reproducing on the current 486 state.** Baseline + both instrumented runs all exit cleanly. This rules out the instrumentation-masking hypothesis — the bug itself is absent, not hidden. The 2026-04-18 afternoon reproduction on commit `7e4bdcb` (v5-hung capture) was valid at the time but is no longer observable today. State variable causing the drift is unidentified: candidates include CMOS battery drift, warm-boot residue vs cold-boot state, Vibra PnP init ordering, or some third unknown.

Decision per session-2026-04-18 branch plan (outcome B): investigation tabled, instrumentation preserved unapplied as a local stash, issue filed for future reproduction. `/NOUI` retained as user-visible escape hatch. Not a v0.2-rc1 blocker.

---

## Instrumentation disposition

The exit-path crumb work (~16 tag definitions in `main.c`, per-row + per-box crumbs in `ui.c`, Makefile dep updates for `crumb.h`) is preserved as a local `git stash` entry labeled `ui-hang-instrumentation 2026-04-18`. The diff stands ready to reapply if the hang reproduces in a future session. Not committed tonight — landing speculative fix code against a bug that doesn't currently reproduce would violate the "no silent failures" quality-gate posture: there's nothing to verify the fix against.

---

## Consistency engine state — nine rules live

| # | Rule | Landed | Detects |
|---|---|---|---|
| 1 | `486dx_fpu` | pre-tonight | 486DX CPU must report integrated FPU |
| 2 | `486sx_fpu` | pre-tonight | 486SX CPU must NOT report integrated FPU |
| 3 | `386sx_bus` | pre-tonight | 386SX must be on ISA-16+ |
| 4a | `timing_independence` | pre-tonight | PIT C2 vs BIOS tick within 15% |
| **4b** | `cpu_ipc_bench` | **`7e4bdcb` today** | bench IPC within DB-seeded CPU empirical range |
| 5 | `fpu_diag_bench` | pre-tonight | FPU diag PASS iff bench has result |
| 6 | `extmem_cpu` | pre-tonight | extended memory implies 286+ |
| **7** | **`audio_mixer_chip`** | **`c22e886` today** | audio DB `mixer_chip` column agrees with hardware probe |
| 9 | `8086_bus` | pre-tonight | 8086-class must be on ISA-8 |

Rule-slot-numbering note: session brief offered "Rule 8 or extend Rule 5" for the mixer rule. Rule 5 is `fpu_diag_bench` (wrong domain — extending would fuse unrelated concerns). Rule 8 is reserved in `consist.c`'s block-comment footer for the future cache-stride vs CPUID-leaf-2 cross-check. Landed as Rule 7 (next free slot) with the rationale documented inline. Rule 8 still reserved, no collision.

Rule 7 probe classifies the byte from mixer register 0x80 (Interrupt Setup):
- Valid low-nibble IRQ bitmap (bits 0-3 populated, high nibble clear) → `CT1745`
- Open-bus (0xFF), index-echo (0x80), or zero → `none`
- Anything else → `unknown`

DB seeded conservatively per session brief: `opl3:0404`, `opl3:0405`, `opl3:040D:T6` → `CT1745`. All other 28 rows `unknown` pending real-hardware verification. Rule silent on `unknown+none` pairs; WARNs on `unknown+CT1745` (invites DB contribution); FAILs on explicit-chip mismatch.

---

## Issues filed

- **[#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1)** — `test_timing` 4 failures after PIT wrap-range rework (`b6c179b`, `6c3a023`). Pre-existing on `main` at session start; reproduced via `git stash` on the untouched tree. Not introduced by tonight's work. Tracked for resolution during Rule 4a deep-dive work (UMC491 8254 phantom-wrap root cause) which is explicitly out-of-scope for this session.
- **[#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2)** — intermittent OPL detection on Vibra 16 PnP. Same binary + same box + different boot → `opl=opl3` vs `opl=none`. Partial fix in `eeba319` (BLASTER-base+8 primary, 0x388 fallback) made detection work *sometimes*; this ticket covers the residual state-dependence. Deferred per session's not-tonight list.
- **UI hang** — filed as issue with reproduction-returns reopen criterion. Instrumentation patch stashed locally. If the hang surfaces in a future real-iron session, reapply the stash and run the `_dos_commit`-surgical experiment on a live-reproducing build.

---

## Open items for next session

1. **UI hang** — reopen criterion is reproduction on real iron. If the hang surfaces in a future session, restore the stashed instrumentation (`git stash list | grep ui-hang-instrumentation`), rebuild, run once to confirm the hang returns WITH instrumentation (to validate the stash is still applicable), then proceed with the `_dos_commit` surgical experiment. If months pass without reproduction, consider dropping the stash after capturing the diff as a standalone patch file in `docs/plans/attic/`.
2. **Rule 8 candidate** — `cache_stride_vs_cpuid_leaf2` still reserved in `consist.c` footer. Needs cache-bench (Task 3.3) and CPUID leaf-2 decode to land first.
3. **Three-pane summary UI polish** — mentioned in README status table as "still pending". Lives in the same rendering path as the UI hang, so blocked on hang disposition.
4. **Audio DB maturation** — 28 rows still marked `mixer_chip=unknown`. Each SB-family card on real hardware that passes through the tool triggers a Rule 7 WARN inviting a contribution. Opportunistic DB growth rather than a single focused pass.
5. **`test_timing` repair (issue #1)** — concurrent with the Rule 4a deep-dive or as its prerequisite. Not a v0.2-rc1 blocker.
6. **OPL intermittency root-cause (issue #2)** — needs probe instrumentation on real iron, multiple cold-boot cycles. Dedicated session rather than squeezed into a broader one.
7. **v0.2-rc1 tag** — waiting on (a) matrix resolution (this session) and (b) clean-commit-count verification. README status table and CHANGELOG updates landing alongside RC-prep commits; tag itself held.

---

*Report scope ends at baseline-result time. Post-baseline work (whichever branch) → next session.*
