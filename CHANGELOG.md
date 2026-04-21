# Changelog

All notable changes to CERBERUS. Format loosely based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); dates are ISO-8601, hash references are short-sha from `main`.

## [v0.7.0-rc2], 2026-04-20 — end-to-end quality gate fixes

Release candidate 2 lands fixes surfaced by a systematic end-to-end
quality-gate audit of the v0.5.0 → v0.7.0-rc1 arc. No functional
changes from rc1; correctness + doc fixes only. Tag `v0.7.0` still
held until Part B server validation.

### Fixes

1. **`intro.c read_ticks()` atomic-read pattern.** `read_ticks()`
   was doing a single 32-bit pointer deref against the BIOS tick
   counter at 0040:006C. On 8088 that compiles to two 16-bit loads,
   and INT 8 can fire between them, corrupting the high word.
   Animation-only impact (the intro's `wait_ticks_or_key` was
   consuming the value) but pre-existing bug surfaced by the audit.
   Fixed to use the same atomic h1/l/h2 retry pattern that
   `timing.c` and `tui_util.c` already use.

2. **`upload.c htget_post()` UPLOAD.TMP cleanup on fopen failure.**
   Every failure branch of `htget_post` called `remove(UPLOAD_TMP_PATH)`
   except one: if HTGET wrote UPLOAD.TMP successfully but the
   subsequent `fopen()` failed (I/O error, permission denial, TSR
   interference), the stale file was left on disk and would be
   mis-read on the NEXT upload attempt. Added `remove()` to that
   branch.

3. **`[upload]` section INI duplicate-key emission.** The flow
   `report_write_ini() → upload_execute() → report_write_ini()` was
   appending new `upload.status` / `upload.submission_id` /
   `upload.url` rows on the second pass without deduplicating
   against the empty versions added on the first. Last-value-wins
   parsing tolerated this, but the INI file looked ugly with
   duplicated `upload.nickname=` lines. New helper
   `report_update_str()` updates in place (mirrors the existing
   `report_set_verdict` pattern). `upload.c:set_status` and
   `set_submission` switched to it.

4. **`[upload] status` enum documented.** `ini-format.md` now lists
   all six values the client actually emits (`uploaded`, `offline`,
   `skipped`, `no_client`, `failed`, `bad_response`) with
   when-emitted semantics. Contract in `ini-upload-contract.md`
   updated to match and explicitly requires server tolerance of
   the full enum.

5. **`[upload] url` documented.** Was emitted by the client on
   successful upload but absent from `ini-format.md` — server's
   permissive parser accepted it, but spec/code drift. Added to
   the format reference.

### Deferred to v0.7.1+

- `ui.c` and `intro.c` retain private VRAM helper copies predating
  the v0.6.2 `tui_util` extraction. Cleanup-only, no behavior
  delta; filed as refactor.
- `bench_cache.c:105` stale "TODO (v0.5)" comment; should move to
  issue tracker.

### Build state

- CERBERUS.EXE: 164,050 bytes (+128 vs rc1; report_update_str + comments)
- MD5: 4042491e8334d904c561bb0942ec092a
- DGROUP: 59,168 / 64,000 DOS limit (soft 56K target exceeded,
  accepted)
- Host tests: 7 suites, 201 OK, 0 failures
- Zero warnings on clean rebuild
- Version string: `0.7.0-rc2`

## [v0.7.0-rc1], 2026-04-20 evening — Part A of Community Upload

**Status**: Release candidate. Tag is `v0.7.0-rc1`, NOT `v0.7.0`. Per
the v0.7.0 brief, the full `v0.7.0` tag is reserved until Part B
(server + results browser in a separate repo) is deployed and an
end-to-end round-trip has been validated on real hardware. This
release is the DOS client side complete + documented + waiting.

Part A ships the full upload-client infrastructure: network transport
detection, INI format freeze with stable API contract, command-line
flags, prompt UI, HTTP POST via HTGET shell-out, and UPLOAD STATUS
section in the scrollable summary.

### T0 — INI format freeze + server contract

`[cerberus]` section gains `ini_format=1` — the server-parser API
switch. Additive INI changes (new keys, new sections) stay at
`ini_format=1`; breaking changes would bump. Documented in
`docs/ini-format.md`.

Upload contract specified in `docs/ini-upload-contract.md`: endpoint
URL, request format, response format, which INI fields the server
parses, error handling. Written BEFORE Part B so the server session
inherits it as a requirements spec instead of a blank page.

### T1 — Network transport detection

New module `src/detect/network.{c,h}`. Probes at startup:

  1. NetISA via INT 63h (reserved for v0.8.0 TLS; stub detector)
  2. Packet driver via INT 60h-7Fh scan for "PKT DRVR" signature
     at handler offset +3
  3. mTCP via `MTCP_CFG` env var
  4. WATTCP via `WATTCP` / `WATTCP_CFG` env var
  5. `none` — offline

Emits `[network] transport=<value>`. All probes are non-destructive
IVT reads + env lookups; safe on 8088 and every adapter.

### T2 — Upload prompt

After detection + journey + summary build, the upload orchestrator
checks network state. If online and not `/NOUPLOAD` / `/UPLOAD`,
prompts `"Upload results to barelybooting.com? (Y/n)"`. Default Y;
Enter / Y proceeds; N / Esc skips.

### T3 — HTTP POST via HTGET shell-out

v0.7.0 uses mTCP's `HTGET` as the HTTP client (spec-driven choice
from the brief). Command:

```
HTGET -P CERBERUS.INI -m text/plain http://barelybooting.com/api/v1/submit > UPLOAD.TMP
```

Parses response from `UPLOAD.TMP`: line 1 = submission ID, line 2 = URL.
Clean failure modes on connection refused, DNS miss, non-200, timeout.
If `HTGET.EXE` is absent from PATH, prints install instructions and
skips with status=`no_client` — never crashes.

The exact HTGET flag syntax may need field-verification during the
first real deploy; it's encapsulated in one `#define HTGET_CMD_FMT`
line for easy adjustment. Raw TCP over packet driver (no mTCP)
remains deferred to v0.8.0+ per the brief.

### T4 — Command-line flags

  `/NOUPLOAD`     never prompt, never upload
  `/UPLOAD`       upload without prompting (auto-yes)
  `/NICK:<name>`  nickname (alnum + space + hyphen, max 32, sanitized)
  `/NOTE:<text>`  note (printable ASCII, max 128)

