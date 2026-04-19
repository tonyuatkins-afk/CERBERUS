# CheckIt 3.0 Comparison and Adoption Targets

> **Status:** Plan document, not a committed roadmap. Source of adoption targets for v0.4 bench expansion and v0.5 UI polish. Re-visit alongside the v0.4 phase re-plan.

## 1. CheckIt as reference point

CheckIt (TouchStone Software, DOS versions 3.0 and 4.0 in the early 1990s) is the most-widely-recognized DOS-era hardware diagnostic and benchmark suite. It established the vocabulary the DOS scene still uses when talking about "how fast is this box":

- **Dhrystones** for integer throughput.
- **Whetstones** for floating-point throughput.
- **Times IBM PC-XT** as a human-readable relative-performance ratio.
- **Subsystem bar graphs** against a fixed reference machine.

CheckIt wasn't doing anything CERBERUS couldn't — it was running PIT-timed workloads and comparing to a lookup table — but it was one of the first tools to package those numbers in a way period-correct technicians could reason about at a glance. Every DOS benchmark tool since has either adopted or explicitly differentiated against CheckIt's conventions. CERBERUS does both.

### Reference run — 2026-04-18 on the CERBERUS bench box

CheckIt 3.0 on the same BEK-V409 / i486DX-2 / S3 Trio64 / Vibra 16S box CERBERUS is validated against:

| Metric | Value |
|---|---|
| CPU detected | 80486, 66.74 MHz |
| Dhrystones | 33,609 |
| Whetstones | 11,419,900 (11,419.9 K) |
| Integer ratio (× IBM PC-XT) | 97.70 |
| Math ratio (× IBM PC-XT) | 1,730.29 |

These are the canonical comparison values for anything CERBERUS adopts from CheckIt's methodology. A CERBERUS run on the same box producing materially different numbers is a self-check fail, not a feature.

## 2. Features worth adopting

### 2.1 Dhrystone 2.1 — v0.4 (replaces one of the deferred benchmarks)

**What it is.** Reinhold Weicker's Dhrystone 2.1 is the industry-standard integer-throughput benchmark from 1988. Well-documented, widely reproducible, in the public domain. Output is raw Dhrystones-per-second.

**Why CERBERUS should ship it.** The current `bench.cpu.int_iters_per_sec` is a fixed instruction mix we wrote ourselves. It is reproducible across our own runs and lets Rule 4b compare against a DB-seeded empirical range — but it produces a number nobody outside CERBERUS has a reference point for. Dhrystones are the lingua franca. A vintage-PC operator asking "is my 486 healthy?" can compare CERBERUS's Dhrystones directly against period reviews in old PC Magazine archives without needing to trust our custom benchmark.

**Cost.** Dhrystone 2.1 is ~600 lines of portable C. Watcom will compile it to real-mode 16-bit without modifications. Runs in under 2 seconds at 486 speeds with enough iterations for PIT-scale resolution.

**Replaces.** One of the two deferred v0.4 benchmarks. Either the cache-bandwidth benchmark (harder — needs cache-size inference) or the video-throughput benchmark (easier — direct VRAM write). Dhrystones displace cache-bandwidth from the v0.4 target; cache becomes a v0.5 concern that lands alongside Rule 8.

### 2.2 Whetstone — v0.4

**What it is.** Harold Curnow's Whetstone benchmark, public domain since 1972, specifically designed to exercise floating-point throughput with a realistic mix (transcendentals, multiplication, addition). Output is K-Whetstones-per-second.

**Why CERBERUS should ship it.** Our current `bench.fpu.ops_per_sec` is a custom mix that produces a CERBERUS-specific number. Whetstones are the FPU equivalent of Dhrystones in terms of scene recognition. A user reporting "my 487 is scoring 2K Whetstones on a 486SX, is that right?" is asking a question with a well-documented answer; CERBERUS should speak that dialect.

**Cost.** ~400 lines of portable C. Compiles under Watcom `-fpi` (inline 8087 emulation) so it runs on 8088/V20 FPU-socket configurations, not just integrated FPUs.

**Addition, not replacement.** `bench.fpu.ops_per_sec` stays as the internal CERBERUS reference metric (feeds Rule 5 fpu_diag_bench). Whetstones become the second FPU bench row, fed by the same measurement loop's outer wrapper.

### 2.3 PC-XT relative rating — v0.4

