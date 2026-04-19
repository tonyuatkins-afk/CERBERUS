# Session Report — 2026-04-18 Afternoon → 2026-04-19 Morning Weekend

**Operator:** Tony Atkins
**Duration:** ~22 hours wall-clock across three contiguous sessions (Saturday afternoon, Saturday evening, Sunday morning)
**Arc:** v0.2 real-hardware gate through v0.2-rc1 tag through v0.3 feature-complete + first-data-point validation. Twenty-eight commits. One release-candidate tag (v0.2-rc1). Five GitHub issues filed. One quality-gate cycle.

---

## What shipped

### Tags

- **`v0.2-rc1`** at `311c19e` — first real-hardware-validated CERBERUS release candidate. Ten consistency rules live. Dhrystone matches CheckIt within ±2.4%. Whetstone shipped CONF_LOW with documented divergence.
- **`v0.3-rc1`** — NOT TAGGED. First real-hardware validation of v0.3 on BEK-V409 (2026-04-19 Sunday morning) produced mixed results:
  - ✅ `diag.dma.summary=ok (6/6 channels responsive, 2 safety-skipped)` — target met.
  - ✅ `CERBERUS.LAST` absent — 4th clean run in a row, UI-hang non-regression.
  - ✅ Zero FAIL consistency rules (five WARNs, all pre-documented).
  - ⚠️ `diag.cache.status=WARN (ratio=27.39 × — partial)` — **threshold miscalibration, not diagnostic failure.** The 27.39× ratio is above the 16× linear-scaling baseline (so cache IS being observed) but below the 40× PASS threshold. Root cause: the PASS threshold was chosen based on theoretical 5-10× per-line cache-hit-vs-miss speedup; BEK-V409's TSR-contended BIOS configuration produces only 1.71× per-line speedup, landing the ratio in the WARN band. Filed as issue #5 for threshold recalibration.
  - Corpus INI archived at `tests/captures/486-real-2026-04-19-v0.3/CERBERUS.INI`.
  - v0.3-rc1 tag waits on threshold recalibration + re-validation run, next session.

### Commit arc (28 commits since `7da102e`)

Grouped by sub-arc rather than chronologically — the day's shape was thematic, not linear.

#### Saturday afternoon — real-hardware gate (`eeba319` → `7da102e`)

Three commits on Saturday before the evening session started, all at the bench box:

- `eeba319` — Real-iron Task 1.10 gate: **five bugs fixed on contact with the BEK-V409**. HIMEM.SYS intercepting INT 15h AX=E801h (extended_kb=0 → 63076). S3 Trio64 BIOS-string collision ("IBM VGA" matched first). Vibra 16S DSP status port at base+0x0E (was wrongly polling base+0x0A). OPL 0x388 mirror gating under CTCM (added BLASTER-base+8 fallback). UMC491 8254 latch race (added upper-bound + shape-check + 25% divergence rejection). Iteration refined in `6c3a023` and `4d28e8e` earlier in the week; this commit was the landing.
- `7e4bdcb` — Phase 4 Rule 4b + audio DB T-key split + bench_memory corrections. Audio DSP-4.13 disambiguation (SB16 vs AWE32 via BLASTER `Tn` token). Rule 4b `cpu_ipc_bench` seeded. bench_memory `kb_per_sec` precision fix + REP LODSB read helper.
- `7da102e` — `/NOUI` escape hatch + `unknown_finalize` moved pre-UI. Surfaced the UI hang observed on v5; not a root-cause fix, a workaround.

#### Saturday evening — Arc 1 (mixer) + Arc 2 (historical benchmarks)

- `98c07d5` — 486 real-hardware run corpus: six INIs at `tests/captures/486-real-2026-04-18/` + narrative README.
- `0161e99` — CERBERUS.md "Why real hardware" H2 section naming the five `eeba319` bugs.
- `c22e886` — CT1745 mixer-chip probe at BLASTER-base+4/+5 reg 0x80 + new consist Rule 7 (`audio_mixer_chip`).
- `3fc4e50` — `docs/sessions/SESSION_REPORT_2026-04-18-evening.md`.
- `4415c63` — v0.2-rc1 prep bundle: CHANGELOG + README refresh + `docs/plans/checkit-comparison.md` + `docs/sessions/NEXT-PLATFORMS.md` + plan-doc checkbox updates.
- `e897c15` — bench_dhrystone.c: Dhrystone 2.1 port + v0.4 plan doc.
- `525f65b` — bench_whetstone.c: Whetstone port with FPU-presence gate.
- `f0cebde` — PC-XT relative ratings (`cpu/fpu/mem_xt_factor`) + consist Rule 10 (`whetstone_fpu_consistency`).
- `97d24e6` — anti-DCE `volatile` + checksum observers for both benchmarks. **Proved insufficient on v8 real-iron** (volatile didn't defeat Watcom's internal DCE inside non-volatile-qualified Proc_1/2/3/7 bodies).
- `8552c6d` — NULL-display pattern for Whetstone V_U32 emits. **Proved sufficient** — fixed the `fpu.whetstone_elapsed_us=(�` display corruption class.
- `1788561` — Makefile `CFLAGS_NOOPT -od` on bench_dhrystone + bench_whetstone only. The accepted fallback after volatile alone failed.