Help text + synopsis updated.

### T5 — UPLOAD STATUS summary section

New fourth section in the scrollable summary, after SYSTEM VERDICTS.
Uses HEAD_CENTER (forward-facing, "sending data outward" fits the
direction metaphor; avoids proliferating head variants).

Rows shown (each skips if empty):
  Network:    packet driver / mtcp / wattcp / offline
  Status:     uploaded / skipped / offline / failed / no_client
  Submission: 8-char hex id (populated after 200 response)
  URL:        public view URL (same)
  Nickname:   if `/NICK` set
  Notes:      if `/NOTE` set

### Build state

- CERBERUS.EXE: 163,922 bytes (target <185KB ✓)
- DGROUP: 59,008 bytes (past Tony's 56K soft target but well under
  DOS 64KB limit — acceptable per v0.6.1 "exceed is OK" sign-off)
- Host tests: 7 suites, 201 OK, 0 failures
- Version string: `0.7.0-rc1`
- Tag: `v0.7.0-rc1` (not v0.7.0)

### Held back

- **Tag `v0.7.0`** reserved until Part B server exists and an
  end-to-end POST round-trip is validated on BEK-V409.
- **Part B server** is a separate project / repo / Claude session.
  Contract is defined in `docs/ini-upload-contract.md`.

## [v0.6.2], 2026-04-20 evening

Cleanup + SB DSP direct-mode PCM audio path.

### T1 — Shared TUI helpers (src/core/tui_util.{c,h})

Six visual modules (journey, bit_parade, lissajous, metronome,
audio_scale, cache_waterfall, latency_map) each carried a private
~60-line copy of the same VRAM-helper block: adapter-aware
vram_base, is_mono, putc, puts, fill, hline, ticks, keyboard
polling. All consolidated into one shared tui_util module.

Each visual module now calls tui_putc / tui_puts / tui_fill /
tui_ticks / tui_is_mono / tui_kbhit / tui_read_key / tui_drain_keys
instead of its private prefix. EXE shrunk ~1 KB from code
deduplication.

### T2 — DGROUP reclaim DEFERRED

Tried Watcom `-zc` in v0.6.1 (doesn't move unnamed string literals,
only const-qualified declarations). wlink CLASS redirection via
ORDER statements needs per-file `#pragma data_seg` directives to
mark which data goes where — touches every .c file for a marginal
savings given DGROUP is currently 57,472 bytes (6,864 bytes under
the DOS 64KB limit). Not worth the churn. Future work if DGROUP
ever constrains.

### T3 — SB DSP direct-mode PCM audio path (T7c from v0.6.0 brief)

audio_scale.c now probes a third audio path ahead of OPL2:

  Probe order
    1. SB DSP direct PCM — BLASTER env parsed for base port; DSP
       reset sequence at base+6 tested for the 0xAA ACK. If present,
       plays the scale as square-wave PCM samples via DSP command
       0x10 per sample (direct-output mode).
    2. OPL2 FM — fallback. Same as v0.6.1.
    3. PC speaker — universal fallback.

Scope decision: DSP direct mode instead of full DMA-buffered
playback. Direct mode is genuine PCM (the SB DSP outputs the actual
sample byte on each 0x10 write) but runs at a lower effective sample
rate (~2-4 kHz via Watcom busy-wait). The "is my SB card producing
PCM samples at all" question gets a direct yes/no answer; fidelity
is squarer than DMA-streamed audio but clearly distinct from OPL
FM. Full DMA playback (8237 channel + buffer + IRQ) stays filed as
a future v0.7+ candidate if the audio-verification story needs the
quality upgrade.

Square-wave generation: per-note half-period in samples precomputed
from target frequency at an assumed 3 kHz effective rate (~150 ms
per note × 8 notes ≈ 1.2 s total playback).

Title card + on-screen heading change to match the active path —
users see "Audio Scale — SB DSP Direct PCM" when SB wins the probe,
"Audio Scale — OPL2 FM Synth" when OPL wins, "Audio Scale — PC
Speaker" on fallback.

### Build state

- CERBERUS.EXE: 156,700 bytes (target <180KB ✓)
- DGROUP: 57,472 / 56,000 soft target (6,864 bytes under DOS 64KB)
- Host tests: 7 suites, 201 OK, 0 failures

### Known follow-ups

- Issue #4 Whetstone calibration — still pending real-hardware
  multi-cold-boot validation.
- Issue #6 VLB bandwidth — still pending.
- SB16 full DMA audio path (8237 auto-init + IRQ) remains a v0.7+
  candidate. DSP direct is functionally sufficient for the
  journey's "does audio work" question.

## [v0.6.1], 2026-04-20 evening

Visual-journey completeness pass. Closes the gaps flagged in the
v0.6.0 "Known gaps" list.

### Result flashes wired (T1.1)

journey_result_flash() was authored in v0.6.0 but no visual called
it. Now all six visuals end with a one-line result banner:
  Bit parade    "ALU: every op verified on a live register"
  Lissajous     "FPU: trig functions produced a symmetric curve"
  Mandelbrot    "FPU: Whetstone done, Mandelbrot rendered live"
  Metronome     "Timer: PIT ticking at 18.2 Hz"
  Video pattern "Video: bandwidth measured on live VRAM"
  Audio scale   "Audio: speaker path verified end-to-end"

Each renders centered on row 12 for ~1 second before the next title
card takes over.

### T4 Memory Cache Waterfall

Text-mode 9-band bar chart, one band per block size (1B, 2B, 4B, 16B,
256B, 1KB, 4KB, 16KB, 64KB). Each band's fill length + animation
speed is proportional to measured write bandwidth at that block size
against a 32 KB FAR-allocated buffer.

Colors: bright green for fastest tier, yellow middle, bright red for
slowest. On cached systems the small-block bars land in the green
tier (L1 speed); large-block bars land in red (main-memory speed).
The visible transition IS the cache boundary.

All adapters, text mode. No VGA-specific path; the bars are CP437
block characters and the color classes degrade cleanly on mono.

### T5 Cache Latency Heat Map

