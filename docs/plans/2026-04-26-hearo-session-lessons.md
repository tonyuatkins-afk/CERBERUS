# HEARO session applicable lessons — CERBERUS 0.9.0 addendum

Drafted 2026-04-26 after the HEARO session in NetISA produced sixteen
commits worth of audio-detection, exit-cleanup, and test-infrastructure
work. Items below either fold into existing 0.9.0 milestones or stand
alone as new defensive work. This is an addendum to
`docs/CERBERUS_0.9.0_PLAN.md`, not a replacement; existing M1–M7 stand.

The HEARO commits referenced are on `origin/main` of the NetISA repo,
range `156695c..HEAD`. File paths in HEARO are absolute under
`C:\Development\NetISA\hearo\`. CERBERUS reuses the patterns by writing
fresh implementations in CERBERUS's MIT-licensed source. No code is
copied across repos.

## Direct fits with existing M-numbered milestones

### M2 (Hardware identification expansion) — audio family enrichment

Three HEARO patterns lift cleanly into M2's audio-detection scope. Each
is one milestone task, smoketestable in DOSBox-X for the genuine-Creative
case and in 86Box `hearo-ymf715` / `hearo-ess` for the clone cases.

**M2-H1: OPL3-SAx variant identification.**
HEARO `src/audio/wake_opl3sa3.c:92-128` (probe half only; CERBERUS does
not write chip state). Port-scan five known control bases
(0x370, 0x100, 0x538, 0xE80, 0xF86), validate via MISC at index 0x0A
(read, XOR low 3 bits, confirm readback, restore) plus MIC at index
0x09 (write 0x8A, confirm `(read & 0x9F) == 0x8A`, leave at 0x9F).
Variant code from MISC low 3 bits per the May 1997 YMF715E datasheet:
1=YMF711/SA2, 2=YMF715/SA3, 3=YMF715B, 4=YM719/SA4, 5=Toshiba Libretto
SA3, 7=NeoMagic. Reports as e.g. "OPL3-SA3 YMF715B at control base
0x370" instead of just "Sound Blaster Pro 2 compatible".

DGROUP cost: handful of bytes for the variant string table, same shape
as existing CSV-driven probes. Implementation note: the probe must
stay non-destructive (CERBERUS reads, does not wake), so omit the
`wake()` half of HEARO's backend entirely.

**M2-H2: DSP 0xE3 copyright probe.**
HEARO `src/detect/audio.c:267-345`. Issue DSP cmd 0xE3, read NUL-terminated
copyright string with bounded per-byte timeout (1 ms per byte budget),
search case-insensitive for "CREATIVE". Differentiates Creative-genuine
silicon from clones in the boot-screen output without changing detection
logic ("Sound Blaster 16 (Creative-genuine, DSP 4.13)" vs "Sound
Blaster 16 (clone, no copyright string, DSP 4.13)").

Reset the chip on probe timeout to avoid leaving the DSP state machine
expecting more bytes (HEARO's pattern). Skip the probe for DSP < 2.0
(predates the 0xE3 command).

**M2-H3: SB16 mixer 0x81 DMA channel cross-check.**
HEARO `src/detect/audio.c:271-322`. Read SB16 mixer register 0x81 at
detect time, decode the bit field (bits 0/1/3 = 8-bit DMA channels 0/1/3,
bits 5/6/7 = 16-bit DMA channels 5/6/7). Cross-check against BLASTER
env. CERBERUS already accepts BLASTER as input; adding the chip-side
read lets the report flag mismatch ("BLASTER says D3 H7; chip reports
D1 H5 — BLASTER may be misconfigured") and surface the Vibra 16XV H==D
quirk explicitly.

Smoketest: 86Box `hearo-sb16` (BLASTER matches mixer), `hearo-ymf715`
(YMF715 may report differently — that's a finding worth capturing on
its own).

### M3 (Network stack + OUI lookup) — PCI BIOS scan reference

HEARO's session research included a deep walk of Mpxplay's
`AU_CARDS/PCIBIOS.C` pattern: per-driver vendor/device ID table,
`pcibios_search_devices()` iterating the table via INT 1Ah PCI BIOS
calls. The pattern maps cleanly to NIC detection in M3 if any NIC
family (3Com 3C90x PCI) joins the existing ISA-PnP scope.

Documented in `C:\Development\NetISA\hearo\docs\research\hearo-mpxplay-comparison.md`
section D. Read for pattern, do not copy code (Mpxplay is GPL,
CERBERUS is MIT). Same prose-not-code discipline HEARO followed.

This is reference material, not a new task. If M3 stays ISA-only it
does not apply.

### M5 (Upload revival + full CUA shell) — cleanup paths

HEARO Phase 4 work directly applies to CERBERUS exit hygiene
regardless of upload-pipeline status.

**M5-H1: DOS MCB chain validation on exit.**
HEARO `src/platform/dos.c:dos_mcb_validate`. Walks MCB chain via INT
21h AH=52h head pointer at ES:[BX-2]. Six independent corruption checks,
0x4000 walk budget, head-segment range guard. Surfaces a "WARNING: DOS
MCB chain corrupt at exit" diagnostic before COMMAND.COM panics with
"Memory allocation error". Cheap insurance for any DOS app; CERBERUS
allocates and frees as much as HEARO during long probe sequences.

Self-contained ~50 lines. Drop into `src/platform/` (or wherever
CERBERUS keeps DOS-service wrappers) and call from main's exit path.

**M5-H2: atexit + INT 23h/1Bh + INT 24h handlers.**
HEARO `src/hearo.c:38-80`. Idempotent `run_shutdown` helper, registered
via `atexit()`, called from `signal(SIGINT, ...)` / `signal(SIGBREAK, ...)`,
and `_harderr(handler)` returning `_HARDERR_FAIL` to suppress DOS's
"Abort, Retry, Fail" prompt on critical errors.

Particularly relevant for CERBERUS because diagnostics often probe
not-ready devices (floppy with no media, COM port with nothing
attached). Without `_harderr`, DOS draws the prompt over CERBERUS's
TUI and the user has to type Fail on a screen they cannot read.

The pattern is ~60 lines. Watcom-specific: `_DOSFAR` macro is
`#undef`-ed at end of `<dos.h>`; use `__far` directly in handler
parameter declarations. SIGBREAK is Watcom-specific (separate from
SIGINT).

