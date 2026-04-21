# CERBERUS Catch-Up Brief

**Purpose:** single point-in-time reference that lets a fresh
Claude Code session orient quickly: what shipped, what is open,
what is on the bench box, what to do next.
**Last updated:** 2026-04-20 (Monday evening)
**Latest tag:** `v0.4.0` at `ded126a`
**Latest `main`:** `b7ba3f4`, seven commits past the tag
**Status:** working tree clean, origin up to date

## TL;DR

CERBERUS is a DOS-native hardware detection, diagnostic, and
benchmark tool for 8088-through-486 machines. Over a 48-hour
arc (2026-04-18 afternoon through 2026-04-19 evening) it
shipped four release candidates plus a final `v0.4.0` tag,
with real-hardware validation on BEK-V409 (486 DX-2-66,
S3 Trio64, AMI BIOS). A post-v0.4.0 dev block landed an ANSI
boot splash with OPL2 stinger, closed two GitHub issues (#1,
#3), instrumented a third (#2), built diagnostics for a fourth
(#6), and shipped a Phase 3 Homage research pass with seven
lesson docs and three attribution corrections. Four open
issues remain, two of which are blocked on pending real-iron
data captures.

## Timeline of releases and major commits

| Date | Artifact | Commit / Tag | Highlight |
|---|---|---|---|
| 2026-04-18 PM | `v0.2-rc1` RC | `311c19e` | Five bench-box bugs fixed on first real iron |
| 2026-04-19 AM | v0.3 host-tested | `7a28850` | diag_cache + diag_dma landed autonomously |
| 2026-04-19 noon | `v0.3-rc1` RC | `dee7360` | 6/6 diagnostics complete, issue #5 closed |
| 2026-04-19 PM | `v0.4-rc1` RC | `ab67703` | bench_cache, bench_video, rules 4b and 11 |
| 2026-04-19 eve | `v0.4.0` final | `ded126a` | 3 UI fixes; `REPO/releases/tag/v0.4.0` live |
| 2026-04-19 eve | ANSI intro | `593b139` | Three-headed-dog splash with OPL2 stinger |
| 2026-04-19 late | Issue sweep | `036cc1c`, `2f5b26e`, `74bc439` | #1 closed, #3 closed, #6 tool built, #2 instrumented |
| 2026-04-20 00:30 | Homage Phase 3 | `37777f3` | 7 new research docs, 3 attribution corrections |
| 2026-04-20 01:00 | Catch-up docs | `b7ba3f4` | CHANGELOG + session report |

## Current repository state

- **`main` HEAD** is `b7ba3f4` on branch `main`, pushed to
  `origin/main`. No local-only commits.
- **Tags on origin:** `v0.1.1-scaffold`, `v0.2-rc1`,
  `v0.3-rc1`, `v0.4-rc1`, `v0.4.0`. No prerelease tag exists
  yet for the post-v0.4.0 dev work.
- **Public release:** `v0.4.0` at
  `github.com/tonyuatkins-afk/CERBERUS/releases/tag/v0.4.0`
  with `CERBERUS.EXE` (144,166 B) and a BEK-V409 validation
  INI and four photographs attached.
- **Envelope (tip-of-tree):** EXE is 150,556 B, DGROUP is
  53,808 / 56,000 (4% working-ceiling headroom, 18% under the
  65,536 hardware ceiling), 167 host-test assertions green.

## Binaries on the BEK-V409 bench box (`C:\CERBERUS\`)

| Binary | Size | Purpose | Status |
|---|---|---|---|
| `CERBERUS.EXE` | 144,166 B | Production v0.4.0 tag | Validated |
| `CERB-VOX.EXE` | 144,054 B | Issue #6 test-1: bench_video at -ox | Results captured, posted to issue #6 |
| `CERB-INT.EXE` | 150,042 B | ANSI intro dev build | Validated, operator reaction "Wow that looked epic" |
| `CERB-DBG.EXE` | 150,556 B | Intro + OPL2 probe instrumentation | Deployed, awaiting multi-cold-boot data |
| `REPSTOSD.EXE` | 11,258 B | Issue #6 test-2: mode 13h VRAM ceiling | Deployed, awaiting run output |

Production CERBERUS.EXE is bytewise identical to the v0.4.0
release on GitHub. Dev binaries coexist with it; different
filenames so the production binary is never overwritten.

## Open GitHub issues

| # | Title | Current state | Next action |
|---|---|---|---|
| 1 | `test_timing` 4 pre-existing failures | **Closed** on 2026-04-19 (commit `036cc1c`). Test inputs updated for the 25% pit/bios divergence guard that landed later. Added dedicated divergence-guard coverage. | Done |
| 2 | OPL detection intermittency on Vibra 16 PnP | Instrumented with new `audio.opl_probe_trace` INI key (commit `74bc439`). `CERB-DBG.EXE` deployed on BEK-V409. | Multi-cold-boot capture: paste `audio.opl=` and `audio.opl_probe_trace=` from two or three boots covering both opl3-detected and none-detected outcomes. |
| 3 | UI hang on real iron | **Closed** on 2026-04-19 as cannot-reproduce. 9 consecutive clean real-iron runs. Instrumentation stash preserved at `docs/plans/attic/`. | Done |
| 4 | Whetstone FPU-asm rework | **Reframed** post Phase 3 T3. CheckIt's 11,420 is a custom-synthetic number, not a Curnow target. Real target: published Curnow Whetstone references in the 1,500-3,000 K-Whet range on 486 DX-2-66. | Rewrite issue body + plan around Curnow references. Then NASM x87 inner kernels for `PA`, `P0`, Module 2 hot loop as a multi-session arc. |
| 5 | diag_cache threshold miscalibration | **Closed** 2026-04-19 (commit `25306b4`). Per-line ratio reformulation validated on BEK-V409. | Done |
| 6 | bench_video reports ISA-range on VLB | Test-1 (CERB-VOX) showed CFLAGS_NOOPT tax is only +7-9%, not dominant. Test-2 tool (`REPSTOSD.EXE`) deployed to isolate C-loop overhead vs hardware path. | Run `REPSTOSD.EXE` on BEK-V409 and paste output. If 20+ MB/s the bottleneck is bench_video's C-loop. If ~5 MB/s the bottleneck is the hardware path (BIOS shadow, VLB slot, jumpers). |

## Key research: Homage Phase 2 and Phase 3

Lesson docs at [`docs/research/homage/`](../research/homage/).
Phase 2 and Phase 3 together cover 14 lesson docs, all under
the ethical frame "no decompiled code reproduced, no binary
redistribution, attribution preserved, corrections flagged
openly."

**Load-bearing findings that changed CERBERUS planning:**

- **CheckIt's "Dhrystones" is a custom synthetic** (Phase 2
  T2). Zero canonical Weicker markers. The `bench_dhrystone.c`
  comment now frames the BEK-V409 33,609 anchor as empirical,
  not algorithmic.
- **CheckIt's "Whetstones" is also a custom synthetic**
  (Phase 3 T3). Zero canonical Curnow-Wichmann markers.
  Reframes issue #4's 100x-low framing.
- **CheckIt does not measure mode 13h video at all** (Phase 3
  T5). Text-mode-only benchmark. No CheckIt reference exists
  for CERBERUS's `bench.video.mode13h_kbps`.
- **3DBENCH reports a per-frame phase breakdown** with a
  dedicated `Clr` VRAM-clear column (Phase 3 T15). Sharpest
  issue-#6 comparator in the corpus.
- **CACHECHK's UMC timer workaround** is structurally the
  same as CERBERUS's `timing_compute_dual` 25% divergence
  check (Phase 3 T8). Useful technique: the
  "Timer messed up! %08lx %08lx %08lx" forensic-value-emit
  pattern. Filed as v0.5+ Rule 4a enhancement.

**Attribution corrections shipped across Phase 2 and 3:**

| Tool | Phase 1/Phase 2 planning said | Reality |
|---|---|---|
| FASTVID | Bill Yerazunis VLB TSR | Pentium Pro 82450 MTRR tool (John Hinkley, 1996) |
| SPEEDSYS | Roedy Green | Vladimir Afanasiev (Russia) |
| PCPBENCH | Jim "Trixter" Leonard | PC Player magazine / Computec Media (Germany) |
| 3DBENCH | Future Crew / Paralax | Superscape VRT Ltd (UK) |

Four attribution corrections across two research phases is a
high rate. Future Homage-family research should embed an
explicit "verify stated author against binary attribution"
step before tool work begins.

## Session reports index

All in [`docs/sessions/`](./):

- `SESSION_REPORT_2026-04-18-evening.md` Initial v0.2 real-iron
  gate. Five bugs fixed on contact.
- `SESSION_REPORT_2026-04-18-to-19-weekend.md` The three-RC
  weekend arc in narrative form, including commit breakdowns
  and the issue-filing sequence.
- `SESSION_REPORT_2026-04-19-evening-to-2026-04-20.md` The
  post-v0.4.0 dev work: ANSI intro, issue sweep, Phase 3
  research.

## Next-session candidates, prioritized

**Tier 1: convert existing groundwork into closed issues** (small effort, high payoff)

1. **Run `REPSTOSD.EXE` on BEK-V409.** ~15 seconds. Paste KB/s
   output. Closes issue #6 test-2 decision (hardware vs
   methodology).