Text-mode horizontal heat strip, 64 cells spanning the width of an
allocated 32 KB buffer (one cell per 512-byte window). Each cell
timed by tight read-sweep; heat level classified into 4 quartiles
of the min→max range. Green/yellow/red attribute classes.

On a 486 with 8 KB L1 cache the expected pattern is: first 16 cells
green (in cache), remainder transition through yellow to red. On
cacheless systems the entire strip should be red.

Annotation labels mark 0 KB, 16 KB, 32 KB and include a legend
below.

### T7b OPL2 FM scale

audio_scale.c now probes port 0x388 for an AdLib/OPL2 chip before
playing the 8-note scale. If probe succeeds: play via OPL2 with a
clean sine voice (modulator silent, carrier sine wave, no feedback);
title changes to "Audio Scale — OPL2 FM Synth." If probe fails:
falls back to the v0.6.0 PC-speaker path.

OPL2 probe uses the standard timer-status sequence (write 0x60 to
reg 0x04, clear status, start timer 1, 100us wait, check status for
0xC0 pattern). Universal across AdLib, Sound Blaster 1, SB Pro,
SB16. Probe is ~400us total — imperceptible.

### T7c SB16 PCM DMA — DEFERRED to v0.6.2

Original brief had a third audio layer: PCM DMA playback of
triangle/square samples via 8237 + SB DSP for Sound Blaster 16+
machines. Scoped out for v0.6.1 because:
  - OPL2 FM covers AdLib + SB1 + SB Pro + SB16 (all cards with an
    OPL chip) with equivalent short-scale audio fidelity.
  - PCM adds complexity (DMA channel programming, DSP init, buffer
    management, IRQ handling) for marginal audible benefit on an
    8-note 2-second scale.

OPL2 path ships in v0.6.1; PCM DMA a v0.6.2 candidate if a use case
surfaces that actually needs the upgrade.

### Build state

- CERBERUS.EXE: 157,002 bytes (target <180KB; 165KB flag threshold
  not tripped)
- DGROUP: 57,280 / 56,000 soft target exceeded (7,256 bytes under
  DOS 64KB limit — Tony confirmed acceptable for v0.6.1)
- Host tests: 7 suites, 201 OK, 0 failures

### DGROUP note

`-zc` was tried as a path to move const strings out of DGROUP; it
doesn't move unnamed string literals (only explicitly const-qualified
declarations). Left disabled. Future optimization candidate:
renamed CONST class with a wlink ORDER directive to park it in a
far segment. Not a v0.6.1 blocker.

## [v0.6.0], 2026-04-20 evening

The Visual Diagnostics Journey release. CERBERUS becomes an experience
that shows hardware working, not just a tool that emits numbers. Six
new visual demonstrations, a title-card framework that glues them into
a structured run, a new tagline, directional head art that gives each
section its own mythological guardian, and a /QUICK flag for users
who want measurements without the visuals.

### T0a — new tagline (`6c255e1`)

"Three heads. One machine. Zero pretending." retires. Replaced by
"Tough Times Demand Tough Tests" on the splash screen subtitle row
and in the animation's BEHOLD-flash swap-back target.

### T0b — chain removed from static title

The chain/body/serpent-tail composition below the three heads is gone
from the static resting state. It read as a progress widget and broke
the wordmark → heads → tagline flow. The animation's chain rattle +
shatter still run, and the post-shatter redraw explicitly clears the
chain-area rows so nothing lingers.

### T0c — directional head art

New shared module `src/core/head_art.{c,h}` with three 9x4 variants:
  HEAD_LEFT    — faces left, eye at col 6, snout + fang at col 1,
                  ear bump top-right.
  HEAD_CENTER  — faces forward, two eyes + two fangs, dominant.
  HEAD_RIGHT   — mirror of LEFT, faces right.

Accent kinds (BODY / EYE / FANG) let renderers color each feature
independently for animation pulses. The intro splash uses all three
variants left-to-right; the summary section headers guard their
domains (Detection=LEFT scanning, Benchmarks=CENTER measuring,
Verdicts=RIGHT judging). A new shared-neck row at DOG_TOP+4 sells
"one creature with three heads" rather than three separate dogs.

Intro eye-cascade animation adapts to the new 4-eye total (1 on left
head, 2 on center, 1 on right); eye_head_map[] routes each cell to
its head's heat attr.

### T1 — journey framework (`6c255e1`)

New module `src/core/journey.{c,h}`:
  journey_init()             — reset skip-all latch per run
  journey_should_skip(o)     — true under /NOUI, /QUICK, or skip-all
  journey_title_card()       — full-screen card with directional head,
                                ALL-CAPS title, 1-3 wrapped lines of
                                description, "any key / S / Esc" hint.
                                ~2.5s hold or keypress advance.
  journey_result_flash()     — single-line banner on row 12, ~1s
  journey_poll_skip()        — non-blocking keyboard poll during
                                visuals: 0=continue, 1=skip this,
                                2=skip all (Esc latches)

New /QUICK command-line flag: skips visuals + title cards, runs
measurements, renders interactive summary. For batch users who want
timings without the journey.

### T2 — CPU ALU Rolling Bit Parade (`a55a293`)

Post-diag_cpu. 16-bit register rendered as CP437 block row (bright =
1, light shade = 0). Real ALU ops executed in sequence: ROL, ROR,
SHL, SHR, AND, OR, XOR, NOT, ADD, SUB. Each state displayed is the
literal result of the literal instruction — no animation. Wall-clock
bounded (~3 s); 8088 shows ops ticking past, 486 blurs. Text mode,
all adapters.

### T3 — FPU Lissajous Curve (`3e4d4f2`)

Post-diag_fpu. VGA mode 13h. 1800 parametric points x=sin(3t+π/4),
y=sin(2t), drawn one by one by the x87 native FSIN. Amber
oscilloscope palette. A working FPU produces a smooth symmetric 3:2
figure. Gated on VGA-capable + fpu.detected != "none".

### T4 / T5 — DEFERRED to v0.6.1

Cache Waterfall + Latency Heat Map visuals were in the brief but
need more care around per-band measurement methodology than fit in
this session. Ship v0.6.0 without them; add in a follow-up pass
once the cache-measurement primitives have had a real-hardware
calibration round.

### T6 — PIT Metronome (`7b8405e`)