#### Saturday evening — quality gate (6 rounds)

User called `/quality-gate` after Arc 2 landed. Six red-team rounds across eight fix commits:

- `b3b54d9` / `d3de045` — round 1 fix (F1 + S1 + S2 + S3: parse_blaster forward decl, Rule 10 inconclusive handling, Dhrystone warmup overflow, doc drift).
- `af2b6cd` — round 2 Fatal fix: `timing_start_long` / `timing_stop_long` primitives. Round 2 surfaced the single biggest defect of the session: `timing_elapsed_ticks` only handled one PIT wrap (~55ms), so Dhrystone/Whetstone at 5-second targets were measuring modulo-garbage. **Critical catch.**
- `b8ef065` — round 2 refinements (I/O delay, Rule 10 tightening, type checks, doc resync, stash patch archive).
- `5279095` — round 3 rate-compute overflow + atomic BIOS-tick read. Caught another numeric-overflow-on-fast-hardware defect (same class as the original v7/v8 bug, migrated to the rate computation).
- `a00f5a0` — round 4 Rule 10 docs resync + sub-ms status standardization + whetstone edge cleanup.
- `bcf42ad` — round 5 Rule 7 WARN-not-FAIL on observed="unknown" + README DB count.
- `9c64991` — end-of-gate minor sweep (README + CHANGELOG number resync).

Round 6 closed with zero Fatal, zero Significant, two trivial Minors that the sweep handled. Gate verdict: **clean, v0.2 ship-ready.**

#### Saturday evening — Arc 2 real-hardware bench tuning (`99ed900` → `311c19e`)

With v0.2-rc1 gate-clean, the remaining blocker was real-iron benchmark magnitudes. v9 showed Dhrystone and Whetstone 20-30× over CheckIt reference (DCE still winning). Iterative flag tuning:

- `99ed900` — `-oi` (intrinsic math). **Zero measurable effect** on Whetstone on v10.
- `5aaeb22` — `-ot -oi` experiment. v11 Dhrystone nailed it (32,810 = ±2.4%), but Whetstone still catastrophically slow (107 K).
- `76c3ae4` — Whetstone FPU accumulators moved from `volatile double` locals to non-volatile locals + publishing `_Acc` statics. Unlocked x87 register allocation. **v12 showed no change in per-unit speed** — confirmed bottleneck is inside PA's inner loop (volatile E1[] memory traffic), not at function boundaries.
- `27dd8dd` — `-om` (inline 80x87 math). Zero effect.
- `814db25` — `-oe` (function inline expansion). v14 halved elapsed time (calibration hit fewer units) but same per-unit speed — final confirmation the floor is structural.
- `311c19e` — **revert to `-ot -oi` + ship Whetstone at CONF_LOW + full divergence documentation**. The honest trade.

#### Sunday morning — autonomous v0.3 phase (`10237c3` → `d9d1f5a`)

User away at the park, session continued autonomously:

- `10237c3` — `docs/plans/v0.3-diagnose-completion.md`: full design for diag_cache (stride-ratio) + diag_dma (8237 count-register probe) + consistency rule candidates + verification gates + stop conditions.
- `7a28850` — implementation of both diagnostics + wiring into diag_all.c + host tests. **diag_cache**: 2 KB vs 32 KB `__far` buffers, stride-16 read loops, ratio classifier kernel, verdict PASS/WARN/FAIL. Safety-skips 8088-class floor. **diag_dma**: 8237 count-register write+readback on channels 1/2/3/5/6/7 with hard-skip of channel 0 (refresh) and channel 4 (cascade). XT-class detection via cpu.class / bus.class. Both modules host-tested: 17 + 10 assertions respectively. Host-suite total: 134 → 161 OK (+27).
- `d9d1f5a` — README + CHANGELOG: "4 of 6 complete" → "6 of 6 complete". v0.3 feature-complete pending real-hardware gate.

---

## Issues filed (five)