**What it is.** "Times IBM PC-XT" across CPU / FPU / memory. A single scalar per subsystem where 1.0 is the original 4.77 MHz 8088 reference. CheckIt ships with a fixed reference baseline and divides measured throughput by it.

**Why CERBERUS should ship it.** Scene-readable. "My 486 is 97× PC-XT" is more immediately legible than "my 486 does 33,609 Dhrystones" to anyone who didn't benchmark PCs professionally in 1993. Also feeds the bar-graph UI (2.4) with natural data.

**Cost.** Three divisions per run. The reference baseline is three constants in a header: the measured PC-XT Dhrystones, Whetstones, and memory bandwidth. Easy.

**Subtle risk.** CheckIt's PC-XT reference was measured in ~1990 on an actual vintage IBM PC-XT that TouchStone owned. We don't have that hardware. Options:

- **Import CheckIt's reference numbers.** Zero risk of measurement error on our side, but means CERBERUS's ratio is literally the CheckIt ratio with different measurement precision — we become a CheckIt replacement that uses CheckIt's reference data.
- **Measure our own PC-XT reference on an 8088 bench box.** More authentic, but the numbers will differ from CheckIt's by a few percent (different 8088 speed grade, different memory speed, different BIOS) and every community comparison has to know which reference is in use.
- **Hybrid: use CheckIt's numbers with full credit in the docs,** optionally allowing `CERBERUS.INI` operators to override via env var for their own reference hardware.

Decision deferred to v0.4 re-plan. Hybrid is the likely default.

### 2.4 Bar-graph comparison UI — v0.5 (post-UI-hang)

**What it is.** CheckIt's signature visual: for each benchmarked subsystem, render a horizontal CP437 bar graph showing `<this machine>` relative to `<reference class>`. Reference-class dropdowns in CheckIt include "IBM PC-XT", "IBM PC-AT", "386-25", "486-33". Instant visual grasp of "where does this box sit in the DOS-era hierarchy."

**Why CERBERUS should ship it.** The three-pane summary UI mentioned as "still pending" in README's status table is the architectural home for this. Once the current UI hang is resolved (one way or the other — fix or close-as-intermittent), the bar-graph renderer is a focused follow-up that reuses the CP437 box-drawing primitives already in `display.c`.

**Cost.** ~150 lines for the bar-graph renderer + reference-class table. Uses `display_putc` + `display_goto` — no new primitives.

**Blocker.** UI hang resolution. Adding new rendering to the exit path while the existing rendering is under active investigation is asking for compounded debugging pain.

## 3. Consistency rule candidates enabled by Dhrystone / Whetstone

### 3.1 Rule candidate: Dhrystone-implied MIPS vs detected CPU class

Dhrystone scores have well-documented ranges per CPU family. 33,609 on a 486 DX2-66 lands in the published band for that chip (reference: PC Magazine 1993 round-up, ~35K for 486 DX2-66 with no TSRs). An 8086 returning 35K Dhrystones is electrically impossible — something is either misidentifying the CPU or miscounting iterations.

Structurally similar to Rule 4b (`cpu_ipc_bench`) but with a CheckIt-compatible metric, which makes it useful for cross-validating against other community-run tools rather than just against CERBERUS's own history.

**Seed data comes from the CheckIt reference run.** Future community submissions populate more per-CPU-class bands.

### 3.2 Rule candidate: Whetstone success vs FPU detection consistency

If `fpu.detected` is "integrated-486" or "387" or other positively-present-FPU value, the Whetstone benchmark should return a nonzero result. If it returns zero, either the FPU detection is lying (DB bug, counterfeit card) or the Whetstone path ran without an FPU and fell through to software emulation (in which case the measured K-Whetstones will be several orders of magnitude lower than the CPU-class expected band).

Extends Rule 5 (`fpu_diag_bench`) with a scene-readable throughput axis. Implementation-wise: same find-both-keys-or-no-op structure as Rule 5.

### 3.3 Composite rule candidate: PC-XT ratio vs detected class

CheckIt's integer ratio (97.70×) on a 486 DX2-66 maps to an expected band. Anything outside the band — throttle, TSR contention, counterfeit — is a WARN.

This is the CheckIt-compatible form of Rule 4b. Whether to ship both or to collapse them is a v0.4 re-plan decision; at minimum, both axes (raw Dhrystones and × PC-XT) should be *reported* so downstream consumers can cross-check against either convention.