Post-timing_self_check. A dot bounces between columns 4 and 75 at
row 12, one column per 18.2 Hz BIOS tick. Each tick fires a PC
speaker click via port 61h bit-1 toggle. Text mode, all adapters,
universal hardware. Steady rhythm = PIT and BIOS agree; stutter =
something between them is biased (same signal Rule 4a checks
numerically).

### T7 — Audio Hardware Scale (this commit)

End-of-journey audio coda. 8-note C major scale (C4 through C5) via
PIT Channel 2 gated through port 61h. Each note ~250 ms. Rising
vertical bars accompany the notes. v0.6.0 ships PC-speaker only —
OPL2 FM and SB16 PCM DMA are deferred to v0.6.1. PC speaker is
universal hardware so this always fires (subject to skip flags).

### T8 — title cards for existing visuals

bench_whetstone gets an FPU BENCHMARK title card (covers both
Whetstone measurement + the Mandelbrot visual that fires at its
tail). bench_video gets a VIDEO BANDWIDTH card before its pattern
fill (the pattern IS the measurement). bench_mandelbrot reuses the
parent section's card — no double-card.

### Build state

- CERBERUS.EXE: 152,058 bytes (target <180KB)
- DGROUP: 56,080 / 56,000 (80 bytes over soft target; 8,456 bytes
  under the DOS 64KB limit)
- Host tests: 7 suites, 201 OK, 0 failures
- Version: 0.6.0

### Known gaps

- T4 Cache Waterfall — deferred to v0.6.1
- T5 Latency Heat Map — deferred to v0.6.1
- T7 OPL2 + SB16 audio paths — deferred to v0.6.1 (PC speaker ships)
- journey_result_flash() is authored but currently unused; visuals
  transition straight to the next title card. Adding the flashes is
  a polish item for v0.6.1.
- Issue #4 Whetstone calibration still pending real-hardware
  validation (inherited from v0.5.0).
- Issue #6 VLB bandwidth still open.

## [v0.5.0], 2026-04-20 evening

v0.5.0 is a UI + FPU release. The three-pane fixed-width summary
retires in favor of a scrollable full-width layout where each
section (Detection, Benchmarks, System Verdicts) is headed by a
Cerberus dog head in CP437 block art — the mythology becomes
literal, three heads guarding three domains. The Whetstone
benchmark gets a hand-coded x87 assembly kernel, and a Mandelbrot
set visual fires at the end as a post-run proof-of-life for the
FPU.

### v0.4.1 UI polish (`3b88d3a`)