2. **Run `CERB-DBG.EXE` across 2-3 cold boots on BEK-V409.**
   Paste `audio.opl=` and `audio.opl_probe_trace=` from each.
   Enables issue #2 bisection.
3. **Tag `v0.5.0-alpha-intro`** at `main` HEAD with
   `CERB-DBG.EXE` attached. Makes the ANSI intro downloadable
   from GitHub Releases.
4. **Rewrite issue #4 body** around Curnow Whetstone references
   instead of CheckIt's synthetic number (Phase 3 T3 informed
   the reframe).

**Tier 2: new arcs** (bigger effort, longer payoff)

5. **Issue #4 NASM x87 Whetstone rework.** 6 to 10 hours across
   two or three sessions. Write NASM kernels for `PA`, `P0`,
   Module 2 hot loop. Watcom/NASM interop. Target: a genuine
   Curnow Whetstone result in the 1,500 to 3,000 K-Whet band
   on 486 DX-2-66.
6. **386 DX-40 next-platform validation.** Per
   [`NEXT-PLATFORMS.md`](NEXT-PLATFORMS.md). Fresh real-iron
   bugs likely. Requires physical hardware access.
7. **Rule 4a forensic-value-emit enhancement** from Phase 3 T8
   finding. Augment the current `measurement_failed` emit with
   raw pit/bios hex values. Small patch, same shape as
   `audio.opl_probe_trace`.

