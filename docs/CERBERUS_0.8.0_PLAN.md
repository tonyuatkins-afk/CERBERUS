# CERBERUS 0.8.0 Plan (Revised)

Revised: 2026-04-21. Supersedes the initial 0.8.0 draft of the same date. Reconciles the first draft against a red-team critique focused on execution risk (DGROUP, Whetstone ambiguity, upload contamination, UI overreach, 8088 architectural reality).

The core thesis is preserved: **0.8.0 is the trust and validation release, not the feature-expansion release.** What changes from the first draft is sharpness. Conflicted recommendations are now single decisions. Scope is smaller. The feature budget is explicitly constrained by near-data memory, not just by calendar time.

---

## 1. Executive Summary

**0.8.0 identity.** The release in which CERBERUS became real-hardware-proven across the target range it claims to support, and in which the known credibility traps (Whetstone, upload-path crash, bespoke UI grammar) were closed by reduction rather than by addition. Not "CERBERUS got bigger." "CERBERUS got trustworthy."

**Three governing principles for the release, in this order:**

1. **Trust beats reach.** A tighter 0.8.0 that runs clean on four machine classes is worth more than a broader 0.8.0 that runs on one. If any feature cannot be cleanly validated on at least two distinct real-hardware generations by the 0.8.0 freeze, it does not ship in 0.8.0.
2. **Near-data is a release budget, not a guideline.** DGROUP sits at ~60.6 KB of 64 KB. Every new string, every new probe, every new UI label competes for the same 3-4 KB of headroom. A DGROUP budget gate is introduced before any M2 feature expansion work; no feature lands without a post-landing DGROUP delta measurement. The tool runs on an 8088 with 256 KB or it does not ship.
3. **8088 is not "an older CPU." It is a different environment.** The floor claim is load-bearing. Honoring it requires either real XT-class validation before 0.8.0 tag, or public retraction of the floor claim for this release. No middle ground.

**Deltas from the first draft, summarized.** Whetstone is removed from default-emit. Upload execution is compiled out of stock binaries. UI scope narrows to CUA-lite (keybindings + F-key legend, no menu bar, no Turbo-Vision-style window chrome). DGROUP is a gate. 8088 captures are architecturally required, not "would be nice."

---

## 2. What the Existing Plan Got Right

These elements from the first draft remain unchanged in direction and are not revisited by this revision:

- **0.8.0 as the trust-and-validation release, not feature-expansion.** This is the core thesis. Every decision below serves it.
- **Real-iron proof as the release identity.** The first draft's insistence on three independent captures (486, 386, 8088/286) is correct. This revision hardens the requirement.
- **Community upload skepticism.** First draft leaned toward deferral; this revision makes it a hard cut from the default build. The direction was correct; the decisiveness was not.
- **Brand preservation without domination.** The three-heads iconography, the ANSI boot intro, the visual journey (bit parade, Lissajous, Mandelbrot, metronome, audio scale, cache waterfall, latency heatmap), and the scrollable summary all stay. None of them are modified in 0.8.0 beyond bug fixes. The costume is load-bearing identity; do not mess with it.
- **`MS-DOS UI-UX Research.md` as primary UX input.** The three orthogonal axes (visual-adapter-tier, interaction-CUA/SAA, structural-archetype) remain the reference. What changes is the ambition: 0.8.0 adopts the interaction axis only. Structural and visual polish defer.
- **Validation strategy as release-grade discipline.** The first draft's "ground truth, then observe" protocol and the `tests/captures/` archival convention are retained unchanged.
- **Homage research posture.** The ethical frame (no decompiled code, attribution-honest, corrections-flagged) stays. The CheckIt collision reframing (Whetstone and Dhrystone as custom synthetics, not ports) stays. The QA-Plus "possible causes" pattern stays on the must-have list.
- **The strengths ledger.** Confidence model, consistency engine, dual-signature scheme, thermal Mann-Kendall, crumb/LAS trail, emulator confidence clamping, FPU behavioral fingerprint, cache characterization. These are the instrument. 0.8.0 deepens them; it does not replace any of them.

---

## 3. Where the Existing Plan Was Too Optimistic or Internally Inconsistent

### DGROUP / Memory Budget

The first draft mentioned DGROUP pressure in one line of the Weaknesses section ("~60.6 KB against the 64 KB DOS ceiling") and did not treat it as a constraint on the 0.8.0 scope itself. That is wrong. Look at what M2 and M3 actually add as DGROUP cost:

- **CSV output**: column header row, per-row format strings, unit labels. At least 500-1000 bytes of new string data.
- **Possible-causes tables**: one static string per consistency rule failure path, across 11 rules, with narration. Realistic estimate: 2-4 KB.
- **FPU research-gap probes (I/J/K/L/M)**: per-probe status strings, per-mode result labels, per-edge-case operand names. Conservatively 1-2 KB if kept terse.
- **L2 cache probe**: probe labels + larger working-set sweep (64/128/256 KB labels). Small in DGROUP (~200 bytes), the FAR buffers live in FAR data, not DGROUP.
- **CUA-lite F-key legend**: 10-12 label strings, each short. ~300 bytes.
- **Help overlay text** if F1 is wired: every section gets a help string. 1-2 KB if disciplined.
- **Memory checkerboard patterns**: pattern constants (trivial DGROUP), verdict-label strings (~200 bytes).

Conservative total DGROUP cost of the full must-have list: **5-8 KB of new near-data**. Current headroom: **3-4 KB**. The math does not work.