Landed first on top of v0.4.0 as a recovery point before the
v0.5.0 rewrite. Narration rewrites so no consistency-rule text
truncates in the 48-col SYSTEM VERDICTS pane (the original defect
hid the exoneration on Rule 4b's "cache diag PASS" branch).
Human-readable DETECTION labels (`CPU`/`FPU`/`BIOS`/`Emulator` with
acronym preservation, sentence case elsewhere). `<TAG>` brackets
for diagnostic heads, `[TAG]` for consistency rules. BENCHMARKS
grouped by subsystem (CPU/FPU raw, memory raw, PC-XT ratios).
Runtime `audit_narration_widths` assert in consist.c fires to
stderr if any narration exceeds 48 cols on-screen after prefix
strip. Validated on BEK-V409 CRT as `CERB-UI2.EXE` (151,044 B).

### T1 — CONF_LOW text marker (`b769f08`)

Dim-color rendering of CONF_LOW values replaced with an explicit
`" (low conf.)"` text marker appended at render time. Text is
adapter-neutral and self-documenting in screenshots;
render_kv_row_dim_value deleted; per-row dim flags in the bench
table gone. Data owns its confidence; the UI surfaces it.

### T2 — scrollable three-heads summary (`00d6f6b`)

Full ui.c rewrite. Virtual-row table (80-entry cap) holds one
entry per content row; viewport renders 24 rows starting at
scroll_top. Each section is prefixed by a 9x4 CP437 Cerberus dog
head using the same visual primitives as intro.c (dark-shade
skull, half-block edges, eye glyph, fang glyph). BIOS INT 16h
navigation — Up/Dn arrows scroll one row, PgUp/PgDn one viewport,
Home/End jump top/bottom, Q or Esc exits. Status bar on row 24
shows current row range ("rows 1-24 of 54") and nav hints.

No information truncation anywhere: the old 8-row VERDICTS pane
cap silently dropped the 9th verdict on BEK-V409 (Whetstone FPU
was consistently lost). With the scroll buffer, all verdicts
display regardless of count. /NOUI now dispatches to
`ui_render_batch()` which prints the same content as plain text to
stdout — batch mode preserves the issue-#3 escape hatch while
still surfacing run results.

### T3 — Whetstone verdict investigation (no code)

Documented: the old `ui_render_consistency_alerts` capped at
`r <= 24` which is 8 rows in rows 17-24. On BEK-V409 9 verdicts
fire (cache.status, dma.summary, 7 consistency rules), so the 9th
(whetstone_fpu, last emitted) was silently dropped. T2's scroll
buffer self-fixes this.

### T4a — Whetstone x87 asm kernel (`3537c00`)

New file `src/bench/bench_whet_fpu.asm` (NASM). Hand-coded
Curnow-Wichmann 1976 Whetstone using native x87 (FSIN, FCOS,
FPATAN, FSQRT, F2XM1, FYL2X). Replaces the Watcom-compiled C
kernel that was forced through volatile memory traffic for DCE
suppression. Dispatch: FPU present → asm kernel at CONF_HIGH; no
FPU → skip (as before). bench_whetstone.c now compiles at -ox
instead of -od because the asm kernel is opaque to the optimizer
and owns the DCE-barrier responsibility via its external globals.

Issue #4 stays open. Real-hardware validation on BEK-V409 is the
next step: the published 486 DX-2-66 envelope is 1500-3000 K-Whet
and pre-asm C ran ~110. Multi-cold-boot capture needed before the
CONF_HIGH claim gets anchored.

### T4b — Mandelbrot FPU visual (`e2f3dea`)

`src/bench/bench_mandelbrot.c`. Fires at end of bench_whetstone
on FPU-equipped VGA-capable machines. VGA mode 13h, 320x200x256.
Center (-0.6, 0), window [-2.0, 0.8] x [-1.2, 1.2], 64
iterations. Pixel-by-pixel progressive render so the fractal
emerges visibly on a 486. DAC palette is a blue → cyan → white
gradient programmed via 0x3C8/0x3C9. Blocking INT 16h keypress
exits, mode 3h restores text. /NOUI skips. Non-VGA skips without
mode-switching.

Not timed, not reported as a measured value. A post-run visual
coda that makes the tool memorable and proves the FPU is live.

### T4c — forensic-value INI emit for Rule 4b (`b639d6a`)

Rule 4b (cpu_ipc_bench) now emits three additional INI rows when
it fires on any branch:
- `consistency.cpu_ipc_bench.measured` (bench iters/sec)
- `consistency.cpu_ipc_bench.expected_low` (CPU-DB low bound)
- `consistency.cpu_ipc_bench.expected_high` (CPU-DB high bound)

VERDICT_UNKNOWN on the forensic rows keeps them INI-only (filtered
from the SYSTEM VERDICTS pane). Post-run readers can reconstruct
why Rule 4b fired without re-running the tool. Pattern sourced
from the CACHECHK Phase 3 "Timer messed up! %08lx" forensic-emit
lesson.

### Build state

- `CERBERUS.EXE`: 146,290 bytes (target <160KB)
- DGROUP: 54,528 / 56,000 (1,472 bytes headroom)
- Host tests: 7 suites, 201 OK, 0 failures
- Version string: `0.5.0`

### Issue posture

- **#1** closed in v0.4.1 work (test_timing reassertions).
- **#3** closed pre-v0.5.0 (UI hang cannot reproduce).
- **#4** OPEN — Whetstone calibration needs multi-cold-boot
  BEK-V409 capture.
- **#6** OPEN — VLB bandwidth investigation continues.

## [Unreleased, post-v0.4.0], 2026-04-19 evening through 2026-04-20

Development work on `main` past the `v0.4.0` tag. Not yet
collected under a rc/final tag; next anchor point will be
either a `v0.5.0-alpha` pre-release for the ANSI intro and
instrumentation, or a full `v0.5.0` once issue #4 and issue
#6 close.

### ANSI boot intro (`593b139`)

New module `src/core/intro.c` + `intro.h` plus wiring into
`main.c` between `display_init` and `display_banner`. Adapter-
aware three-headed-dog emblem with OPL2 stinger. Gated by
`/NOINTRO` (already plumbed in `opts_t`) and `/NOUI`. Five
iconographic elements (three heads with fangs, serpent-mane
spines, chain-bound body, serpent tail, double-line gate)
plus seven embellishments (heartbeat pre-sequence, eye
cascade with escalating OPL2 barks, chain-shatter DAC flash
with rhythm-mode snare, sustained A-minor chord with vibrato,
sub-bass drone, hellfire ember row, chain rattle around
broken link, breath sparks, serpent tail wiggle, wordmark
color pulse, BEHOLD mid-sustain flash). Full classical
iconography sourced from Pseudo-Apollodorus, Virgil, Dante,
and Hercules 12th-labor Roman iconography.

Real-hardware validated on BEK-V409 as `CERB-INT.EXE`
(150,042 bytes). User reaction: "Wow that looked epic."
Detect/diag/bench flow after the intro produces numbers
consistent with v0.4.0 baseline; no regressions.

### Issue-fix sweep

- **Issue #1 (`036cc1c`).** The four pre-existing
  `test_timing` assertion failures were written before a 25%
  pit/bios divergence guard landed in `timing_compute_dual`;
  their c2 values produced highly divergent pit_us vs bios_us
  and now correctly trigger that guard. Updated the test
  inputs to use near-full-wrap sub_ticks values that satisfy
  all three kernel gates (lower-bail, upper-bail, divergence)
  while preserving the test's original semantic (verifying
  that target=1 with 0 wraps does not trip the lower-bail).
  Added dedicated coverage for the divergence guard itself
  (both pit>>bios and bios>>pit branches) which had no test
  coverage before. 167 host-test assertions green, 0 failures.
  Previous state: 163 green, 4 failed.

- **Issue #3 closed.** UI hang unable to reproduce across 9
  consecutive clean real-iron runs (v0.2-rc1 through v0.4.0
  plus CERB-VOX diagnostic plus both CERB-INT runs). Closed
  as "cannot reproduce" with reopen criteria preserved.
  Instrumentation stash at
  `docs/plans/attic/ui-hang-instrumentation-2026-04-18.patch`
  available for re-application if the hang re-emerges.

- **Issue #6 test-1 data-point landed earlier this session.**
  Built `CERB-VOX.EXE` at 144,054 bytes with `bench_video.c`
  compiled at `-ox` instead of `CFLAGS_NOOPT`. BEK-V409
  results: `text_write_kbps` 4,988 (+6.9% vs 4,668 at
  CFLAGS_NOOPT) and `mode13h_kbps` 5,021 (+8.8% vs 4,613).
  CFLAGS_NOOPT tax is small, not the dominant factor.
  Posted as comment to issue #6; bottleneck is not compile
  flags.

- **Issue #6 test-2 tool built (`2f5b26e`).** New standalone
  `tools/repstosd/REPSTOSD.EXE` (11,258 bytes) with pure-
  assembly REP STOSW inner loop writing 128 MB to mode 13h
  VRAM. Isolates C-loop overhead vs hardware-path limitation.
  Shipped to BEK-V409 for Tony to run; output will tell us
  whether CERBERUS's 4.6 MB/s number reflects a real hardware
  ceiling or a C-loop-overhead artifact.

- **Issue #2 instrumentation (`74bc439`).** New INI key
  `audio.opl_probe_trace` emits byte-level trace of every
  status-register read across primary and fallback OPL
  probe attempts. Enables multi-cold-boot capture-and-diff
  to identify which byte value differs between "opl3
  detected" runs and "none detected" runs on the same
  Vibra 16 PnP card. Built into `CERB-DBG.EXE` on BEK-V409
  (150,556 bytes).

### Homage Phase 3 research (`37777f3`)

Seven additional lesson docs in `docs/research/homage/` under
the same ethical frame as Phase 2: no decompiled code, no
binary redistribution, attribution preserved, corrections
flagged openly.

