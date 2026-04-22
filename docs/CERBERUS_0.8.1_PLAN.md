# CERBERUS 0.8.1 Plan

Drafted: 2026-04-22. Follows v0.8.0 tag. 0.8.1 is the completion release: picks up the items explicitly deferred from 0.8.0 to 0.8.1 per the v0.8.0 release notes, plus the known real-iron issues that have been localized but not root-caused.

---

## 1. Identity

**0.8.1 is the completion release, not feature-expansion.** 0.8.0 shipped fewer numbers than 0.7.1 on purpose (Whetstone gated, upload compiled out) and every number it did ship has a real-iron signature under it. 0.8.1 restores the coverage that 0.8.0 deferred, without backing off any of the trust-first discipline that earned the 0.8.0 tag.

**Three governing principles, carried forward from 0.8.0:**

1. **Trust beats reach.** Same rule. Every feature ships with real-iron validation on at least two distinct hardware generations before the 0.8.1 tag.
2. **Near-data remains a budget.** DGROUP stands at 60,976 / 64,000 at v0.8.0 tag, 2,512 bytes headroom vs the 62 KB soft target. 0.8.1 scope fits within that or reclaims before adding.
3. **Cuts stay cut.** Whetstone emit stays suppressed. Upload path stays compiled out. 0.8.1 does not reverse 0.8.0 decisions; it adds orthogonal capability.

---

## 2. What Deferred from 0.8.0

Per `docs/releases/v0.8.0.md` "What is deferred" section, the 0.8.1 inbox is:

**From M2 scope-cut (code was partially planned, DGROUP/time pressure deferred):**
- **L** IEEE-754 edge-case diagnostic (9 operand classes x 5 ops).
- **H** `/CSV` output mode alongside INI.

**From research-gap inventory (never scheduled into 0.8.0):**
- **A** L1 pointer-chase latency probe (ns/access).
- **B** L2 cache detection via 64/128/256 KB working-set sweeps.
- **E** DRAM latency derivation from the largest sweep.

**From DB gap inventory:**
- IIT 3C87 FPU discrimination (386 bench box has one, currently mis-tagged as Intel 80387).
- Genoa ET4000 video chipset ID (currently detected as generic SVGA).
- Hercules variant discrimination: HGC, HGC+, InColor.

**From real-iron defect inventory:**
- BEK-V409 BSS overwrite root-cause investigation (empirically localized to 3 probe paths; needs focused session with debugger).

---

## 3. Milestone Structure

Four milestones, ordered by risk and dependency. M1 and M3 are pure additive-code work with DGROUP impact; M2 requires FAR-buffer discipline and real hardware; M4 requires BEK-V409 hands-on access.

### M1 — Precision and output completion (code-only, host-testable)

Closes the two items explicitly deferred from M2-of-0.8.0. Both are self-contained, both have well-defined test surfaces, both fit within a ~1 KB DGROUP budget.

**M1.1 IEEE-754 edge-case diagnostic (research gap L).**
- Extend `src/diag/diag_fpu.c` with an edge-case suite: 9 operand classes ({+0, -0, +inf, -inf, QNaN, SNaN, +denormal, -denormal, normal}) as inputs to FADD/FSUB/FMUL/FDIV plus unary FSQRT.
- For each combination, a canonical IEEE-754 result bit-pattern (or "exception expected" tag). Store 10-byte tword results and compare bytewise to gold constants.
- SNaN class is skipped on pre-387 FPUs (8087/287 silent-accept). Guard on `fpu.family` inferred token.
- New `diag/diag_fpu_edges_a.asm` holds the tword-result helpers; C driver enumerates the cases and emits per-class pass/fail rows.
- Emit `diagnose.fpu.edge.<class>_<op>=ok|bad` per case; aggregate `diagnose.fpu.edge_cases_ok=N_of_45` (or N_of_40 on pre-387).
- Host test: replay a synthetic "all-correct" fingerprint matrix and an "all-broken" matrix and assert the aggregate emits correctly.
- **DGROUP cost cap:** 800 bytes. Use a single packed static table of expected tword results rather than per-case string literals.