The first draft's scope does not fit in DGROUP. Either the scope shrinks or the near-data is reorganized. Prior work (v0.6.1 `-zc` attempt, v0.6.2 deferred reclaim) already surfaced that Watcom's const-segment-reclaim path does not handle unnamed string literals. The right answer is a mix of targeted cuts, explicit `__far const` migration for large static tables, and a DGROUP gate.

### Whetstone / FPU Benchmark Strategy

The first draft said "either retire or reframe." That is two plans. A trust-first release does not ship with two plans for a benchmark whose number is likely 30x wrong. The first draft named the wrong choice as acceptable: "keep the kernel but mark CONF_MEDIUM." A CONF_MEDIUM value that is 30x out of range is not a medium-confidence measurement; it is a broken measurement with an apologetic label. The retro community does not read labels, they read numbers.

This revision resolves the ambiguity below (§7).

### Upload / Network Scope

The first draft recommended "disable by default, gated behind a build flag or a preview flag." That leaves upload code in the stock EXE, which means the stack-overflow defect remains in the default code path, which means any user who passes `/UPLOAD` crashes, which means the 0.8.0 trust story is one command-line flag away from being destroyed.

"Disable by default" is not enough when the code in question has an unfixed crash. "Compile out of stock builds" is the only posture that matches the trust-release identity.

This revision resolves this decisively below (§8).

### UI / UX Implementation Scope

The first draft's M3 proposed "top menu bar (File / View / Detect / Diagnose / Benchmark / Help), bottom F-key legend, F1 context help... Turbo Vision-flavored without requiring the TV library." That is a new menu-system module, a new modal help-viewer module, a new dropdown-paint module, a new menu-dispatch state machine. Each of those carries code-size cost, DGROUP cost (labels, help text), and validation cost (test every path under every adapter tier).

The critique is right: a thin CUA-conformant frame layer is a medium-effort project with its own integration surface. The existing `ui.c` + `intro.c` + `head_art.c` + `tui_util.c` already render cleanly; they do not need frames. They need the keyboard grammar that DOS users already know.

This revision narrows UI scope below (§9).

### Real-Hardware Validation Scope

The first draft said "one 486, one 386, one 8088 or 286." That "or" is the weakness. 8088 and 286 are not interchangeable for validation purposes. A 286 with 287 and 1 MB of RAM boots protected mode, has an AT-class keyboard controller, has slave DMA, has an RTC, and is on the AT side of a deep architectural boundary. An 8088 with MDA and 256 KB of conventional has none of those things and is on the XT side. A 286 capture proves approximately 60% of what an 8088 capture would prove. The floor claim is specifically about XT class, not about "any old CPU."

This revision hardens validation requirements below (§10).

---

## 4. Non-Negotiable Ship Criteria for 0.8.0

These are pass/fail. Any one of them missing, 0.8.0 does not tag.

1. **Real-hardware captures archived in tree with public claims matched to them.** Minimum three captures (486, 386, 286 or equivalent AT-class). Fourth capture (8088 or equivalent XT-class) is required if and only if the README retains the "8088 floor" claim. Per §10, the ship gate is the honesty of the claim, not the capture count: either the 8088 capture lands and the floor claim ships, or the floor claim retracts to "286 through 486 validated, XT-class validation pending" and 0.8.0 ships on three. Both are honest; both ship-valid. Each capture carries a ground-truth README, a full INI, a UNK dump if emitted, and a consistency-rule diff.
2. **Default-flag run completes to DOS prompt cleanly on every validated machine.** No reboot required, no `/SKIP:` workaround needed, no `/NOUPLOAD` needed, no `/NOUI` needed. Every flag is optional in 0.8.0.
3. **No CONF_HIGH value in any default-run INI is known to be 10x or more out of range for its hardware.** This is the Whetstone gate, restated as a general principle.
4. **DGROUP near-data ≤ 62 KB** at tag, with a 2 KB hard-reserve for future patch-level additions. Verified via `wmap` or equivalent.
5. **`CERBERUS.EXE` ≤ 180 KB**, consistent with the current soft ceiling.
6. **Host test suite green**, with a new inference test for every new probe landed in 0.8.0.
7. **Documentation parity.** `README.md`, `CERBERUS.md`, `CHANGELOG.md`, `docs/methodology.md`, `docs/consistency-rules.md`, `docs/ini-format.md` all reflect 0.8.0 content. "Known does not work" section is explicit.
8. **No network transmission in the default-built binary.** Upload execution is compiled out. Network detection and INI annotation remain, because they have value without transmission.
9. **Every consistency rule that can FAIL has a "possible causes" narration string attached.** Short, static, no dynamic composition.
10. **Public support claims match captured evidence.** If no 8088 capture exists at tag, the 8088 floor claim is retracted from the README for the 0.8.0 cycle.

---

## 5. Explicit Cuts, Deferrals, and Build Gating Decisions

### Must Cut from Default 0.8.0

- **`bench.fpu.k_whetstones` from the default INI emit.** See §7.
- **All runtime upload execution from the stock build.** See §8.
- **Menu bar and drop-down UI.** See §9.
- **Per-instruction FPU microbenchmarks.** Methodologically attractive, scope-heavy. Defer.
- **Cache associativity probe (research gap D).** Medium-model DOS limitation. Defer.
- **Instruction-vs-data cache split detection (F).** Pentium+ scope. Out.
- **Self-modifying code probe (G).** Risky on real iron, narrow applicability. Out.
- **Address-to-chip QARAM-style physical translator.** Needs board-map data format. Defer to 0.9.0+.
- **SVGA 132-column text modes.** Polish, not trust. Defer.
- **Full disk-I/O benchmark suite.** Wide validation surface. Defer.
- **Interactive hardware tests** (keyboard loopback, serial, parallel, joystick, mouse-presence). Out of batch-first model. Defer.
- **Themed DAC-gradient title bars.** Polish only. Defer.
- **Refresh-failure memory classification as a distinct verdict.** Nice, defer.