Deferred-from-Phase-2 tasks now closed: T3 CheckIt
Whetstone (confirmed custom synthetic, not Curnow-Wichmann;
reframes issue #4), T5 CheckIt video methodology (text-mode
only, no mode 13h reference), T8 CACHECHK UMC timer
workaround (structural match to CERBERUS; raw-forensic-emit
pattern filed as v0.5+ Rule 4a enhancement), T9+T10
SPEEDSYS (Afanasiev attribution confirmed, Russian origin;
Pentium-era peer, out of CERBERUS scope).

New issue-#6 second-opinion research: T14 PCPBENCH (PC
Player magazine / Computec Media, German origin; DOS/4GW
32-bit 3D with REP STOSD primitives), T15 3DBENCH v1 and
v2 (Superscape VRT Ltd, UK; per-frame phase breakdown with
dedicated Clr column is the sharpest issue-#6 comparator
in the corpus), T16 CHRISB (DJGPP 1996; SVGA variant's S3
path relevant to BEK-V409 Trio64), T17 LM60 Landmark Speed
6.0 (same IBM PC/XT anchor as CheckIt, era convention).

Three attribution corrections this pass: SPEEDSYS (Roedy
Green → Vladimir Afanasiev), PCPBENCH (Jim Leonard → PC
Player magazine), 3DBENCH (Future Crew → Superscape VRT
Ltd).

### Envelope at end of 2026-04-19 work block

- `main` HEAD: `37777f3`
- EXE (tip-of-tree, CERB-DBG / non-tag): 150,556 bytes
- DGROUP: 53,808 / 56,000 (4% headroom under working
  ceiling; 18% under 65,536 hardware ceiling)
