# Session Report, 2026-04-19 Evening through 2026-04-20

**Operator:** Tony Atkins
**Window:** Sunday evening (post-v0.4.0 tag) through dinner
with Rick Dueringer and back. Primary CERBERUS bench box
BEK-V409 remained live and receiving binaries throughout.
**Arc shape:** post-release hotfix follow-up, cinematic
polish, targeted issue closure, and a Phase 3 Homage
research pass. Five commits past the `v0.4.0` tag; no new
release tag cut.

---

## What shipped

### Post-v0.4.0 commits on `main`

| Commit | Topic | Impact |
|---|---|---|
| `593b139` | ANSI three-head boot intro with OPL2 stinger | +3 KB source, +320 B DGROUP, +5 KB EXE |
| `036cc1c` | Issue #1 test_timing 4 failures fixed | 167 / 167 host assertions green |
| `2f5b26e` | Issue #6 test-2 REPSTOSD.EXE reference | standalone 11 KB diagnostic |
| `74bc439` | Issue #2 OPL probe instrumentation | new `audio.opl_probe_trace` INI key |
| `37777f3` | Homage Phase 3, seven lesson docs | 7 new files in `docs/research/homage/` |

`main` HEAD at `37777f3`. All pushed to origin.

### ANSI boot intro (`593b139`)

Adapter-aware three-headed-dog emblem rendered between
`display_init` and `display_banner`. Iconography sourced
from Pseudo-Apollodorus (three dog heads, serpent tail,
snake-mane spines), Virgil (three throats with fangs), Dante
(red glowing eyes, chained beast), and Hercules 12th-labor
Roman iconography (bound at the gate of Hades).

Adapter ladder is full: VGA color / MCGA get DAC palette
fade-in and fade-out; EGA / CGA color get attribute-only
animation; MDA / Hercules / VGA-mono / EGA-mono get
intensity-bit pulse with block-shading silhouette.

Audio is OPL2 on channels 0 through 3 plus channel 7 for
rhythm-mode snare. Three escalating barks at D2, F2, A2 as
each head's eyes illuminate; sustained A-minor triad with
vibrato through the hold phase; sub-bass drone on channel 3
under everything; rhythm-mode snare hit at the chain-shatter
climax. Writes are unconditional so silent boxes (no OPL2)
simply have the port writes absorbed with no audible effect;
state is cleaned up before `detect_audio` runs.

Visual embellishments on top of the iconographic skeleton:
hellfire ember row flickering at row 22, chain rattle around
the broken link, breath sparks rising above each mouth,
serpent tail wiggle between two poses, wordmark color cycle,
BEHOLD flash at mid-sustain, DAC white-flash at the chain-
shatter moment.

The full sequence runs ~2.2 seconds. Keypress dismisses
immediately and cleans up DAC plus OPL state. Validated on
BEK-V409; operator reaction recorded as "Wow that looked
epic." No detect / diag / bench regressions; numbers
consistent with v0.4.0 baseline to run-variance only.

### Issue-fix sweep

- **Issue #1 closed.** The four pre-existing `test_timing`
  assertion failures were written before the 25% pit/bios
  divergence guard landed in `timing_compute_dual`. Their
  c2 values produce highly-divergent pit vs bios readings
  that correctly trip the new guard. Updated inputs to
  near-full-wrap sub_ticks (satisfies all three kernel
  gates) while preserving the original test semantic
  (target=1 with 0 wraps does not trip the lower-bail).
  Added dedicated divergence-guard coverage (both pit>>bios
  and bios>>pit branches). 167 assertions green.

- **Issue #3 closed.** UI hang did not reproduce across 9
  consecutive clean runs. Closed as cannot-reproduce with
  reopen criteria preserved. Instrumentation stash stays
  at `docs/plans/attic/`.

- **Issue #6 test-1 earlier today.** `CERB-VOX.EXE` at
  144,054 bytes showed CFLAGS_NOOPT tax is only +7 to +9%.
  Not the dominant factor. Posted data point to issue #6.