### May Remain Only Behind Experimental or Build-Time Gating

- **Whetstone kernel (`bench_whet_fpu.asm` + `bench_whetstone.c`).** Stays compiled in every build because it exercises FPU asm paths other code does not exercise, and because removing it entirely would invalidate the issue #4 archaeological trail. **The build flag gates emit, not compilation.** `wmake WHETSTONE=1` enables the dispatcher to run the kernel and emit `bench.fpu.k_whetstones`; stock builds compile the same objects but the dispatcher returns immediately with a `whetstone_status=disabled_for_release` row and no number. Host tests run against the kernel in every build. The kernel source remains in tree, the path is there for 0.9.0 reframing.
- **Upload execution** (`upload_execute`, HTGET shell-out, INI re-write on success). Gated behind `wmake UPLOAD=1`. In stock builds, `/UPLOAD` prints "not built with upload support" and exits non-zero with a clear message; `/NOUPLOAD` is accepted as a no-op for backward compatibility. See §8 for what stays in the stock binary.
- **Cyrix DIR-based pre-CPUID discrimination.** The `/NOCYRIX` flag already exists. Code path can land in 0.8.0 if it is fully validated on real Cyrix silicon; otherwise defer. No real Cyrix machine in the current validation corpus means: defer.

### Deferred to 0.8.1 or 0.9.0

- **Per-instruction FPU microbench** (new FPU throughput story to replace Whetstone).
- **Community upload** re-enabled with circuit-breaker, health check, deployed server, and a security review.
- **Full CUA shell** with menu bar, help viewer, dialog system.
- **System summary dashboard as the default landing screen** (the CheckIt "launch with SysInfo populated" principle from `docs/research/homage/`), with detection / diagnostic / benchmark selection from that context. 0.8.0 keeps the existing intro → journey → scrollable-summary sequence; 0.9.0 is where the user-greets-into-populated-data pattern replaces it. Naming it here so the direction is not lost.
- **PCI enumeration beyond current BIOS-probe path.**
- **Hardware database expansion past the current 128 seed entries** (grow organically via the UNK submission flow, no structured push in 0.8.0).
- **Assoc / I-D split / SMC cache probes.**
- **Disk I/O benchmarks, interactive peripheral tests, 132-column SVGA modes.**

---

## 6. Revised Workstream Plan

Four milestones, each with a decision gate. Any milestone that blows its gate triggers scope renegotiation, not schedule slip.

### M1: Stability, Pruning, and Real-Hardware Baseline

**Purpose.** Close every defect visible in default-flag operation and establish the real-hardware validation corpus. Before anything expands, the existing scope must be clean on every target tier.