- Host tests: 167 assertions, all green
- Phase 2 lesson docs: 7 (unchanged from v0.4.0)
- Phase 3 lesson docs: 7 (added this session)
- GitHub issues: 2 closed (#3, #5), 4 open (#1 gated, #2
  instrumented, #4 reframed, #6 two tests built)

## [v0.4.0], 2026-04-19

Fourth release in the weekend arc. Closes the UI defects found in v0.4-rc1's BEK-V409 screenshot. Full release notes at [`docs/releases/v0.4.0.md`](docs/releases/v0.4.0.md).

- `diag_cache` and `consist` status-string CP437 corruption (`2d6a0a7`). UTF-8 em-dash and multiplication-sign bytes in runtime-emitted format strings were rendering as CP437 garbage in the SYSTEM VERDICTS pane. Replaced with ASCII equivalents. Audit swept three latent paths in `consist.c` (Rule 4b cache-contextualized WARNs plus Rule 11 XT-class DMA WARN).
- UI `value_str` type-aware rendering (`841d7c3`). Five V_U32 BENCHMARKS rows (`fpu ops/s`, `mem write`, `mem read`, `mem copy`, `k-whet (LOW)`) rendered labels but blank values in the rc1 screenshot because `value_str` returned `""` for non-V_STR rows with `display=NULL`. Mirrored `format_result_value`'s switch inside `value_str` with a 32-byte static scratch buffer.
- `bench_dhrystone.c` comment updated per Phase 2 T2 lesson (`6acb559`). CheckIt's "Dhrystones" is a custom synthetic, not a Dhrystone port. Comment now frames the 33,609 BEK-V409 anchor as an empirical match target, not an algorithmic equivalence. Full derivation at `docs/research/homage/checkit-dhrystone-version.md`.

EXE: 144,166 bytes. DGROUP: 53,184 / 56,000. Host tests: 163 assertions green except the 4 pre-existing `test_timing` failures (issue #1, gated). Real-hardware validated on BEK-V409 with full INI capture at `tests/captures/486-real-2026-04-19-v0.4.0/` plus four screenshots at `docs/releases/v0.4.0/screenshots/`.

## [Unreleased, covers v0.2-rc1 through v0.4-rc1]

### Detection

- Emulator / environment confidence-clamping path (`d8529d3`). All downstream detection rows carry an explicit confidence that gets clamped to MEDIUM when the environment is identified as an emulator, so a DOSBox-X run and a bench-box run look distinguishable in the INI.
- CPU class detection covering 8086 / 8088 / V20 / V30 / 286 / 386SX / 386DX / 486SX / 486DX / CPUID-capable (`cb9d588`), with a 34-entry CPU database (`0934486`) and CPUID vendor/family/model/stepping extraction.
- FPU detection (`872657c`) with a 14-entry database: integrated-486, 287, 387, RapidCAD, no-FPU, and the V20/V30 8087-socket path.
- Memory detection (`6b9b06c`): INT 12h conventional, INT 15h AH=88h / AX=E801h extended, XMS driver presence via INT 2Fh AX=4310h, EMS via INT 67h.
- Cache detection (`8fc9d9b`) — minimum-viable inference-from-CPU-class; real cache-stride detection deferred to Phase 3.
- Bus detection (`b145a57`) — PCI BIOS probe + ISA inference fallback, VLB "possible on 386DX+/486" heuristic.
- Video detection (`3b159e0`) with a 28-entry chipset database covering MDA / CGA / Hercules / EGA / VGA classes and S3 / Tseng / ATI / Trident / Cirrus / Paradise chipset identification.
- Audio detection (`80b5fec`) with a 24-entry database covering PC speaker / AdLib / OPL2 / OPL3 and the Sound Blaster DSP-version family (original through AWE64).
- BIOS info + family database (`e90a316`) — date-string scan, copyright extraction, PnP header detection.
- Unknown-hardware submission path (`18a5601`) — `CERBERUS.UNK` capture + end-of-run summary card inviting a GitHub issue with the probe data.
- Post-detection summary UI (`d6cd223`) — formatted text panel with CP437 confidence meters, minimum-viable for v0.2 (three-pane polish deferred).

#### Real-iron fixes from the 2026-04-18 bench-box gate (`eeba319`)

All five surfaced on contact with an actual BEK-V409 / i486DX-2 / S3 Trio64 / Vibra 16S box. None reproduce in DOSBox-X.

- **HIMEM.SYS intercepts INT 15h AX=E801h.** `detect_mem` now acquires the XMS entry point via INT 2Fh AX=4310h and calls AH=08h directly. `extended_kb=0` → `extended_kb=63076` on the 64 MB bench box.
- **S3 Trio64 option ROM carries "IBM VGA" string.** The substring scan matched the first hit. Added `probe_s3_chipid()` — unlocks S3 extended CRTC (write 0x48 to CR38) and reads CR30. `0xE1` → Trio64, `0xE6` → Virge. Runs ahead of the BIOS-string scan.
- **Vibra 16S DSP status port is base+0x0E, not base+0x0A.** Creative's DATA register lives at 0x0A, STATUS at 0x0E. Fix: poll the right port; DSP v4.13 now detected.
- **OPL 0x388 mirror disabled by CTCM on Vibra 16 PnP.** OPL3 undetected on a demonstrably-working card. Fix: probe BLASTER-base+8 first, fall back to 0x388. Partial: residual intermittency tracked in [#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2).
- **UMC491 8254 latch race produces biased phantom wraps.** PIT-C2 vs BIOS-tick cross-check reported 49% divergence every run. Fix: upper-bound the wrap count, require low-band→high-band shape for a valid wrap, and reject measurements at >25% post-hoc PIT/BIOS divergence (`measurement_failed` instead of biased data). Refined in `6c3a023` after the biased-misread pattern was identified.

#### Audio disambiguation (`7e4bdcb`)

- `audio.csv` composite match-key now tries `"<opl>:<dsp>:T<n>"` first (using the BLASTER T token), falls back to the bare `"<opl>:<dsp>"` on miss. Splits the DSP-4.13 family that was collapsing all onto a single wrongly-labeled AWE64 row. New disambiguated rows: `opl3:040D:T6` = SB16 / Vibra 16S, `opl3:040D:T8` = AWE32, `opl3:0410:T9` = AWE64 (CT4500). DSP-version table for 0x040B/0C/10/11 corrected against Creative's programmer's reference (AWE64 lives at 4.16/4.17, not 4.13).

#### Audio mixer-chip probe (`c22e886`)

- CT1745 mixer discriminator at BLASTER-base+4 / +5 reg 0x80 (Interrupt Setup). Returns `CT1745` / `none` / `unknown`. `audio.csv` grows a `mixer_chip` column. CT1745 seeded for SB16 CT2230 / CT2290 / Vibra 16S T6 family; all other 28 rows `unknown` pending real-hardware verification. Consumed by new consistency Rule 7.

### Diagnostics

- ALU + memory-pattern diagnostics (`a565a05`) — walking-1s / walking-0s / address-in-address patterns, flag-register correctness, stuck-bit detection.
- FPU correctness diagnostic (`cfd8ffd`) — known-answer bit-exact tests for add / sub / mul / div / compound expressions.
- Video-RAM diagnostic (`667d7a3`) — direct VRAM walk, plane consistency on EGA / VGA.
- Cache-coherence + DMA diagnostics deferred (`3ca0d7e`) — documented in plan, no stubs left in code.

- **Cache-coherence diagnostic** (`7a28850`) — stride-ratio timing test: 2 KB small buffer + 32 KB large buffer (both __far), compare per-iteration times. Verdict: ratio ≥ 40× = PASS (cache working), 20-40× = WARN (partial), < 20× = FAIL (cache disabled or absent). Skips on 8088-class floor hardware where `cache.present=no` from detect. Pure classifier kernel `diag_cache_classify_ratio_x100` host-tested across 17 scenarios.
- **DMA controller diagnostic** (`7a28850`) — 8237 count-register write+readback probe on channels 1/2/3/5/6/7. Safety-skips channel 0 (DRAM refresh) and channel 4 (cascade link). XT-class detection via `cpu.class` or `bus.class=isa8` skips the slave controller (channels 5-7) with `skipped_no_slave` status. Per-channel pass/fail + summary verdict. Pure summary kernel `diag_dma_summary_verdict` host-tested across 10 scenarios.

**Status: 6 of 6 subsystems covered (ALU, memory, FPU, video, cache, DMA). v0.3 complete pending real-hardware gate on BEK-V409.**

### Benchmarks

- CPU integer benchmark (`9b758ed`) — fixed instruction mix, PIT-C2-timed, MIPS-equivalent iters/sec output.
- Memory bandwidth benchmark (`b5ca6e0`) — REP STOSW (write), REP MOVSW (copy), REP LODSB (read — replaced the volatile-checksum approach post-real-iron in `eeba319` / `7e4bdcb`). `kb_per_sec` rewritten for microsecond-precision scaling after the 486 surfaced the sub-ms elapsed-time truncation that clamped every fast operation to the same bogus 4 MB/s.
- FPU benchmark (`6b67cc5`) — x87 instruction mix + DGROUP fix.
- Calibrated multi-pass mode for `bench_cpu` (`1774aa9`) — feeds thermal-stability tracker with per-pass timings.
- `total_ops=0` silent-display corruption (`b6c179b`), then systematic V_U32-display-buffer dangling-pointer class fix applied to all six bench_memory statics (`6c3a023`).

**Status: 3 of 5 subsystems covered (integer, memory, FPU). Cache-bandwidth + video-throughput deferred.**

### Historical benchmarks

- Dhrystone 2.1 port (`e897c15`) — Weicker's reference workload ported to Watcom medium model with full original structure (`Ptr_Comp` linked list, `Record` variant union, `Arr_1_Glob` / `Arr_2_Glob` global arrays). Emits `bench.cpu.dhrystones`, indexed off a PC-XT baseline so PC-class machines get a recognizable relative-rating number. v0.4 plan landed alongside.
- Whetstone port (`525f65b`) — the 1970s floating-point synthetic, eight modules (Module 1 through 8), FPU-presence gate so machines without an FPU emit `bench.fpu.whetstone_status=skipped_no_fpu` without attempting any x87 instructions. Emits `bench.fpu.k_whetstones`.
- Anti-DCE guards for both Dhrystone and Whetstone (`97d24e6`) — Watcom `-ox` was eliminating the synthetic loops that produced no externally-observable output. Fix: volatile globals plus an end-of-run checksum emitted via `report_add_u32`, whose call into report.c (separate TU) prevents Watcom from proving the chain unused.
- NULL-display pattern for Whetstone V_U32 emits (`8552c6d`) — Whetstone uses the same dangling-pointer-safe static-buffer pattern the bench_memory rework established in `6c3a023`.
- Makefile `CFLAGS_NOOPT -od` pinned on `bench_dhrystone` and `bench_whetstone` (`1788561`) — the two benchmarks are the only objects built unoptimized; opt-level drift on those files would silently change the synthetic numbers.

### Consistency engine

Engine + first four rules (`fac1500`). Alert-box UI for WARN/FAIL renders (`7b1a9b0`). Rules 3 + 9 (`4a9f24e`). Methodology documentation (`bb760c8`). Thermal stability — Mann-Kendall α=0.05 (`d5e7400`). Rule 4a PIT/BIOS timing independence (`111347a`). Rule 4b `cpu_ipc_bench` (`7e4bdcb`). Rule 7 `audio_mixer_chip` (`c22e886`). Rule 10 `whetstone_fpu_consistency` (`f0cebde`) — `bench_whetstone` completion state agrees with `detect_fpu` report, catching detect under-reporting of socketed FPUs.

**Ten rules live** (1, 2, 3, 4a, 4b, 5, 6, 7, 9, 10). Rule 8 (`cache_stride_vs_cpuid_leaf2`) reserved pending Phase 3 cache work; slot preserved even with Rule 10 numerically above it.

### Infrastructure

- Crash-recovery breadcrumb (`crumb_enter` / `crumb_exit`) wired into WRAP macros for every detect / diag / bench probe (`ae1cfd9`), so a hang during a probe leaves a named trail that the next boot surfaces with a NOTICE + `/SKIP:<name>` suggestion.
- `/SKIP:TIMING` escape hatch for PIT-C2 probe on boards where the 8254 clone hangs on touching channel 2 (`5fdf7fa`), with the crumb-enter/exit pair so a mid-probe hang is surfaced on next boot.
- `unknown_finalize` reordered pre-UI (`7da102e`) so `CERBERUS.UNK` lands on disk even if the UI path hangs.
- `/NOUI` escape hatch (`7da102e`) — user-visible workaround for the UI-render hang observed on v5 real-iron runs. Retained documented as a debug flag, not a feature-level user workaround.
- Real-hardware run corpus archived (`98c07d5`) — six diffable INI captures at `tests/captures/486-real-2026-04-18/` with per-run narrative README, anchoring every real-iron fix in observable artifacts rather than assertions.
- "Why real hardware" section in CERBERUS.md (`0161e99`) — H2 under Status, names the five 2026-04-18 bugs with symptom / cause / commit, closes with non-negotiable real-hardware-gate statement.
- Adversarial quality gates applied at Phase 4 completion (`6686574`, 5 rounds) and post-real-iron (`4d28e8e`, round-2) catching the phantom-verify biased-baseline and sub-crumb lifecycle bugs before the bench-box validation.
- `tests/target/` scaffold (`0e6c7e3`) for Phase 1 real-hardware validation drops.
- Host-side test suite: 138 assertions across timing (65), consistency (37), thermal (15), diag_fpu (21). Test-expectation drift in timing tracked as [#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1).

### Documentation

- README rewritten in plain-status voice (`dee1c64`).
- README refreshed at Phase 4 completion (`2882388`); current refresh landing with this RC.
- `docs/consistency-rules.md` methodology (`bb760c8`).
- `docs/plans/2026-04-16-cerberus-end-to-end.md` — single-file end-to-end implementation plan covering v0.1 → v1.0 with phase-level architecture and quality-gate criteria.

### Known issues

- **[#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1)** — `test_timing` has 4 pre-existing failures after the PIT wrap-range rework in `b6c179b` / `6c3a023`. Test expectations drifted from behavior. Gated behind the Rule 4a UMC491 8254 phantom-wrap deep-dive, which is out-of-scope for v0.2-rc1. Other host suites (consist 37/0, thermal 15/0, diag_fpu 21/0) are clean.
- **[#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2)** — intermittent OPL detection on Vibra 16 PnP. Same binary, same box, different boot produces `opl=opl3` vs `opl=none`. Partial fix in `eeba319`; residual state-dependence remains. INI still complete on the `opl=none` path (`audio.sb_present=yes`, `sb_dsp_version=4.13`); downstream T-key lookup falls back to the raw composite key.
- **UI hang on real iron.** Observed once on v5 (`7e4bdcb`) without `/NOUI` (2026-04-18 afternoon): after `ui_render_consistency_alerts` paints, the program did not return to DOS. The 2026-04-18 evening session ran the baseline (`7da102e`, no instrumentation) and two builds with exit-path instrumentation; all three exited cleanly on the same 486 box. The reproduction regime is not active on the current state. State variable causing the drift is unidentified — candidates include CMOS drift, cold-vs-warm-boot residue, Vibra PnP init ordering. Instrumentation patch preserved as local `git stash` for re-application if reproduction returns. Tracking issue filed with reopen criterion "reproduction on real iron." `/NOUI` retained as a user-visible escape hatch. Full investigation arc: [`docs/sessions/SESSION_REPORT_2026-04-18-evening.md`](docs/sessions/SESSION_REPORT_2026-04-18-evening.md).
- **Whetstone K-Whetstones under-reports by ~100× vs CheckIt 3.0 reference.** Real-iron validation on the BEK-V409 i486DX-2 showed CERBERUS Whetstone at ~109 K-Whet against CheckIt's 11,420 reference. Dhrystone on the same hardware hits CheckIt within ±2.4% (32,810 vs 33,609). Root cause (documented in detail at [`docs/plans/checkit-comparison.md` §6](docs/plans/checkit-comparison.md)): the `volatile double E1[4]` accumulator array's memory traffic dominates per-unit time at any non-aggressive Watcom optimization level where volatile is honored; no compiler-flag combination reaches CheckIt's reference speed without reintroducing the v7/v8 DCE pattern that over-reports by 30×. The emit is CONF_LOW to mark it as NOT cross-tool-comparable. The measurement IS reproducible cross-run on the same machine, so it remains useful for same-machine thermal tracking + TSR-contention signal (Rule 10 logic is scale-invariant and continues to work). Tracked for v0.4 FPU-assembly rework where the Whetstone inner kernels move to NASM with register-resident x87 state.

## [v0.1.1-scaffold] — 2026-03-20

Scaffold tag covering Phase 0: source-tree layout, Makefile, timing subsystem, CPU detection stub, display abstraction, INI reporter, BIOS info, and host-test infrastructure. First end-to-end CERBERUS.EXE that produces a valid (if mostly-stub) INI on DOSBox-X.