**Tier 3: research and scope-expansion candidates** (from
Phase 3 T3-T17 lesson docs)

8. **Cyrix DIR-based pre-CPUID discrimination.** Top v0.5+
   candidate per `chkcpu-lessons.md`. Closes the generic
   `486-no-cpuid` tag gap for Cyrix 486DLC and family.
9. **Cache WB/WT mode detection.** Per `chkcpu-lessons.md`.
   CR0.CD/NW plus Cyrix CCR2 reads.
10. **Clock multiplier decomposition.** Per
    `chkcpu-lessons.md`. Informative, not blocking.
11. **Phase 4 Homage research.** MTRRLFBE (UPX-packed
    FASTVID sibling), Norton SysInfo 8 (Symantec 1993, classic
    distinguished utility), DOOMS/QUAKES TIMEDEMO methodology
    (from now-open-source id engines).

## Important memory / feedback rules

(Saved in `C:\Users\tonyu\.claude\projects\C--Development\memory\`;
all will be applied automatically. Listed here for
explicit orientation.)

- **No em-dashes or sentence hyphens** including `--` in any
  prose (commits, docs, READMEs, release notes, site copy,
  chat). Use commas, colons, semicolons, periods instead.
  Compound-word hyphens inside single words are fine.
- **Keep site and README current** after new work ships.
  barelybooting.com plus repo README get updated proactively
  on major deliverables.
- **486 FTP workflow** at `10.69.69.160`, user `tony`,
  password `netisa`. Targets `/drive_c/CERBERUS/`, never root.
  Uses `binary` mode for EXE, `ascii` for text.
- **FTPSRV no-overwrite policy** on the 486: `delete FILENAME`
  before each `put` to replace an existing file.
- **DOS development conventions** live in
  `feedback_dos_conventions.md` plus the other DOS feedback
  files. Key patterns: DGROUP ceiling discipline, unsigned
  counters, BIOS-tick wrap handling, Mode 13h conventions.
- **Quality-gate iteration pattern:** round 2 finds the real
  defects, rounds 3+ find polish. Capped at 3 rounds since
  bench_video; results validated on BEK-V409.

## Methodological patterns captured

- **Label collision.** When two benchmarks report numbers in
  the same format with the same label but very different
  magnitudes, verify the algorithms are the same before
  treating the gap as a port defect. First seen Phase 2 T2
  (CheckIt Dhrystone); reinforced Phase 3 T3 (CheckIt
  Whetstone).
- **Forensic-value emit.** When a sanity check fails, emit
  the raw values that failed the check alongside the status
  string. CACHECHK's "Timer messed up! %08lx %08lx %08lx" is
  the exemplar; applied in this session's issue #2 OPL probe
  trace.
- **Attribution verification.** Four research passes produced
  four author corrections. Embed an explicit "confirm
  attribution from binary strings" step in any future
  Homage-family research.

## How to resume

1. Read this file first.
2. Check `git log --oneline -10` in CERBERUS repo for any
   further progress past `b7ba3f4`.
3. Read the most recent session report in
   `docs/sessions/` for narrative context.
4. Read the most recent Phase 3 lesson in
   `docs/research/homage/` for research context.
5. Ask operator (Tony) which of the Next-Session-Candidates
   tiers to engage, or if the priorities have shifted.