**M1.2 `/CSV` output mode (research gap H).**
- Add `/CSV` to `opts_t` CLI parsing. When set, after INI emit, open `<out>.CSV` and write a machine-parseable CSV form of the same table.
- CSV schema: header row `key,value,confidence,method`, one row per emitted key, values quoted with `"` when they contain comma or quote; RFC 4180 minimal quoting rules.
- Integer values emit as decimal; string values emit verbatim except for quoting; confidence encodes as `HIGH|MEDIUM|LOW`; method field pulls from `*.method` companion keys where present, empty otherwise.
- No new CLI flag needed beyond `/CSV` (filename derived from `/O:` or default).
- Host test: golden-fixture comparison; 20-row synthetic table → expected CSV output bytewise-compared.
- **DGROUP cost cap:** 400 bytes. Reuse existing format-helpers; no new per-value format strings.

**M1.3 quality gate + commit.**

**M1 exit criteria:**
- Both host test suites green.
- Stock build succeeds with `wmake` (Whetstone+upload still gated out).
- Research build succeeds with `wmake WHETSTONE=1 UPLOAD=1`.
- DGROUP delta < 1.2 KB.
- CERBERUS.md §6 (FPU) and §7 (flags) updated.
- methodology.md extended with "IEEE-754 edge-case suite" and "CSV output format" sections.

### M2 — Cache depth expansion (research gaps A + B + E)

This is the substantive research-parity milestone. Requires a FAR-buffer framework we do not yet have for 64-256 KB working sets, plus real hardware with L2 to validate.

**M2.1 L1 pointer-chase latency probe (research gap A).**
- New `src/bench/bench_cache_lat.c` + `bench_cache_lat_a.asm`. Allocate a 2 KB buffer (fits in every target L1 with margin). Initialize as a randomized linked chain of 32-bit next-pointers. Measure N chase operations via PIT-C2.
- Emit `bench.cache.char.l1_ns` as nanoseconds/access. Confidence HIGH on RDTSC-capable parts (~1 ns resolution), MEDIUM on PIT-only (~838 ns resolution, so accurate only for ~20 ns and up with enough iterations).
- Integrated into `bench_cache_char.c` dispatcher so the single `bench.cache.char` section carries both throughput + latency keys.
- **DGROUP cost cap:** 200 bytes.

**M2.2 L2 cache detection via extended sweep (research gap B).**
- Extend `bench_cache_char` to sweep 64/128/256 KB working sets via FAR `halloc`-style buffers (not DGROUP). Plateau detector treats a second throughput drop after L1 as the L1→L2 boundary; the working-set size at the midpoint is `l2_size_kb`.
- FAR buffer allocation via `_fmalloc` (Watcom medium model). On failure (low conventional memory) the probe skips with `bench.cache.char.l2_status=no_far_mem` rather than failing the run.
- Emit `bench.cache.char.size_64kb_kbps` through `size_256kb_kbps` as individual rows; `bench.cache.char.l2_size_kb` as the inferred aggregate.
- Pre-486 CPUs skip (no on-chip L2 in era; L2 on 386 is always external SRAM, which shows as "memory" not "L2" in the sweep).
- **DGROUP cost cap:** 300 bytes; FAR buffer memory is not DGROUP.

**M2.3 DRAM latency derivation (research gap E).**
- From the largest successfully-measured sweep size, compute `bench.cache.char.dram_ns = 1_000_000 / (size_largest_kbps / line_size_kb)`. Emits alongside `l1_ns` so the DRAM/L1 ratio is inspectable.
- Confidence MEDIUM; derived, not directly measured.
- **DGROUP cost cap:** 0 bytes (derived, no new strings).

**M2.4 quality gate + real-iron validation on BEK-V409 + 386.**

**M2 exit criteria:**
- BEK-V409 capture emits `l1_ns`, `l2_size_kb`, `dram_ns` with plausible values (L1 ~15-30 ns, L2 on 486 with on-chip L1 only shows no L2 plateau; DRAM ~80-120 ns).
- 386 DX-40 capture emits `l1_ns` (no L1 on-chip on 386; the 2 KB buffer hits zero-wait external SRAM cache), `l2_size_kb` if external L2 present (expected "not probed" on the 386 bench box), `dram_ns`.
- DGROUP delta < 600 bytes.