## Cross-cutting items not tied to a specific M-number

### M0-H1: Bounded DSP write timeout

HEARO `src/audio/sb.c:dsp_write_to`. Audit any place CERBERUS does
`outp(base+0xC, val); inp(base+0xE)` style DSP probing. Without a
bounded busy-bit-clear timeout, a wedged YMF715 (or any clone with a
stuck busy bit, including some ESS variants) hangs CERBERUS at the
audio-detect step and the user has to power-cycle.

The pattern: spin on `inp(base+0xC) & 0x80` for at most N iterations
(HEARO uses 0x4000 for normal writes, 256 for ISR-context writes),
then return failure if it never cleared. Caller treats failure as
"chip not responsive at this base" and moves on.

Belongs in the audio probe module, gates every DSP write.

### M0-H2: Cyrix FPU probe env-gating

HEARO `src/platform/probes.c:fpu_cyrix_probe`. Audit CERBERUS's FPU
detection. If it does the canonical `outp(0x22, 0xC3); inp(0x23)`
sequence unconditionally to identify Cyrix FasMath, the write to
port 0x22 has side effects on chipsets that decode 0x22/0x23
differently (some Intel PIIX variants, some VIA chipsets). The result
is cosmetic on identification (wrong FPU brand displayed) but the
chipset-side disturbance is non-cosmetic.

Gate behind `CERBERUS_CYRIX=1` env var, same idiom CERBERUS already
uses for `CERBERUS_*` test-mode flags. Skip the probe when env unset;
report FPU brand as "unknown" instead of best-guess.

## Test infrastructure (cross-cutting)

### TEST-H1: 86Box VM matrix as a CERBERUS test asset