- **Issue #6 test-2 tool built.** Standalone
  `tools/repstosd/REPSTOSD.EXE` at 11,258 bytes. Pure-
  assembly REP STOSW inner loop writing 128 MB to mode 13h
  VRAM. Isolates C-loop overhead versus hardware-path
  limitation. Deployed to BEK-V409 at `C:\CERBERUS\`.

- **Issue #2 instrumentation.** New
  `audio.opl_probe_trace` INI key emits byte-level trace of
  every status-register read across the primary and fallback
  OPL probe attempts. Deployed as `CERB-DBG.EXE` at 150,556
  bytes. Awaiting multi-cold-boot trace captures to identify
  the differing byte pattern.

- **Issues still open past this session.** #1 gated behind
  UMC491 deep-dive closed as a test-infrastructure fix; the
  runtime UMC491 behavior is unchanged. #2 instrumented;
  needs multi-boot capture. #4 reframed via Phase 3 T3
  (see below). #6 two tests built and deployed; needs
  capture output.

### Homage Phase 3 research (`37777f3`)

Seven lesson docs in `docs/research/homage/`. Phase 2
ethical frame carried forward: no decompiled code
reproduced, no binary redistribution, attribution
preserved, corrections flagged openly.

**Deferred-from-Phase-2 tasks closed:**

- **T3 CheckIt Whetstone.** Not a Curnow-Wichmann port.
  Zero canonical Whetstone markers in the binary or overlay
  (Module 1..11, Curnow, Wichmann, PA, P0, canonical
  floating-point constants, MFLOPS all absent). Custom FPU
  synthetic with the familiar label. **Reframes issue #4:**
  the 100x gap between CERBERUS's 109 K-Whet and CheckIt's
  11,420 is a label collision, not a port defect. New
  target: published Curnow Whetstone references (1,500 to
  3,000 K-Whet on 486 DX-2-66).
- **T5 CheckIt video methodology.** Only text-mode video
  measurements (BIOS Video CPS and Direct Video CPS). No
  mode 13h path. No CheckIt reference number exists for
  CERBERUS's `bench.video.mode13h_kbps`. Issue #6 second-
  opinion data must come from tools that actually measure
  mode 13h (PCPBENCH, 3DBENCH, CHRISB, REPSTOSD).
- **T8 CACHECHK UMC timer workaround.** Structural match
  to CERBERUS's `timing_compute_dual` 25% divergence check.
  No technique delta. Useful borrow: the "Timer messed up!
  %08lx %08lx %08lx" emit pattern, publishing raw forensic
  values on failure. Filed as v0.5+ Rule 4a enhancement.
- **T9+T10 SPEEDSYS.** Vladimir Afanasiev (Russia), not
  Roedy Green as Phase 1 notes had it. Calibrated-reference
  methodology anchored on Pentium MMX 200 (later abandoned
  in v4.76 for absolute Peak Bandwidth). Feature matrix
  overlaps CERBERUS only at pre-CPUID CPU detection; CHKCPU
  (Phase 2 T12) already covers that range.

**New research for issue #6 cross-corroboration:**

- **T14 PCPBENCH.** PC Player magazine (Computec Media,
  Germany), not Jim Leonard. DOS/4GW 32-bit 3D with 16x
  REP STOSD and 11x REP MOVSD.
- **T15 3DBENCH.** Superscape VRT Ltd (UK), not Future Crew
  / Paralax. **Critical finding:** reports a per-frame
  phase breakdown (`Mov Prc Srt Clr Drw Cpy Tot Fps`) with
  `Clr` as a dedicated VRAM-clear time column. Sharpest
  issue-#6 comparator in the corpus.
- **T16 CHRISB.** "Chris's 3d Benchmark" DJGPP 1996. SVGA
  variant has explicit S3 path relevant to BEK-V409 Trio64.
- **T17 LM60.** Landmark Speed 6.0. Same IBM PC/XT anchor
  as CheckIt; era convention, not tool-specific.

**Attribution corrections this pass:** SPEEDSYS
(Green → Afanasiev), PCPBENCH (Leonard → PC Player
magazine), 3DBENCH (Future Crew → Superscape VRT Ltd).

---

## Current state on the BEK-V409

Five CERBERUS-family binaries coexist at `C:\CERBERUS\`:

| Binary | Size | Purpose |
|---|---|---|
| `CERBERUS.EXE` | 144,166 B | Production v0.4.0 tag |
| `CERB-VOX.EXE` | 144,054 B | Issue #6 test-1 (bench_video at -ox) |
| `CERB-INT.EXE` | 150,042 B | ANSI intro dev build |
| `CERB-DBG.EXE` | 150,556 B | Intro + OPL2 probe instrumentation |
| `REPSTOSD.EXE` | 11,258 B | Issue #6 test-2 hardware-ceiling probe |

All FTP-deployed during the session. Production `CERBERUS.EXE`
is untouched from the v0.4.0 tag; dev binaries stay available
for targeted data collection.

## GitHub state

- `main` at `37777f3`
- Tags: unchanged from earlier session
  (`v0.4.0`, `v0.4-rc1`, `v0.3-rc1`, `v0.2-rc1`,
  `v0.1.1-scaffold`)
- Releases page: v0.4.0 as before (with EXE, INI, 4
  photographs)
- Issues: #3 closed, #5 closed (earlier), #1 / #2 / #4 / #6
  open with documented next steps; no new issues filed this
  session

## Open questions for next session

1. **Run `REPSTOSD.EXE` on BEK-V409** and paste the KB/s
   result. This closes the branch on issue #6 test-2.
2. **Run `CERB-DBG.EXE` across several cold boots** and
   paste the `audio.opl=` plus `audio.opl_probe_trace=`
   lines from each. Two or three boots covering both the
   "opl3 detected" and "none detected" outcomes lets us
   diff the traces and identify the differing byte pattern.
3. **Optionally run `PCPBENCH`, `3DBENCH`, `CHRISB` on
   BEK-V409** as the Phase 3-informed cross-corroboration
   plan for issue #6. Three more data points to triangulate
   the mode 13h bandwidth question.
4. **Issue #4 Whetstone** reframing. With the Phase 3 T3
   finding in hand, the issue's "aim for 11,420" target is
   wrong; the real target is a published Curnow Whetstone
   number on matched hardware. Rewriting the issue body and
   plan is a quick next-session starter.
5. **386 DX-40 next-platform validation** per the long-
   standing `NEXT-PLATFORMS.md` plan. The session before
   this one had Tony preparing the 486 for 386 transition;
   still pending.
6. **v0.5.0-alpha tag decision.** Current `main` past
   v0.4.0 has shippable intro + test fix + two instrumented
   dev paths + Phase 3 research. Worth a prerelease tag?
   If yes, next-session opens with `v0.5.0-alpha-intro` or
   similar, with `CERB-DBG.EXE` attached.

## Methodological notes recorded this session

- **The "label collision" pattern,** first established in
  Phase 2 T2 (CheckIt Dhrystone) and reinforced in Phase 3
  T3 (CheckIt Whetstone). When two benchmarks report
  numbers in the same format with the same label but
  significantly different magnitudes, check first whether
  the algorithms are the same. A 100x divergence is not
  automatically a port defect.
- **The "forensic-value-emit" pattern,** from CACHECHK's
  "Timer messed up" path. When a sanity check fails, emit
  the raw values that failed the check alongside the
  status, so post-hoc diagnosis has material to work with.
  This session applied the pattern in issue #2 (OPL probe
  trace); filed as v0.5+ enhancement for Rule 4a.
- **Three attribution corrections in one pass** is a high
  rate. Planning notes benefit from preserved-provenance
  checks before tool work starts. Phase 4 planning, if it
  happens, should embed an explicit "verify stated author
  against binary attribution" step.

## Closing

Five commits past the `v0.4.0` tag with no regression to
v0.4.0 production behavior (production CERBERUS.EXE
unchanged on BEK-V409). ANSI intro is the headline deliverable;
issue sweep is the invisible work; Phase 3 Homage research is
the groundwork for the next cycle. Repo and site ready for
the next session's real-iron data-collection pass.