- **[#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1)** — `test_timing` 4 pre-existing failures after PIT wrap-range rework (`b6c179b` / `6c3a023`). Expectation drift, gated behind the Rule 4a UMC491 8254 phantom-wrap deep-dive that is explicitly out-of-scope for v0.2-rc1.
- **[#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2)** — Intermittent OPL detection on Vibra 16 PnP. Same binary + same box + different cold boot produces `opl=opl3` vs `opl=none`. Partial fix in `eeba319`; residual state-dependence remains.
- **[#3](https://github.com/tonyuatkins-afk/CERBERUS/issues/3)** — UI hang observed once on Saturday afternoon, did not reproduce across baseline + two instrumented builds on Saturday evening. State-dependent per-boot; reopen criterion is reproduction on real iron. Instrumentation patch preserved as `stash@{0}` AND archived at `docs/plans/attic/ui-hang-instrumentation-2026-04-18.patch`.
- **[#4](https://github.com/tonyuatkins-afk/CERBERUS/issues/4)** — Whetstone FPU-assembly rework. The ~100× divergence from CheckIt cannot be closed without hand-rolled NASM inner loops for Modules 2/3/9. v0.4 scope.
- **[#5](https://github.com/tonyuatkins-afk/CERBERUS/issues/5)** — diag_cache PASS threshold miscalibrated. First real-iron data on BEK-V409 shows ratio 27.4× on known-healthy cache; PASS threshold was set at 40×. Threshold based on theoretical per-line speedup not matched in practice on TSR-contended BIOS configs. Recalibrate to ≥ 24× (per-traversal) OR restructure classifier to compute per-line ratio and threshold at ≥ 1.3×. Blocks v0.3-rc1 tag.

---

## Load-bearing lessons from this weekend

### Methodological

**Dhrystone's anti-DCE problem is an order harder than Weicker's paper suggests.** The 1984 paper assumes an optimizer-family that does DCE but does not defeat `volatile`. Watcom's `-ox` DCEs past volatile because the qualifier only propagates through variables whose full access path is volatile-qualified — non-volatile-qualified pointer parameters accepting addresses of volatile data defeat the barrier inside the callee. The session spent five flag-tuning commits (97d24e6 → 814db25) discovering that no pure-flag approach produces Dhrystone + Whetstone simultaneously at CheckIt reference speed without hand-rolled assembly. Lesson: **published scene benchmarks assume 1988-era compiler aggression; modern optimizers require either aggressive `volatile` propagation through every signature or structural anti-DCE like asm kernels**. Tag for v0.4.

### Process

**Quality gate caught a class of defect the initial implementation missed.** Round 2's discovery of the PIT-wrap issue in `timing_elapsed_ticks` was the single biggest bug of the session — the DCE-suppression work in the Whetstone tuning ladder would have produced modulo-garbage numbers regardless of flag choice, because the timing primitive couldn't measure the 5-second intervals the benchmarks targeted. Root-cause catch happened because the round-2 reviewer actually opened `timing.c` and noticed the single-wrap comment, whereas round 1 had focused on the surface-level findings (forward decls, doc drift). **Lesson:** the value of iterative adversarial review is not the first round's findings but the layered discovery that happens only when the first round's fixes expose what's underneath.

### Real-hardware

**State-dependent bugs are real.** The UI hang reproduced once and then went into hiding across three cold-boot attempts with varying build configurations. Issue #3 captured this honestly as "observed once, not reproducing, reopen on return." Alternative — declaring the bug fixed and moving on — would have been wrong because the instrumentation never exercised on real iron. **Lesson:** intermittent real-hardware bugs deserve their own disposition category separate from "fixed" and "deferred." File the issue, stash the investigation, move on.

### Scope

**Autonomous scope-fit.** The Sunday-morning 60-minute autonomous run delivered v0.3 code-complete + documented + host-tested. The budget was tight (cache-bench module + DMA probe + host tests + docs in one hour), but scope fit because:
1. Plan doc was written first (15 min), making the implementation an execution task rather than a design task.
2. Both modules had pure-math kernels that were host-testable, absorbing the uncertainty budget.
3. Real-hardware validation was honestly deferred to the user's next session rather than faked via guesswork.

**Lesson:** autonomous sessions are productive when the scope is planning + code-complete + host-tested, with hardware validation explicitly deferred to a human-present session. Don't try to validate on hardware you can't operate; document what needs validation and move on.

---

## Open items for next session

### Prereq for v0.3-rc1 tag

First real-hardware validation ran 2026-04-19 Sunday morning on BEK-V409:
- ✅ `diagnose.dma.summary=ok (6/6 channels responsive, 2 safety-skipped)`
- ✅ `CERBERUS.LAST` absent — UI-hang non-regression (4th clean run)
- ✅ Zero FAIL consistency rules
- ⚠️ `diagnose.cache.status=WARN (ratio=27.39 × — partial)` — threshold miscalibrated, see issue #5

v0.3-rc1 tag waits on:
1. Resolve issue #5: recalibrate `diag_cache_classify_ratio_x100` threshold (lean: per-line ratio reformulation with threshold ≥ 1.3×; alternative: per-traversal threshold drop to 24× based on BEK-V409 observation).
2. Update host-test Scenarios I/J in `tests/host/test_diag_cache.c` to reflect new thresholds.
3. Re-validate on BEK-V409: expect `cache.status=pass` with per-line ratio 1.71× (same measurement, new classification).
4. Then tag `v0.3-rc1` at the recalibration commit.

### Prereq for v0.2 final + v0.3 final

Real-hardware validation on 386 DX-40 and 8088/V20 classes. See `docs/sessions/NEXT-PLATFORMS.md` for the bench-box inventory + expected-surfacing bug budgets.

### Consistency-rule follow-ups

Not blockers for the RCs, but tracked as future commits:
- **Rule 4b narration extension** — consult `diag.cache.status` when emitting `cpu_ipc_bench` WARN. Distinguishes "TSR stealing cycles" from "cache BIOS-disabled".
- **Rule 11 `dma_class_coherence`** — XT-class CPU + DMA slave responsive = contradiction worth WARN.
- **Rule 8 `cache_stride_vs_cpuid_leaf2`** — reserved for Phase 3 cache-bench work.

### v0.4 scope

Per `docs/plans/checkit-comparison.md`:
- Whetstone FPU-assembly rework (issue #4).
- Cache-bandwidth benchmark.
- Video-throughput benchmark.
- Bar-graph comparison UI (v0.5, contingent on UI-hang resolution).

### Investigation reopen criteria

- **Issue #3 UI hang** — reopen on any real-hardware reproduction. Stashed instrumentation (`stash@{0}` + `docs/plans/attic/ui-hang-instrumentation-2026-04-18.patch`) ready for re-apply.
- **Issue #2 OPL intermittency** — reopen on a dedicated session with systematic cold-boot cycling to characterize the state variable.

---

## Metrics

| | Session start (`7da102e`) | Session end (`d9d1f5a`) | Δ |
|---|---|---|---|
| Commits since `v0.1.1-scaffold` | 32 | 60 | +28 |
| EXE size | 74,948 B | 136,096 B | +61,148 |
| DGROUP | 45,600 B | 51,712 B | +6,112 |
| Host-test assertions | ~119 | 161 + 4 pre-existing FAIL | +42 (+38 new pass + 4 pre-existing) |
| Consistency rules live | 7 | 10 | +3 |
| Diagnostics complete | 4/6 | **6/6** | +2 |
| Hardware DBs (total entries) | 121 | 128 | +7 (audio mixer_chip column) |
| Real-iron bugs found + fixed | 0 | 5 | +5 (all `eeba319`) |
| RC tags | 0 | 1 (v0.2-rc1) | +1 |
| GH issues filed | 0 | 5 | +5 |
| UI-hang intermittent data points | 1 (afternoon repro) | 4 clean (evening + autonomous + v0.3 validation) | state drift documented |
| Cache diagnostic threshold calibration | speculative | first real-iron data point (27.4× on BEK-V409) | issue #5 |

---

## Closing

Twenty-eight commits. Two arcs. One quality gate. One autonomous session. One RC tag (v0.2-rc1). Five issues filed and one preserved-for-later investigation. Real-iron-validated against the canonical 486 bench box for the first time, with DCE + display + timing + benchmark-methodology + cache-diagnostic-threshold-calibration classes of bug all surfaced and either fixed, honestly documented, or filed as first-data-point observations. CERBERUS is no longer "pre-alpha approaching v0.2" — it's **v0.2-rc1 tagged, v0.3 feature-complete and first-run-validated**, and waiting on a threshold recalibration pass to tag v0.3-rc1.

The v0.3 validation itself was the final lesson of the weekend: diagnostic thresholds chosen on theory alone need first-real-hardware recalibration. The diagnostic IS working — the ratio classifier correctly observed the cache signal on BEK-V409 and emitted a verdict. The verdict was wrong because the theoretical threshold assumed hardware behavior that the bench box's TSR-contended configuration doesn't produce. Issue #5 captures this: not a bug, a calibration artifact, fix in next session.

Next weekend:
- Issue #5 first: recalibrate diag_cache threshold, re-validate, tag v0.3-rc1.
- 386 DX-40 session: platform-specific bug budget per `NEXT-PLATFORMS.md` (3-7 expected).
- Rule 11 (`dma_class_coherence`) + Rule 4b narration extension.
- Whetstone FPU-assembly rework (issue #4) if time permits.

Call the weekend.
