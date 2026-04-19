# Session Report — 2026-04-18 Afternoon → 2026-04-19 Evening Weekend

**Operator:** Tony Atkins
**Duration:** ~30 hours wall-clock across five contiguous sessions (Saturday afternoon, Saturday evening, Sunday morning, Sunday afternoon, Sunday evening)
**Arc:** v0.2 real-hardware gate → v0.2-rc1 tag → v0.3 feature-complete + threshold-recalibrated validation → v0.3-rc1 tag → v0.4 benchmarks and polish → v0.4-rc1 tag. Three release candidates in one weekend.

---

## What shipped

### Tags (three RCs)

- **`v0.2-rc1`** at `311c19e` — first real-hardware-validated CERBERUS RC. Ten consistency rules. Dhrystone ±2.4% of CheckIt. Whetstone CONF_LOW with documented divergence.
- **`v0.3-rc1`** at `dee7360` — diagnostics complete (6/6 subsystems). diag_cache classifier recalibrated per-line after first-iron data. diag_dma 8237 count-register probe. Three-pane UI polish. BEK-V409 validation: `cache.status=pass (ratio=1.70 × — cache_working)`.
- **`v0.4-rc1`** at `ab67703` — benchmarks and polish. bench_cache + bench_video + DGROUP lift 52K→56K + Rule 4b narration extension + Rule 11 `dma_class_coherence` + version macro consolidation. Five of six planned deliverables; Whetstone FPU-asm rework (issue #4) deferred.

### Commit arc (~75 commits since `7da102e`)

Grouped by sub-arc. The weekend's shape was layered — each arc's stop conditions informed the next arc's scope.

#### Saturday afternoon — real-hardware gate (`eeba319` → `7da102e`)

Three commits, all at the bench box:

- `eeba319` — **Five bugs fixed on contact with BEK-V409.** HIMEM.SYS intercepting INT 15h AX=E801h (extended_kb=0 → 63076). S3 Trio64 BIOS-string collision ("IBM VGA" matched first). Vibra 16S DSP status port at base+0x0E. OPL 0x388 mirror gating under CTCM. UMC491 8254 latch race.
- `7e4bdcb` — Rule 4b + audio DB T-key split + bench_memory `kb_per_sec` precision fix.
- `7da102e` — `/NOUI` escape hatch + `unknown_finalize` moved pre-UI. Workaround for the UI hang observed on v5.

#### Saturday evening — mixer probe + historical benchmarks (`98c07d5` → `311c19e`)

- `98c07d5` — 486 run corpus (six INIs at `tests/captures/486-real-2026-04-18/`).
- `0161e99` — CERBERUS.md "Why real hardware" H2 naming the five `eeba319` bugs.
- `c22e886` — CT1745 mixer probe + Rule 7 `audio_mixer_chip`.
- `e897c15` / `525f65b` / `f0cebde` — Dhrystone 2.1 + Whetstone + PC-XT relative ratings + Rule 10.
- `97d24e6` / `8552c6d` — anti-DCE iterations. volatile + checksum observers **proved insufficient**; NULL-display V_U32 pattern **proved sufficient** for display-corruption class.
- `1788561` — Makefile `CFLAGS_NOOPT -od` on bench_dhrystone + bench_whetstone.

**Six-round quality gate on the v0.2-rc1 candidate.** Round 2 surfaced the biggest defect of the session: `timing_elapsed_ticks` only handled one PIT wrap (~55ms), so 5-second Dhrystone/Whetstone targets were measuring modulo-garbage. Fixed via `timing_start_long` / `timing_stop_long` (`af2b6cd`). Rounds 3-6 closed everything else. Final gate: 0 Fatal, 0 Significant.

**Real-iron bench tuning ladder** ending in `311c19e`: Dhrystone nailed (32,810 = ±2.4% of CheckIt), Whetstone unfixable at pure-flag level (109 K-Whet vs 11,420 reference → ~100× low); ship CONF_LOW with full divergence documentation. Honest trade.

#### Sunday morning — autonomous v0.3 implementation (`10237c3` → `d9d1f5a`)

User at the park. Session continued autonomously:

- `10237c3` — v0.3 plan doc (diag_cache stride-ratio + diag_dma 8237 probe).
- `7a28850` — both diagnostics + wiring + host tests. diag_cache: 2 KB vs 32 KB `__far` buffers. diag_dma: 8237 count-register probe, hard-skip ch0/ch4. 27 new host-test assertions.
- `d9d1f5a` — README + CHANGELOG: "4 of 6 complete" → "6 of 6 complete".

v0.3 code-complete, host-tested. Real-hardware gate deferred to user's next session.

#### Sunday afternoon — issue #5 + three-pane UI + v0.4 re-plan (`25306b4` → `2956c86`)

User's "one hour autonomous, full runway" session:

- First v0.3 validation on BEK-V409 (`bacd642` archived) showed `diag.cache.status=WARN (ratio=27.39 × — partial)` — **threshold miscalibration, not diagnostic failure.** 27.4× ratio on known-healthy cache, below 40× PASS threshold. Filed as issue #5.
- `25306b4` — **issue #5 fix: per-line ratio reformulation.** Strip the 16× buffer-size bias; PASS at ≥ 1.3× per-line speedup. Host-test Scenarios A-M rewritten (19 total). Scenario J seeded at 171 for BEK-V409 regression protection.
- `4e84481` — three-pane summary UI: direct-VRAM color writes via MK_FP, title / detection / benchmarks / system-verdicts panes. Screenshot-ready polish.
- `dee7360` — v0.4 benchmarks-and-polish plan doc. Five deliverables scoped: bench_cache, bench_video, Rule 4b narration extension, Rule 11 dma_class_coherence, Whetstone FPU-asm.
- **`v0.3-rc1` tag at `dee7360`** — annotated release message covering all six diagnostics, ten consistency rules, known issues, v0.4 scope.
- Second BEK-V409 validation: `cache.status=pass (ratio=1.70 × — cache_working)`. Issue #5 closed.
- `2956c86` — archived v0.3-rc1 validation INI.

#### Sunday evening — v0.4 implementation (`2c5eed1` → `53c9fb4`)

- `2c5eed1` — version macro consolidation. `CERBERUS_VERSION` "0.1.0" → "0.4.0-dev". Every downstream emit sources from the single `#define`.
- `ce30b68` — DGROUP ceiling lift 52,000 → 56,000. Documented in README + v0.4 plan.
- `d1e93b3` — Rule 4b narration extension. Three-way split consulting `diag.cache.status`: cache_working → TSR/thermal; no_cache_effect → cache-BIOS-disabled; missing → original ambiguous form. Five new test_consist scenarios (JJ-NN).
- `169d1d2` — Rule 11 `dma_class_coherence`. XT-class + DMA slave responsive = contradiction WARN. Eight new test_consist scenarios (OO-WW). Uses `strstr("skipped_no_slave")` so future emit tweaks don't silently break it.
- `efcb4ca` — **bench_cache base implementation.** Four KB/s rows + checksum. Shared FAR buffer refactor via new `src/core/cache_buffers.{c,h}`. Pure-math kernel `bench_cache_kb_per_sec` with host tests (9 scenarios).
- `b5a1685` / `3798ed9` / `a0e7611` / `7932b3c` — bench_cache QG rounds 1-4 (score 11 → 4 → 3 → 10 fresh-eyes progression). Final round fixes: CFLAGS_NOOPT for bench_cache.obj (F2/SP4), checksum mask (S2), gate polarity inversion (SP2). Key rename l1_*/ram_* → small_*/large_* landed in round 1 after the reviewer flagged the write-through 486 labeling concern.
- `356a548` — **bench_video base implementation.** Text-mode write + mode 13h round-trip. Save/restore preserves display. INT 10h AH=0Fh mode query at entry. Emulator gate.
- `6ac7e02` / `c507990` — bench_video QG round 1 + round 2 fix commits (score 4 → 6 per cap; characterization only at R3). Per-iter volatile observer parity with bench_cache, fail-safe `/ONLY:BENCH` gate, current-mode gate (not adapter-class), user-facing flicker warning.
- `ab67703` — bench_video F1 + F2 Fatals closed. Page stride computed from mode (0x0800 for 40x25, 0x1000 for 80x25). `saved_mode` masked against bit 7 in mode13h path.
- **`v0.4-rc1` tag at `ab67703`** — release notes at `docs/releases/v0.4-rc1.md`.
- `53c9fb4` — release notes + v0.4-rc1 BEK-V409 validation INI at `tests/captures/486-real-2026-04-19-v0.4-rc1-candidate/`.

---

## Issues filed (six; one closed)

- **[#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1)** — `test_timing` 4 pre-existing failures. Gated behind Rule 4a UMC491 8254 phantom-wrap deep-dive. Unchanged across the weekend.
- **[#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2)** — Intermittent OPL detection on Vibra 16 PnP. State-dependent per cold-boot. v0.4-rc1 BEK-V409 capture showed clean side (`opl=opl3`).
- **[#3](https://github.com/tonyuatkins-afk/CERBERUS/issues/3)** — UI hang observed once Saturday afternoon. **Did not reproduce across 7+ clean real-iron runs.** Instrumentation preserved as `stash@{0}` + `docs/plans/attic/ui-hang-instrumentation-2026-04-18.patch`. Reopen on repro.
- **[#4](https://github.com/tonyuatkins-afk/CERBERUS/issues/4)** — Whetstone FPU-assembly rework. ~100× divergence unfixable at flag level. Still open after v0.4-rc1.
- **[#5](https://github.com/tonyuatkins-afk/CERBERUS/issues/5)** — diag_cache threshold miscalibration. **CLOSED Sunday afternoon at `25306b4`** — per-line reformulation + BEK-V409 revalidation at 1.70×.
- **[#6](https://github.com/tonyuatkins-afk/CERBERUS/issues/6)** — bench_video measures ISA-range bandwidth on VLB hardware. Known measurement-scope limitation; investigation plan covers physical VLB slot check, BIOS/jumper verification, CFLAGS_NOOPT vs -ox bench_video variant profile. Scoped for v0.4.0 investigation.

---

## Load-bearing lessons from this weekend

### Methodological

**Dhrystone's anti-DCE problem is an order harder than Weicker's paper suggests.** Watcom's `-ox` DCEs past `volatile` because the qualifier only propagates through signatures whose entire access path is volatile-qualified — non-volatile-qualified pointer parameters accepting addresses of volatile data defeat the barrier. Five flag-tuning commits (`97d24e6` → `814db25`) discovered no pure-flag approach produces Dhrystone + Whetstone both at CheckIt reference speed. Tag `CFLAGS_NOOPT -od -oi` in the Makefile is the structural answer for synthetic benchmarks. This lesson extended in v0.4: bench_cache and bench_video **also** land in the CFLAGS_NOOPT pool. The absolute-rate cost (~3× slower inner loops vs -ox) trades against DCE safety and is accepted as the measurement-scope posture.

### Process

**Quality gate past round 3 hits diminishing returns.** Saturday's six-round gate on v0.2-rc1 found its biggest defect (the PIT-wrap) at round 2 and then spent rounds 3-6 finding increasingly fine-grained issues. Sunday evening's bench_cache gate ran four rounds (11 → 4 → 3 → 10 score progression) — round 4's fresh-eyes-reviewer found 10 Significant issues round 3 hadn't surfaced; not regressions, just different concerns a different reviewer prioritized. Sunday evening's bench_video gate was explicitly capped at **3 total rounds** with acceptance that round 3 would characterize rather than drive another fix loop. Cap worked: bench_video landed with its highest-impact findings addressed, two Fatals tracked for pre-tag closure, remaining minors documented. **Lesson:** quality gate iteration is valuable; quality gate without a cap is a spiral.

### Real-hardware

**Diagnostic thresholds need first-real-iron recalibration.** diag_cache's 40× PASS threshold was theoretical. Real iron on BEK-V409 produced 27.4× because TSR-contended BIOS configs don't produce the 5-10× per-line speedup the theory assumed — they produce 1.7×. The per-line reformulation (issue #5 fix) didn't change the measurement; it changed what the measurement means. **Lesson:** first-hardware-data is the authority on classifier thresholds, not theory.

**Write-through vs write-back caches make labels lie.** bench_cache's original `l1_*` / `ram_*` keys implied "L1 is faster than RAM" — true on write-back hardware, false on write-through 486s where every store pushes through to DRAM. Keys renamed to `small_*` / `large_*` to match the size-based framing the module actually measures. BEK-V409 capture confirmed the call: `small_write = 5,005`, `large_write = 5,067` (near-identical on write-through hardware). The rename wasn't cosmetic; it was correctness.

### Deployment

**Stale binary gotcha, twice.** Sunday afternoon's first v0.3 validation ran on a stale binary because the push didn't land where the user's working directory expected. Sunday evening the same class of bug bit the version-string check — pushed CERBERUS.EXE to `/drive_c/`, but user's working directory is `C:\CERBERUS\`, where the stale v0.3 EXE sat in DOS search order ahead of the new one. **Lesson:** always push to the user's working directory, verify via byte-size + timestamp on the target, don't trust the push banner alone. Memory updated at `reference_486_ftp_workflow.md`.

### Scope

**Autonomous scope-fit holds up when scoped tight.** Sunday morning's 60-min autonomous run delivered v0.3 code-complete + host-tested. Sunday afternoon's 60-min autonomous run delivered issue #5 fix + three-pane UI + v0.4 plan doc + v0.3-rc1 tag. Sunday evening's ~4-hour block delivered five v0.4 deliverables + v0.4-rc1 tag. All scoped tightly enough to complete; none tried to validate on hardware the operator couldn't operate. **Lesson:** the pattern "plan + code + host-test + explicit-hardware-deferral" scales from 60-min to 4-hour blocks.

---

## Attribution

The Homage Phase 1 research (decompilation-led lessons from CACHECHK / SPEEDSYS / CheckIt) ran as an independent work stream during this weekend. Phase 1 discovery report is parked at `C:\Development\Homage\_research\phase1-report.md` pending authorization to proceed to Phase 2. No Phase 1 findings informed any of the three RCs tagged this weekend; attribution slot for Phase 2 research contributions is held open in the v0.4-rc1 release notes for v0.4.0 final.

---

## Metrics

| | Weekend start (`7da102e`) | Weekend end (`53c9fb4`) | Δ |
|---|---|---|---|
| Commits since `v0.1.1-scaffold` | 32 | 98 | +66 |
| EXE size | 74,948 B | 144,066 B | +69,118 |
| DGROUP | 45,600 B | 53,152 B | +7,552 |
| Host-test assertions | ~119 | 163 + 4 pre-existing FAIL | +44 (+40 new pass + 4 pre-existing unchanged) |
| Consistency rules live | 7 | 11 | +4 (4b narration, 7, 10, 11) |
| Diagnostics complete | 4/6 | **6/6** | +2 (diag_cache, diag_dma) |
| Benchmark modules | 3 (cpu, mem, fpu) + Dhry/Whet mid-session | 5 + 2 historical (added bench_cache, bench_video) | +4 |
| Hardware DB entries | 121 | 128 | +7 (audio mixer_chip column) |
| Real-iron bugs found + fixed | 0 | 5 + 2 Fatals tracked | +5 fixed + 2 closed pre-tag (bench_video F1/F2) |
| RC tags | 0 | **3** (v0.2-rc1, v0.3-rc1, v0.4-rc1) | +3 |
| GH issues filed | 0 | 6 | +6 (1 closed: #5) |
| UI-hang clean runs | 1 (afternoon repro) | 7+ consecutive | state drift documented, not reproducing |
| DGROUP ceiling | 52,000 (implicit) | 56,000 (documented) | +4,000 |
| Version macro | 0.1.0 (stuck) | 0.4.0-dev | consolidated to single source |

---

## Closing

Three RCs in one weekend is the milestone. Every RC landed after real-hardware validation on BEK-V409 with a full INI capture archived alongside the tag. Every RC was gated by quality-gate rounds; every fix commit trailed `Assisted-by: Claude:claude-opus-4-7`. Every deferred item became a tracked issue with explicit reopen criteria. No arc stretched past its scope.

The v0.4 arc specifically demonstrated that quality gate with a round cap is a workable posture: bench_cache converged across four rounds at user-chosen stopping points; bench_video converged across two fix rounds + one characterization round per an explicit 3-round cap. Diminishing returns past round 3 is real; capping acknowledges it without compromising the review's value.

Three known hardware characterization items remain for v0.4.0 final:
- **Issue #4** — Whetstone FPU-asm rework (the structural fix for the ~100× Whetstone gap)
- **Issue #6** — bench_video VLB bandwidth investigation (ISA-range numbers on VLB hardware)
- **386 DX-40 + 8088/V20 validation** per `NEXT-PLATFORMS.md`

Plus the Phase 2 Homage research lessons once Phase 2 authorization lands.

Weekend closed with the 486 powered down cleanly, preparing for 386 transition. Next arc will anchor on a fresh 386 install; the 486 has given everything it can to the v0.4 cycle.

Call the weekend.