Three FreeDOS 1.4 VMs at `C:\Tools\86Box\vms\`:
- `hearo-sb16` — Sound Blaster 16 reference
- `hearo-ymf715` — Yamaha OPL3-SA3 (the chip family CERBERUS most
  needs to identify correctly)
- `hearo-ess` — ESS ES1688 (clone-quirks reference)

All three are tf486 (Award BIOS, 486SX 16 MHz, 16 MB RAM, 250 MB FAT16
boot, 16 MB FAT16 transfer.vhd at guest D:). Identical except for the
sound card. Test loop is `_scripts\run-test.ps1 -Vm <vm> -Payload <dir>`:
host packs payload onto transfer.vhd, launches 86Box, guest's
AUTOEXEC runs `D:\AUTORUN.BAT`, AUTORUN ends with `FDAPM POWEROFF`,
host re-mounts and reads `D:\PROBE.LOG`.

The bootstrap step (one F1 click per VM at first BIOS boot) is the
only manual step; cannot be automated (synthetic keyboard input does
not reach 86Box's SDL canvas, empirically verified).

**For CERBERUS:** create a probe payload at
`C:\Tools\86Box\vms\_assets\cerberus-detect\` with at minimum:
- `CERBERUS.EXE` (current build)
- `AUTORUN.BAT` invoking CERBERUS with `/CSV` or detection-only flags,
  redirecting output, ending with `FDAPM POWEROFF`

Run on all three VMs and capture detection output. Catches a class of
audio-detection bug that is invisible on the 486 DX-2 + Vibra 16S real
iron (only one card family). HEARO uses the same VMs; the matrix is
shared infrastructure.

Reference: `~/.claude/projects/.../memory/reference_86box_vms.md` for
the full guide.

### TEST-H2: Multi-format / multi-platform smoketest matrix

HEARO's session smoketest was MOD-only (TONE.MOD against DOSBox-X
SB16). Round 3 review flagged this as a coverage gap. CERBERUS
already runs `wmake rc=0` plus a smoketest after each build per
`feedback_cerberus_smoketest_after_build.md`, but the smoketest
covers only one CPU class.

For 0.9.0 milestones M1–M7, define the smoketest matrix explicitly:
- DOSBox-X 386 / Pentium / 486 cycle settings (catches CPU-class
  branches in detect/cpu.c)
- 86Box `hearo-sb16` / `hearo-ymf715` / `hearo-ess` (catches audio
  family branches)
- Real iron 486 DX-2-66 + Vibra 16S (single-card primary regression
  baseline)
- Real iron 386 DX-40 + Aztech ISA (clone-compatibility validation)

The first two are local and can run between development passes; the
real-iron tier is per-milestone.

## Process discipline carried over

### PROC-H1: Iterative code review with fresh-eyes rotation

The HEARO session ran three review rounds, each with a fresh reviewer
(no anchoring on prior findings). Each round caught at least one fresh
issue:

- Round 1: 4 Important — fixed in commit `6755251`
- Round 2: 2 Important — fixed in commit `dfefcfd`
- Round 3: 2 Important — one fresh (variant code overwrite, fixed in
  `4b3d2d9`), one project-accepted stance

Pattern: dispatch a code-reviewer subagent per milestone, fix Critical
+ Important findings, re-dispatch with a NEW reviewer. Stagnation
signal (same Critical+Important count two rounds in a row) escalates
to user.

Manifest archived at `hearo/docs/research/session-2026-04-26-review-manifest.jsonl`
on the NetISA repo for reference.

For CERBERUS: apply per-milestone in 0.9.0. Each M-number gate gets
the iterative review treatment before moving to the next.

### PROC-H2: `Assisted-by` trailer on every AI-assisted commit

Already part of the working agreement. Continue. The HEARO session
had 15 of 16 commits with `Assisted-by: Claude:claude-opus-4-7`; the
one outlier was a docs commit that pre-dated the policy. CERBERUS
0.9.0 commits should be 100% compliant.

### PROC-H3: Read before write on chunk-A equivalents

HEARO declared chunk-A (`src/audio/*`, 10 files, 40 fixes) load-bearing
after a six-round quality gate. Touching chunk-A files post-gate
required surfacing as a question first.

CERBERUS has analogous load-bearing surface: the detection probes that
have been validated on real iron. Any change to those files should
surface as a question rather than land silently. The 0.9.0 plan's M1
DGROUP-reclaim work is the obvious risk surface.

## Sequencing recommendation

If 0.9.0 is unparked tomorrow, the leverage-vs-risk sort places
the HEARO-derived items roughly:

**Quickest wins (one commit each, low risk):**
- M0-H2 Cyrix env-gate (single-line defensive)
- M5-H2 atexit + signal handlers (drop-in, ~60 lines)
- M5-H1 MCB validation (drop-in, ~50 lines)

**Audio-detect enrichment (three commits, M2 scope):**
- M2-H2 0xE3 copyright probe
- M2-H3 mixer 0x81 cross-check
- M2-H1 OPL3-SAx variant identification

**Test infrastructure (one task each, parallel):**
- TEST-H1 cerberus-detect probe payload
- TEST-H2 explicit smoketest matrix definition

**Reference material (no commits):**
- M3 Mpxplay PCIBIOS pattern (consult when M3 lands)
- PROC-H1 iterative review (apply per M-gate)

A focused single-session pass on M0-H2, M5-H1, M5-H2, M2-H1, M2-H2,
M2-H3 plus TEST-H1 yields seven commits, all smoketestable, none
touching M1's DGROUP work or the existing detection probes' core
logic. Ships an enriched audio-identification report and hardened exit
path without committing to the deep 0.9.0 hardware-ID roadmap.

This is the analog of HEARO's Phase 1 quick-wins pass: small,
independent, smoketestable, lands without unparking the major
milestones.

## What does not carry over

- **Wake layer architecture.** HEARO `src/audio/wake.{c,h}` and
  `wake_opl3sa3.c` write chip state to ungate playback. CERBERUS is
  detect-only; writing chip state inverts the project's mission.
  Lift only the probe halves.
- **Frequency-scaled DMA buffer.** Playback-specific.
- **SB Pro stereo time-constant math.** Playback-specific.
- **playback module / UI ENTER wiring.** UI-specific to a player.
- **DSP write-error reset cascade in ISR context.** CERBERUS is
  foreground-only.

## Things explicitly unchanged in this addendum

- The 0.9.0 release scope as defined in `CERBERUS_0.9.0_PLAN.md`
  Section 3.
- The seven-milestone structure (M1–M7).
- The DGROUP budget gate (Section 5).
- The real-hardware validation matrix (Section 6).
- The CUA shell direction (M5).
- Whetstone retirement plan (M6).

This addendum adds detail to M2 / M3 / M5 and proposes a parallel
bookkeeping for cross-cutting items (M0-Hx, TEST-Hx, PROC-Hx). It does
not change deliverables.