**In scope.**
- **Whetstone default-emit suppression.** Implement the build gate (`wmake WHETSTONE=1`). In stock builds, `bench_whetstone.c` writes `whetstone_status=disabled_for_release` and no number. Consistency Rule 10 (`whetstone_fpu`) adjusted to skip cleanly when status is `disabled_for_release`. `bench_fpu.c` remains the FPU throughput metric.
- **Upload compile-out.** Implement the build gate (`wmake UPLOAD=1`). In stock builds, `upload_execute` is a stub that writes `upload.status=not_built` and returns. The HTGET shell-out path is `#ifdef`-excluded entirely. Stack overflow defect is eliminated because the crashing code is not in the binary. See §8 for the stock-binary surface.
- **Nickname buffer leak (issue #9)** fix. The narrow fix (move `upload_nickname_buf` out of BSS into a table-owned pool) is preferred; the architectural fix (report_add_str ownership refactor) is deferred unless the narrow fix has a second failure mode.
- **`cpu.class` normalization** to a family token (`486`, `386`, `286`, `8086`, `v20`, `pentium`, `cyrix_*`, etc.). Vendor string moves to a separate `cpu.vendor` key. Validator rule using `cpu.class` updated.
- **`bench_cpu` DB anchor reconciliation.** Either widen the `hw_db/cpus.csv` 486 DX-2 lower bound to include the 2M iters/sec observed, or produce a narration string explaining what TSR / BIOS-cache / thermal hypothesis the WARN implies. Recommend: both. The anchor widens; the narration is added.
- **EMM386 crumb-dependency investigation.** Either document that the crumb enter/exit pairs are load-bearing for V86-mode stability and name the side effect (INT 21h commit, atomic BIOS-tick read, memory barrier), or make the side effect explicit and remove the coincidence. Must not ship a release where reordering a crumb crashes the user's machine.
- **DGROUP audit tooling.** A simple script (`tools/dgroup_check.py` or a `wmake dgroup-report` target) that parses the `.MAP` file and prints per-module near-data usage, total, and distance from the 64 KB ceiling. Run in CI-equivalent (host test wmake).
- **Real-hardware captures.** Validation on BEK-V409 (486 ceiling baseline) plus at least one 386 machine, one 286 machine (or true AT-class XT hybrid with 287), one XT-class machine (8088 or 8086 with MDA or CGA). See §10.

**Out of scope.** Any M2 work. Any new probe. Any new UI element. No discretionary new UI or report strings — the handful of status rows M1 does add (`whetstone_status=disabled_for_release`, `upload.status=not_built`, the widened `cpu_ipc_bench` narration) are load-bearing for the scope cuts themselves and do not count; anything beyond that waits for M2 under DGROUP audit. M1 is reduction and validation only.

**Exit gate.**
- All four must-have captures archived.
- Default-flag run clean on all four.
- Host tests green.
- DGROUP audit shows ≤ 62 KB near-data at end of M1.
- Whetstone is not emitting by default; upload is not transmitting.

### M2: Precision Expansion Within Budget

**Purpose.** Land the research-gap probes from `Cache Test Research.md` and `FPU Test Research.md` that fit within the DGROUP headroom M1 leaves, with real-iron validation of each.

**Pre-M2 gate.** If M1 does not exit with ≥ 2 KB of DGROUP headroom below the 62 KB target, M2 scope is renegotiated before any M2 code lands. The renegotiation options, in priority order: (a) drop possible-causes tables (largest single DGROUP cost, re-scope to INI-only emit without in-UI narration), (b) drop CSV output (second-largest cost, defer to 0.8.1), (c) drop one of the FPU probes. M2 does not begin if the gate fails.

**In scope, contingent on DGROUP budget.**
- **FPU: FPTAN probe** (research gap I). Adds one axis to the behavioral fingerprint. ~100-200 bytes DGROUP.
- **FPU: rounding control cross-check** (gap J). Four modes, one known-differing computation per mode, result-bits emit. ~300-500 bytes.
- **FPU: precision control cross-check** (gap K). Three modes, similar shape. ~300-500 bytes.
- **FPU: IEEE-754 edge-case diagnostic** (gap L). Nine operand classes × five ops = 45 cases. Terse labels, ~600-1000 bytes. If over budget, cut ops to FADD/FSUB/FDIV/FSQRT (skip FMUL as redundant-coverage).
- **FPU: exception-flag roundtrip** (gap M). Six exceptions, one deliberate trigger each. ~300-500 bytes.
- **Cache: L2 detection** via extended size sweep to 64/128/256 KB. FAR buffers, so minimal DGROUP cost (~200 bytes labels).
- **Cache: pointer-chase L1 latency** (gap A). New asm kernel (`bench_cache_char_a.asm`), one additional INI key (`bench.cache.char.l1_ns`). ~100 bytes DGROUP; kernel bytes come from CODE segment.
- **Cache: DRAM-latency derivation** (gap E). Derived from the largest-working-set throughput; ~100 bytes.
- **Cache: stride=128** added to the stride sweep. Trivial.
- **Memory: checkerboard and inverse-checkerboard patterns.** Pattern constants negligible; verdict labels ~200 bytes.
- **CSV output mode (`/CSV`).** Emits `<out>.CSV` alongside the INI. String cost is the biggest question; estimated 500-1000 bytes format strings. If over budget at end of M2, CSV is first to go (defer to 0.8.1).
- **Possible-causes narration** for consistency rules. Per-rule static string attached to FAIL verdicts. Largest DGROUP cost, 2-4 KB. If DGROUP shows headroom risk during M2, narration tables are migrated to a `__far const` pool with a small accessor (3-4 KB FAR data, ~200 bytes DGROUP for the accessor).

**Out of scope.** Everything in §5's "Must Cut" and "Deferred" lists. Per-instruction microbenchmarks. Associativity. I/D split. Self-modifying code. Any UI framing work (that is M3).

**Exit gate.**
- Each new probe executed on BEK-V409 plus at least one other validated machine from the M1 capture corpus.
- Each new INI key documented in `docs/ini-format.md`.
- Host tests cover inference paths for every new probe.
- DGROUP ≤ 62 KB at end of M2 (hard).
- `CERBERUS.EXE` ≤ 180 KB at end of M2 (hard).
- No visual regression on any M1-validated machine.

### M3: UI/UX Interaction Polish Without Architectural Overreach

**Purpose.** Make CERBERUS feel like a DOS application to a DOS user, by closing the interaction-axis gap (CUA keybindings + Norton bottom-legend convention), without touching structural UI architecture.

**In scope.** See §9 for the full CUA-lite decision. Briefly:
- F1 wired to a single-screen help overlay reusing the existing scroll primitives. Help text is a per-section static string array.
- F3 exit. Esc exit (already present; formalize).
- Bottom row converted from status-bar-only to F-key legend with the same data density (row/scroll indicator stays on the right).
- `/MONO` flag to force monochrome regardless of adapter detection.
- Adapter-tier waterfall at startup using existing display detection plus the MDA/Hercules 3BAh-toggle gate already partially present in `display.c`. No new rendering paths; this just refines tier selection.
- Enable `AX=1003h BL=00h` for 16-background-color mode on EGA/VGA. Zero-cost quality win.

**Out of scope.**
- Menu bar (top row). Any dropdown menu. Any modal dialog system.
- Turbo Vision-style window framing around existing content.
- DAC-gradient title bars.
- 80x50 dense mode. Deferred; needs its own validation across adapters.
- 132-column SVGA. Out.
- Themed palette presets. Out.
- Touching intro, visual journey, or scrollable summary structurally. Bug fixes only.

**Exit gate.**
- Every screen navigable by CUA keys only (F1, F3, Esc, arrow keys, PgUp/PgDn, Home/End, Q as legacy alias).
- Bottom legend renders cleanly on all adapter tiers (MDA, Hercules, CGA, EGA, VGA).
- **No visible snow on IBM CGA during legend redraws.** Row 24 repaints on every context change (help overlay open / close, scroll-position update, screen transition). Every write to the legend row goes through the retrace-gated VRAM path in `tui_util.c`; if that path does not already gate on 3DAh bit 0 before each write, this gate forces the fix. Validated on a genuine single-ported IBM CGA (not a dual-ported clone).
- Monochrome rendering pixel-correct on MDA capture.
- No visual regression on any previously-validated capture.
- DGROUP impact ≤ 1 KB.

### M4: Documentation Parity, Release Readiness, and Tag

**Purpose.** Bring public-facing documentation up to what the code actually does, produce the full release notes, tag.

**In scope.**
- `CERBERUS.md` master spec rewrite to match 0.8.0 reality. Every "Stubbed" or "Partial" row is either "Implemented" or "Deferred to 0.8.1+."
- `README.md` rewrite. Status block reflects 0.8.0. "What does not work yet" section explicit. Public support-claims matrix matches real-iron captures (see §10).
- `CHANGELOG.md` v0.8.0 entry.
- `docs/methodology.md` covers every new probe.
- `docs/consistency-rules.md` covers possible-causes narration.
- `docs/ini-format.md` enumerates new keys.
- `docs/CERBERUS.md` §"Why real hardware" extended with non-486 findings from M1 captures.
- Tag `v0.8.0`.

**Out of scope.** Any code change. Any scope creep. If a ship-blocking defect surfaces in M4, it is fixed and a v0.8.0-rc is iterated; M4 does not pull in other work.

**Exit gate.** Tag pushed. Release notes posted. `tests/captures/` contains the validation corpus. README builds the claim hierarchy from the captures, not from aspiration.

---

## 7. Revised Position on Whetstone

**Decision. Whetstone is removed from the default INI emit in 0.8.0. The kernel stays compiled for build gate `wmake WHETSTONE=1` and for the issue #4 archaeological record; stock builds do not produce a `k_whetstones` number.**

**Justification.**

The evidence has been consistent across four release cycles. The pre-asm Watcom C kernel ran at ~109 K-Whet on BEK-V409, roughly 10-30x below the published Curnow-Wichmann envelope of 1,500-3,000 K-Whet for a 486 DX-2-66. The v0.5.0 asm rework aimed to close that gap but has not been validated to do so. The v0.7.2 session established that on real iron one Whetstone unit costs 50-100 ms, where research estimated 1-3 ms — a 30-50x per-unit-cost anomaly that nobody has root-caused. The homage research (Phase 3, `checkit-whetstone-version.md`) eliminated CheckIt's 11,420 as a target: it is a custom synthetic, not a Curnow port. The authentic published reference is the 1,500-3,000 K-Whet range, and CERBERUS has not yet produced a value in that range under any build.

A trust-first release cannot ship a CONF_HIGH value 10x or more out of range for its hardware. The CONF_MEDIUM or CONF_LOW escape hatch does not save it; community users read numbers, not confidence labels. A number that looks broken is more damaging than a missing number. The first-draft hedge ("retire or reframe") was the wrong choice; it left the decision to be re-litigated at every subsequent session.

The replacement story for FPU throughput already exists: `bench_fpu.c` emits `fpu.ops_per_sec` (an aggregate mix of FADD/FMUL/FDIV/FSQRT on the stack) at ~1.17M on BEK-V409, which is plausible and reproducible. Combined with the v0.7.1 behavioral fingerprint (infinity mode, pseudo-NaN, FPREM1, FSIN) and the M2 additions (FPTAN, rounding modes, precision modes, IEEE-754 edges, exception roundtrip), the FPU story is complete without Whetstone. CERBERUS reports that the FPU is present, which family it behaves like, that it rounds and signals correctly, that it handles IEEE-754 edges per the published tables, and what its aggregate throughput is. No K-Whet number is needed to tell that story.

**What this means concretely.**
- `bench_whetstone.c` gains a build-gate check at entry. In stock builds, `whetstone_status=disabled_for_release`, no K-Whet row emitted.
- `docs/methodology.md` gets a "Why Whetstone is not in 0.8.0" note documenting the methodology gap and the 0.9.0+ direction (per-instruction FADD/FMUL/FDIV/FSQRT cycle-count microbench).
- Consistency Rule 10 (`whetstone_fpu`) is updated to treat `disabled_for_release` as a rule skip, not a FAIL.
- Issue #4 is explicitly reframed: the target for 0.9.0 is not "make Whetstone match a published reference" but "replace Whetstone with per-instruction microbenchmarks and retire the synthetic."

This is cleanly reversible if a future pass roots out the per-unit-cost anomaly. The kernel is not deleted; it is retired. A future session can re-enable with `wmake WHETSTONE=1`, capture on real iron, compare against published references, and ship the re-enable in 0.9.0 if and only if the kernel produces an in-envelope number. That work does not block 0.8.0.

---

## 8. Revised Position on Upload / Community Data Collection

**Decision. Upload execution is compiled out of stock 0.8.0 binaries via `#ifdef UPLOAD_ENABLED`. Stock builds contain no HTTP client code path, no HTGET shell-out, no network transmission logic. Only network detection and local INI annotation remain.**

**What stays in the stock binary:**
- `src/detect/network.c` — probes for packet driver, mTCP, WATTCP, NetISA presence. Populates `[network] transport`. This is diagnostic information with value on its own, independent of any upload.
- `/NICK:<name>` and `/NOTE:<text>` flags. They populate local INI annotations, which is meaningful even offline. A user running CERBERUS on ten machines can tag each run locally and correlate them by hand or via the server after deployment.
- `docs/ini-upload-contract.md` as forward-looking design.
- `[upload]` section in the INI, populated with `status=not_built` by default. Server-side parsers that honor the contract will handle this value correctly (server MUST tolerate unknown status values per the contract).

**What is `#ifdef`-gated and excluded from stock:**
- `upload_execute()` in `src/upload/upload.c` — the prompt, the HTGET invocation, the response parse, the INI re-write on success.
- The `/UPLOAD` flag in `main.c`. In stock builds, passing `/UPLOAD` prints `upload not built into this binary; rebuild with 'wmake UPLOAD=1'` and exits with usage error code.
- The `/NOUPLOAD` flag remains accepted as a no-op in stock builds for backward-compatibility with scripts.
- `UPLOAD.TMP` file handling.
- Every path that could stack-overflow on unreachable endpoint is excluded.

**Justification.**

The upload feature's failure mode — stack overflow when the endpoint is unreachable — is the unacceptable condition in a trust-first release. The endpoint is not yet deployed. First-impression risk is irreducible while this condition holds. "Disable by default" leaves the stack-overflow in the default code path, which makes the crash one argv away. "Compile out" eliminates the crash as a code-presence fact, not as a configuration fact.

The cut is cleanly reversible. When `barelybooting.com` goes live, when a health-check circuit breaker is added, when the HTGET dependency is either eliminated or documented as a hard prerequisite, and when at least one non-BEK-V409 machine has uploaded and round-tripped successfully, `wmake UPLOAD=1` produces a shipping 0.9.0-capable binary. The contract doc, the network detection, the INI annotation, and the flag-parsing skeleton all survive the cut. No architectural work is thrown away.

The NetISA long-term story (HTTPS via TLS 1.3 card) remains a valid 1.0.0 direction. Nothing in this cut prevents it.

**What this means for the stock release experience.** A user running CERBERUS with default flags on a networked machine sees `[network] transport=pktdrv` or equivalent in their INI, reads the README section explaining that 0.8.0 does not transmit, and either (a) reviews the INI locally, or (b) manually submits it to a future community database when that exists. No crash. No preview-flag footgun. No partially-working feature to explain.

---

## 9. Revised Position on UI/UX for 0.8.0

**Decision. 0.8.0 ships CUA-lite. Keybindings and bottom F-key legend only. No menu bar, no dropdowns, no framed windows, no modal dialog system.**

Your bias was correct. Here is the justification from the codebase:

Look at what already exists. `ui.c:ui_render_summary` paints a scrollable viewport with Up / Down / PgUp / PgDn / Home / End / Q / Esc navigation. That is already CUA-adjacent on the navigation axis — only Q (WordStar-era) is non-CUA, and it can stay as a legacy alias. `head_art.c` renders the section head-art with adapter-aware color. `display.c` + `tui_util.c` provide the VRAM primitives. `intro.c` runs the ANSI splash. `journey.c` runs the visual demonstrations with per-module poll-skip (0/1/2 meaning) logic. None of this needs framing. It needs a wire of F1 to help and a row-24 rewrite.

A full CUA shell — menu bar, dropdown menus, modal help window, status-line with zones — would require:
- a menu-bar paint module with Alt+letter access-key dispatch,
- a dropdown-menu paint module with Up / Down / Esc / Enter state,
- a help-window paint module with its own scroll viewport,
- ~2-4 KB of menu labels and help text in DGROUP (against 3-4 KB headroom),
- host-test coverage for every new state,
- adapter-tier testing for every new state,
- retesting that none of it breaks the already-validated intro / journey / summary sequence.

The cost is real. The benefit — "feels like a DOS application" — is largely captured by just wiring CUA keybindings to the existing viewport and adding a proper F-key legend at row 24. Users who open CERBERUS will press F1 expecting help, F3 or Esc expecting exit, arrows expecting navigation. Getting those three right closes most of the interaction-grammar gap without any frame work.

**What CUA-lite ships.**

- **F1 = help overlay.** Implemented as a call into the existing scroll viewport with a per-screen help text array. One static string per section header currently in the summary. Cost: one new module function, ~1-2 KB DGROUP (if disciplined) or equivalent FAR data.
- **F3 = exit to DOS.** Non-conflicting with any existing binding.
- **Esc = exit.** Already works in the summary viewport; formalized across all screens (intro, journey, summary).
- **Arrow keys, PgUp / PgDn, Home / End** already work in the summary viewport.
- **Q** stays as a legacy alias for quit.
- **Alt+letter** is not wired (no menu bar to access), which is a known CUA gap accepted for 0.8.0.
- **Bottom row 24 redesigned.** Current: scroll position + "Q/Esc exits." Revised: Norton-style F-key legend with F1 Help, F3 Exit, PgUp/PgDn scroll, Home/End jump, plus the scroll-position indicator pushed to the right edge. Colors: digit bright-white-on-black, label black-on-cyan (Borland orthodox).
- **/MONO flag** forces monochrome. Semantic role mapping follows the nine MDA-valid attributes documented in `MS-DOS UI-UX Research.md` Part B Tier 0: **07h** normal body text, **0Fh** bright / bold (emphasis, section heads), **01h** underline (links, key legend labels), **70h** reverse video (selection bar, current row), **F0h** blinking reverse (alerts and errors), **00h / 08h** invisible (reserved; never used for content). No other attributes are emitted under `/MONO`. CP437 box-drawing carries geometry; underline and reverse carry emphasis. Pinning to the table up front avoids the "why does the help overlay look wrong on MDA" debug cycle.
- **16-background-color mode** enabled via `AX=1003h BL=00h` at startup on EGA/VGA. Zero DGROUP cost; makes the existing head-art color scheme render closer to intent.
- **Adapter-tier waterfall** refined per `MS-DOS UI-UX Research.md` Part B: `INT 10h AH=1Ah → INT 10h AH=12h BL=10h → INT 11h → 3BAh-toggle`. The existing `display.c` already does most of this; 0.8.0 tightens the MDA vs Hercules distinction.

**What CUA-lite does not ship.** Menu bar. Dropdown menus. Modal help window (help is an overlay of the existing scroll viewport, not a new modal class). Dialog boxes. 80x50 mode. 132-column SVGA. Themed DAC gradients. Any change to the intro, the visual journey, or the three-heads head art.

**Exit gate.** A DOS user from Borland IDE, Norton Commander, or MSD can navigate CERBERUS's summary by press F1 for help, press F3 or Esc to exit, press arrow keys and PgUp/PgDn to scroll, press Home/End to jump. That is the grammar they already know. 0.8.0 closes that gap and defers the menu-bar convention to 0.9.0+.

---

## 10. Revised Validation Doctrine

Each capture proves something specific. These are not interchangeable. Public support claims must match the captures, not the wish list.

### What 486 validation proves

Baseline. BEK-V409 (486 DX-2-66, AMI BIOS 11/11/92, S3 Trio64, Vibra 16S, 63 MB XMS) has been the reference platform since v0.1. Proves:

- CPU detection via CPUID + instruction probing on Intel 486DX family.
- Integrated-FPU detection via FNINIT / FNSTSW sentinel.
- Memory detection via INT 15h AX=E801h (HIMEM-intercepted path) and XMS AH=08h.
- S3 Trio64 chipset ID via CR30, past the "IBM VGA" option-ROM string collision.
- Vibra 16S DSP detection at base+0x0E.
- OPL3 probe at BLASTER+8, with CTCM-disabled-mirror fallback.
- PIT Channel 2 timing with the UMC491 latch-race guard active.
- VLB bus detection (currently heuristic, not probed).
- Mann-Kendall thermal trend test across 7-run calibrated mode.
- 11 consistency rules firing with PASS on a healthy machine.
- Cache characterization (L1 size = 8 KB, line = 16 B, write-back inferred).
- FPU behavioral fingerprint: affine / accepts / fprem1 / fsin.

**What the 486 capture cannot prove.** Pre-CPUID CPU detection paths (8088, 8086, V20, 286, 386). Legacy FPU socket detection (287, 387, RapidCAD, IIT). Behavioral-fingerprint divergence from 387+ (infinity_mode=projective expected on 8087/287). `extmem_cpu` rule (Rule 6) FAIL branch (8088 with non-zero extended = impossible, a rule that only fires on a misreport). Writable cache-absent verdict. XT-class bus signaling. Slave DMA absence.

### What 386 validation proves

A 386DX with external 387 (or RapidCAD / 386SX / Am386). Proves:

- CPU detection via instruction probing in the absence of CPUID on some steppings, or via CPUID on later 386s if present.
- 387 external FPU detection, including socket-empty vs socket-populated discrimination.
- FPU behavioral fingerprint on 387: expected `affine / #IE / fprem1=yes / fsin=yes`.
- 32-bit register path coverage in the timing code (TSC-gate will be absent on pre-Pentium CPUs; PIT path exercised).
- Memory detection across a typically-16-MB extended region.
- Bus detection on ISA-16 (no VLB unless a rare 386DX-VLB board).
- Different cache topology: 386 has no on-chip cache, any cache is external.
- 386-era BIOS (AMI, Award, or Phoenix) detection, including cases where the 11/11/92 AMI string does not match.

**What the 386 capture cannot prove.** 8088-specific pre-286 code paths. XT-class bus and DMA. MDA / CGA on a historically-authentic 386 config (rare but possible).

### What 286 validation proves

An IBM PC/AT or clone with 286 and 287 (or empty socket). Proves:

- Instruction-probing path for 286 detection (no CPUID on 286, no 32-bit registers).
- Optional 287 detection (socketed FPU, possible empty socket).
- Extended memory via INT 15h AH=88h, with typical 1-16 MB range.
- PIT Channel 2 on original 8254 (not UMC491 clone).
- AT-class bus (ISA-16) with slave DMA present.
- RTC present via CMOS port 70h/71h (unlike original XT).
- Adapter probably EGA or CGA.
- Keyboard controller 8042-class (different from XT 8255).

**What the 286 capture cannot prove.** 8088 / 8086 / V20 / V30 instruction-probe branches. XT-class bus. Slave DMA absence. 4.77 MHz BIOS-tick-overflow cases. MDA-only displays (though EGA-class can do MDA mode). The 8087 socket path on the pre-286 era.

**Critically.** A 286 is on the **AT side** of the XT/AT architectural boundary. Honoring the 8088 floor claim requires more than a 286 capture.

### What 8088 validation proves

An IBM PC or PC/XT or clone with 8088 (or 8086 / V20 / V30), 256 KB to 640 KB conventional, MDA or CGA, ISA-8 bus. This is the architecturally-critical capture, not "one more old machine." Proves:

- True pre-CPUID CPU detection on the floor target.
- Probably an empty 8087 socket (most home PCs shipped without); exercises the "no FPU present" path.
- If a V20 or V30 is installed, exercises the NEC extended-instruction-set discrimination.
- `extmem_cpu` consistency rule (Rule 6) PASS branch (extended_kb=0 on 8088 is expected).
- `dma_class_coherence` rule (Rule 11) XT-class branch (channels 5-7 skipped as `skipped_no_slave`, no WARN).
- `8086_bus` rule (Rule 9) PASS branch (ISA-8 expected).
- Cache characterization skip path (cache.present=no on 8088).
- Mono-display path in `ui.c` (B000 VRAM, 9-attribute-values constraint, blink enable / intensity gate).
- Adapter detection waterfall MDA and CGA branches.
- `timing_compute_dual` on an actual 4.77 MHz 8253 with BIOS-tick overflow edge cases.
- Scrollable summary rendering on 80x25 with slow REP STOSW (no VGA fast path; visual regressions here are real regressions).
- `intro.c` splash on non-VGA adapters (adapter-aware fallback).
- Crumb / LAS round-trip on a machine with no XMS (buffer must live in conventional).
- `bench_memory` against a 4.77 MHz ISA-8 bus: measured bandwidth should land in a known pre-AT envelope.
- `bench_dhrystone` on 8088: expected iterations per second in the pre-286 envelope; any unusual number flags a defect.
- 8088-specific arithmetic quirks not normally exercised: PUSH SP semantics (8088 pushes old SP, 286+ pushes new SP) — any code in CERBERUS that depends on PUSH SP behavior breaks here, silently, unless the capture surfaces it.
- DOS INT 21h AH=40h file-write performance under real BIOS interrupts at 4.77 MHz.

**Subsystems most likely to break on 8088.** The consistency engine's rule traversal under a reduced key set. The bench-mode calibration math on a machine where even minimal iterations take seconds. The crumb flush timing if any path assumes >1 MHz INT 21h. The intro splash if any path assumes VGA DAC. The ANSI / color handling on mono. The `detect_cpu` instruction probe if the fallback path has never been real-iron tested. `bench_whetstone` status is `skipped_no_fpu` — this is fine, but verifies that the skip emits cleanly.

**Can the 8088 floor claim survive 0.8.0 without an 8088 capture? No.**

The claim that CERBERUS runs on an 8088 with 256 KB is load-bearing for the project's identity. It appears in the master spec, the README, the Why-real-hardware section. Shipping 0.8.0 with "proven on 8088" as a claim but no captured evidence is the exact trust-damage pattern the release is meant to correct.

The honest claim hierarchy at 0.8.0 tag:

- **With 486 + 386 + 286 captures only, no 8088:** "Real-hardware-proven from 286 through 486. 8088 is the design floor; real-hardware validation pending." That sentence, verbatim, in the README. No asterisk. No "should work."
- **With 486 + 386 + 286 + 8088 captures:** "Real-hardware-proven across the full 8088-to-486 target range."

Both are honest. The second earns the floor claim. The first does not and does not pretend to. A 0.8.0 that ships with only the first three captures is still a meaningful release; it is just more modest about what it has proven. If the 8088 capture cannot be produced in the 0.8.0 window, the floor claim retracts gracefully and 0.8.1 earns it back.

### What public support claims are honest before and after each of those captures

| Captures archived | Honest README claim |
|---|---|
| 486 only (current state) | "Developed and validated on 486. 8088-286-386 paths present but not yet hardware-validated. Use at your own risk on other hardware." |
| 486 + 386 | "Validated on 386 and 486. 286 and 8088 paths untested." |
| 486 + 386 + 286 | "Validated on 286 through 486. 8088 is the design floor; XT-class validation pending." |
| 486 + 386 + 286 + 8088 | "Real-hardware-proven across the 8088-to-486 target range." |

This table should appear in the 0.8.0 README, updated as captures land.

---

## 11. Final Judgment

### What will make 0.8.0 respected

- **The four-capture validation corpus, diffed honestly against ground truth.** A retro enthusiast will look at `tests/captures/` before they read anything else. Four captures, each with a ground-truth README, each with matched INI, is the single most persuasive artifact the project can produce.
- **A release whose default-flag run never crashes on any machine in the validation corpus.** No `/SKIP:` workaround needed, no `/NOUPLOAD` needed, no `/NOUI` needed.
- **No number in any default-run INI that looks obviously wrong.** Whetstone's removal is the concrete form of this principle.
- **Upload compiled out means no first-run crash on a historically-valuable machine.** First impressions are the release's currency.
- **A README that tells users what does not work, in short clear words.** The retro community rewards tools that disclose their gaps and mistrusts tools that pretend they have none.
- **CUA-lite shell that makes navigation feel like a DOS application.** F1 and F3 wired correctly closes most of the interaction-grammar gap with near-zero architectural cost.
- **The consistency engine demonstrably firing on hardware that should fire it.** At least one real capture where a rule WARNs or FAILs on a real condition (a cache-disabled BIOS, a 486SX falsely ID'd, a divergent PIT), with the narration explaining the finding, turns the consistency engine from a plausible idea into a proven instrument.
- **Documentation that matches the code.** The v0.7.0-rc2 README / v0.7.1 code drift is the pattern to break. Ship tagged with parity.

### What would most likely sink trust if mishandled

- **Shipping a `k_whetstones` value that is 10x out of range.** Even behind CONF_MEDIUM. Even behind a documented caveat. The number is the evidence; the caveat is the excuse.
- **Compiling upload into the default binary and letting `/UPLOAD` crash on an unreachable server.** One video of CERBERUS crashing on a vintage 486 over network kills the release.
- **Shipping 0.8.0 with "8088 is the floor" in the README and no 8088 capture in the tree.** The community will check. The `tests/captures/` directory is the single point of verification and the first place a skeptic will look.
- **Breaking the DGROUP budget and shipping an EXE that cannot load on an 8088 with the driver stack the community actually uses (HIMEM + EMM386 + mouse driver + TSR network stack leaves typical conventional memory around 520-580 KB).** Every KB the binary grows costs a user.
- **A CUA-lite shell that half-wires F1 help or leaves F3 inconsistent with Esc.** Half-done CUA is worse than no CUA; it primes expectations that then get violated.
- **Adding one more visual demo to the journey.** The existing seven are sufficient. Another one is costume bloat and absorbs validation budget that should go to real iron.
- **Pretending the crumb-coincidence EMM386 fix was understood.** If the side effect is load-bearing and the team does not name it, 0.8.1 will break it by accident.

---

*Revised plan, final. No further code changes until scope decisions in §4, §5, §7, §8, §9 are accepted or countered. The four-capture corpus in §10 is the validation contract. Tag conditions in §4 are pass/fail. Every other decision follows.*