## 4. CERBERUS differentiation from CheckIt

Four axes where CERBERUS is structurally better than CheckIt and where marketing / README copy should lean in:

### 4.1 Cross-validation engine

CheckIt reports numbers. It does not cross-check them. If CheckIt's CPU detection is wrong and its Dhrystone score is wrong in a way that cancels out, the user sees a plausible-looking result. CERBERUS's consistency engine (nine rules today, with the three CheckIt-adoption rules above as immediate candidates) makes the two heads audit each other. Every measurement row carries confidence; every inconsistency is surfaced as PASS / WARN / FAIL with a human-readable reason.

### 4.2 Open database

CheckIt's CPU identification table, reference baseline, and benchmark methodology were closed. Contributing a new CPU to CheckIt's DB required sending physical hardware to TouchStone Software and waiting for the next commercial release. CERBERUS's `hw_db/*.csv` is human-editable, with a Python regeneration step and a one-command contribution path. The unknown-hardware capture flow (`CERBERUS.UNK` → GitHub issue → PR adds a DB row) is a first-class feature.

### 4.3 Active community contribution path

Tied to 4.2: CheckIt's community was commercial-customer feedback letters. CERBERUS's community is VOGONS threads, GitHub issues, and the NetISA upload channel (v0.6). The feedback loop is hours, not shipping cycles.

### 4.4 Machine-readable output

CheckIt produced a printable report. CERBERUS produces an INI — structured, versioned (`schema_version`), and with two independent signature fields (`signature` for hardware identity, `run_signature` for record identity). Any tool downstream of CERBERUS can parse and correlate runs without screen-scraping.

Downstream consumers envisioned:
- VOGONS hardware-of-the-week threads pulling INIs from multiple community members.
- The NetISA-uploaded database doing counterfeit detection by comparing multiple runs at the same `signature` with divergent benchmarks.
- A future web dashboard at barelybooting.com that charts community-submitted runs over time.

None of that works with printable reports. All of it falls out of INI by default.

## 5. Activity log mode — v0.7+ consideration

CheckIt has a persistent activity log ("burn-in test mode") that runs benchmarks in a loop over hours, logging any thermal / stability drift. Useful for shaking out failing-cap and marginal-regulator symptoms that a 30-second CERBERUS run won't catch.

Proposed `/LOG:<minutes>` flag for v0.7+:

- Runs calibrated mode on a schedule (every N minutes for M hours).
- Appends to `CERBERUS.LOG` (human-readable) and `CERBERUS.INI.LOG` (ndjson, one INI per iteration).
- The thermal-stability tracker (already live at v0.5, `d5e7400`) becomes load-bearing in this mode — Mann-Kendall trend detection over 20+ passes catches drift a single run cannot.
- Long-run mode means CERBERUS must survive its own success: the INI writer, signature calculator, and crumb path all need to tolerate being re-invoked dozens of times per run. Most of them already do; any that don't get fixed as prerequisites to `/LOG`.

Not v0.2. Not v0.3. Candidate for v0.7 after the NetISA upload path (v0.6) is live — the two features are mutually enabling: `/LOG` produces the volume of data that makes upload interesting, and NetISA provides the authenticated channel so a 3-hour run isn't wasted by a transient network blip.

## 6. Where this plan lands in the roadmap

- **v0.2-rc1 (this RC).** CHANGELOG references this doc as a known future direction. No code changes required.
- **v0.4 phase re-plan.** Dhrystone 2.1, Whetstone, and PC-XT ratio adopted as part of bench expansion. `bench.cpu.dhrystones` and `bench.fpu.k_whetstones` land as INI rows. Rules 3.1 / 3.2 / 3.3 seeded with DB-backed expected bands.
- **v0.5 phase re-plan.** Bar-graph comparison UI lands as the three-pane summary polish, contingent on UI-hang resolution from session 2026-04-18 (see `docs/sessions/SESSION_REPORT_2026-04-18-evening.md`).
- **v0.7+.** `/LOG` activity-log mode contingent on NetISA upload channel being live and usable.

Re-read this document at the start of each phase re-plan. If CheckIt-compat adoption has drifted out of scope, that is a decision that should be written down explicitly in the phase re-plan, not allowed to drift by silence.