### M3 — DB and detection gap closure (DB entries + narrow probe patches)

Three small items, all DB-and-detection-code work. No research gaps, no benchmark framework. Each ships independently.

**M3.1 IIT 3C87 FPU discrimination.**
- `hw_db/fpu.csv` needs an IIT 3C87 row distinct from Intel 80387. Distinguishing signature: IIT 3C87 has a custom extended instruction set (notably F4x4 matrix multiply opcodes D9 E8-E9 range) that Intel 80387 does not. Detection via the FPU behavioral fingerprint's existing infrastructure.
- New fingerprint axis: `fptan_behavior_on_denormal` or an IIT-specific probe instruction. Preferred: issue a known IIT-specific opcode and catch the INT 6 fault if not supported.
- Emit `fpu.vendor=IIT`, `fpu.model=3C87`, `fpu.friendly=IIT 3C87` on a 386 with IIT installed.
- Host test: synthesize the fingerprint bit pattern, assert DB lookup returns IIT row.
- **DGROUP cost cap:** 100 bytes (one new DB row).

**M3.2 Genoa ET4000 video chipset ID.**
- `hw_db/video.csv` has Tseng ET4000 entries. Genoa Systems' OEM board uses the ET4000 silicon but the BIOS does not surface the expected Tseng signature strings. New detection path: probe ET4000 extended CRTC registers directly (same approach as S3 Trio64 probe), query CR35/CR36 for the ET4000 signature bits independently of BIOS strings.
- Emit `video.vendor=Genoa`, `video.chipset=ET4000` when the OEM BIOS is Genoa but chip-level probe confirms ET4000.
- **DGROUP cost cap:** 100 bytes.

**M3.3 Hercules variant discrimination (HGC/HGC+/InColor).**
- `display.c` adapter-tier waterfall currently returns `ADAPTER_HERCULES` generic. Three variants distinguishable via 3BAh bits 6:4 (status-register extension per HGC+ spec): HGC=000, HGC+=001, InColor=010.
- New `adapter_t` enum values: `ADAPTER_HERCULES_HGC`, `ADAPTER_HERCULES_HGCPLUS`, `ADAPTER_HERCULES_INCOLOR`.
- Downstream code that gates on `ADAPTER_HERCULES` tests `tier >= ADAPTER_HERCULES_HGC && tier <= ADAPTER_HERCULES_INCOLOR` to preserve generic behavior.
- Emit `video.adapter=hercules_hgc|hercules_hgcplus|hercules_incolor`.
- **DGROUP cost cap:** 150 bytes.

**M3.4 quality gate + commit.**

**M3 exit criteria:**
- 386 DX-40 bench-box capture emits `fpu.vendor=IIT` when IIT 3C87 is socketed, `video.vendor=Genoa` when Genoa ET4000 is installed.
- Hercules detection path exercised in host tests (real-iron Hercules validation deferred to when hardware is available).
- DGROUP delta < 400 bytes.

### M4 — BEK-V409 BSS overwrite root-cause (hardware-gated)

This is the one item that cannot be code-only. Needs Tony on the BEK-V409 with the removal-at-a-time protocol documented in `docs/methodology.md` "End-of-run crumb chain (EMM386 #05 mitigation)" applied analogously to the NULL-write bug.

**M4.1 Deploy instrumented build to BEK-V409 with probe-path removal crumbs.**
- Scratch branch from `main` with added crumb pairs isolating each of the three suspected paths (S3 Trio64 probe, Vibra 16S/OPL fallback, UMC491 PIT).
- Tony FTPs the binary, runs, cold-reboots, runs again, compares captured CERBERUS.LAS trail to identify which path's absence eliminates the canary.

**M4.2 Narrow to single faulting instruction.**
- Once the offending probe is identified, single-step through it with DOS debugger equivalent (`DEBUG.EXE`), find the instruction that lands near DGROUP:0.

**M4.3 Explicit fix + re-validate.**
- Pointer-type error, buffer-bound error, segment-prefix error, whatever it turns out to be.
- Restore probe path in main line; verify the canary does not fire across 10 consecutive BEK-V409 runs.

**M4.4 quality gate + commit.**

**M4 exit criteria:**
- 10 consecutive BEK-V409 runs with no `*** NULL assignment detected` canary at exit.
- 386 DX-40 still clean (no regression on the tier that was already clean).
- DOSBox-X smoketest still clean.

---

## 4. DGROUP Budget Gate

Current v0.8.0 headroom: 2,512 bytes vs 62 KB soft target, 4,560 bytes vs 64 KB hard ceiling.

**Per-milestone cost caps (sum = 2,550 bytes worst case, 1,200 bytes realistic):**
- M1: 1,200 bytes (800 IEEE + 400 CSV).
- M2: 500 bytes (200 L1 latency + 300 L2 labels; FAR buffers are not DGROUP).
- M3: 350 bytes (100 + 100 + 150).
- M4: 0 bytes (bug fix, no new strings).

Worst case total: 2,050 bytes. Fits with margin. `tools/dgroup_check.py` runs before each milestone commit.

If any milestone blows its cap, reclaim via:
1. Audit `CONST` segment for duplicate string literals (cheap wins historically).
2. Move any large static read-only tables to `__far const` explicitly.
3. Shorten new narration strings without losing meaning.

---

## 5. Real-Hardware Validation Matrix

Per 0.8.0 tag doctrine, every shipped feature gets a real-iron capture before 0.8.1 tag.

| Milestone | Minimum hardware | Covers |
|---|---|---|
| M1 IEEE edges | 486 DX-2-66 (BEK-V409) + 386 DX-40 + IIT 3C87 | FPU with denormals/SNaN on both tiers |
| M1 CSV | Any | Pure output, hardware-agnostic |
| M2 L1 latency | 486 DX-2-66 + 386 DX-40 | Both have L1 or external cache behavior |
| M2 L2 detection | 486 DX-2-66 (no L2) + Pentium 133 or 200 MMX (has L2) if available | L2 presence/absence contrast |
| M2 DRAM derivation | Both above | Derived from L2 sweep |
| M3.1 IIT 3C87 | 386 DX-40 + IIT 3C87 (on-hand) | Positive case |
| M3.2 Genoa ET4000 | 386 DX-40 with Genoa ET4000 (on-hand) | Positive case |
| M3.3 Hercules | Deferred pending Hercules card availability | Host-tested only |
| M4 BSS overwrite | BEK-V409 specifically | Hardware-specific defect |

---

## 6. Ordered Execution

1. Plan approval + task creation.
2. **M1.1 IEEE-754 edges** — code + host test.
3. **M1.2 `/CSV` flag** — code + host test.
4. **M1.3 quality gate** + commit.
5. **M3.1 IIT 3C87** — DB + probe + host test.
6. **M3.2 Genoa ET4000** — DB + probe.
7. **M3.3 Hercules variants** — enum expansion + probe.
8. **M3.4 quality gate** + commit.
9. **M2.1 L1 latency probe** — asm + C + host test.
10. **M2.2 L2 detection sweep** — FAR buffer framework + host test.
11. **M2.3 DRAM derivation** — 1-line addition.
12. **M2.4 quality gate** + real-iron capture + commit.
13. **M4** — requires dedicated BEK-V409 session with Tony; scheduled outside this plan's autonomous execution window.
14. Tag `v0.8.1` after M1 + M2 + M3 close and real-iron captures land.

---

## 7. What 0.8.1 Explicitly Does Not Ship

- **Per-instruction FPU microbenchmarks** (research gap N). Whetstone replacement story. Stays 0.9.0.
- **Upload path revival**. Stays 0.9.0 pending server go-live + stack-safe offline fallback.
- **Disk throughput benchmarks**. Stays 0.9.0+.
- **Full CUA shell (menus, dropdowns, modals)**. Stays 0.9.0.
- **8088/XT real-hardware capture**. README claim retraction holds until Tony has a live 8088 box.

The 0.8.1 envelope is narrowly: complete the 0.8.0 deferred items. Not expand scope.
