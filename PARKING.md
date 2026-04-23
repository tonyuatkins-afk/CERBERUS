# CERBERUS parked state — 2026-04-23

Active development paused on 2026-04-23 so the maintainer (Tony Atkins) can focus on the NetISA project (all hardware delivered that day). CERBERUS is at a clean stopping point: `v0.8.1` tagged, `v0.9.0` planned in full detail, nothing half-landed on `main`. Everything below is durable. Come back when you want — the state waits.

This document is the single catch-up read for a future session (or a future contributor). If anything below is stale, the commit history on `main` supersedes it.

## Current tagged state

**v0.8.1** on `main` at commit `bdfe95c`, tagged 2026-04-22. Trust-first release arc completed across four milestones:

- **v0.8.0** (earlier same day): trust-first cuts. Whetstone emit suppressed, upload compiled out, end-of-run `_exit()` bypass for Watcom libc teardown, cpu.class family tokens, DGROUP audit tooling, BEK-V409 + 386 DX-40 real-iron validation corpus.
- **v0.8.1**: completion release. IEEE-754 edge-case diagnostic, `/CSV` output mode, L1 pointer-chase latency probe, 64 KB L2 reach, DRAM ns derivation, IIT 3C87 DB row + routing stub, Genoa ET4000 chip-level probe, Hercules variant discrimination, plus the `/CSV` flag-parsing hotfix and the `CERBERUS_VERSION` bump caught by post-tag smoketest.

**Build state at park:**
- `CERBERUS.EXE` stock: 170,722 bytes (commit `bdfe95c`)
- DGROUP: 61,824 / 62,000 soft target (AT RISK yellow, 1,664 bytes headroom)
- Host tests: 376 assertions across 12 suites, 0 failures
- Zero compiler warnings
- DOSBox Staging smoketest clean per `devenv/smoketest-0-8-1.conf`

## Durable assets

### Code + binaries

- `dist/CERBERUS.EXE` — v0.8.1 stock build
- `dist-investigation/CERBERUS.EXE` — *(gitignored)* scratch-branch binary with crumb markers for BEK-V409 NULL-write isolation. Rebuild from `investigation/bek-v409-null-write` branch when needed; `wmake` then `cp CERBERUS.EXE dist-investigation/` reproduces it.

### Bench kit

- `bench-floppy/offline/` — no-network 720-KB-compatible floppy content
- `bench-floppy/network/` — 720 KB / 1.44 MB turnkey kit with bundled 6 Crynwr packet drivers + 5 mTCP binaries (DHCP/FTP/HTGET/SNTP/PING). License-compliant (GPL v3 attribution for mTCP).
- `docs/bench-session-playbook.md` — eight-session protocol covering 486 × 2 + 386 + 286 + Leading Edge XT (V20/8088/8087 swap matrix). Calendar-agnostic; works whenever bench time lands.

### Documentation

- `docs/releases/v0.8.0.md` + `docs/releases/v0.8.1.md` — per-tag release notes
- `docs/quality-gates/*.md` — per-milestone gate records
- `docs/CERBERUS_0.8.0_PLAN.md`, `docs/CERBERUS_0.8.1_PLAN.md`, `docs/CERBERUS_0.9.0_PLAN.md` — release plans including M1.1 DGROUP-reclaim prototype finding
- `docs/methodology.md`, `docs/consistency-rules.md`, `docs/ini-format.md`, `docs/ini-upload-contract.md` — reference docs current at v0.8.1
- `docs/screenshots/` — five canonical DOSBox-captured screenshots + README + reproducible capture harness (`tools/capture_screenshots.ps1`, `tools/capture_help.ps1`)
- `CHANGELOG.md` — current through v0.8.1

### Branches

- `main` — production (tagged `v0.8.1`)
- `investigation/bek-v409-null-write` — scratch branch with selective-skip + crumb instrumentation for the BEK-V409 BSS-overwrite bug. **Do not delete.** Pushed to `origin`. Re-mergeable when root cause is identified, OR kept standalone for the instrumentation as a diagnostic tool.

## Open work — hardware-gated

These are the only items that cannot advance without bench-box access. They're documented; when a bench evening happens, the playbook points at them:

1. **M4 BEK-V409 BSS overwrite root-cause** — `*** NULL assignment detected` canary fires on BEK-V409 (486 DX-2-66 + AMI 11/11/92 + DOS 6.22 + HIMEM + EMM386). Does not reproduce on 386 DX-40 or DOSBox-X. Suspected paths: S3 Trio64 CR30 probe, Vibra 16S OPL fallback, UMC491 PIT. Removal-at-a-time protocol documented in `docs/methodology.md`. Instrumented binary staged at `dist-investigation/`.

2. **M3.1 IIT 3C87 real discriminator** — DB row + routing stub in place on `main`. `fpu_probe_iit_3c87()` returns 0 unconditionally. Needs FNSAVE-bytes capture from 386 DX-40 + IIT 3C87 to pick the signature. One-function edit after capture.

3. **M3.2 Genoa ET4000 probe validation** — 3CDh read-write-readback algorithm from Tseng programmer's reference. Real-iron confirmation from 386 DX-40 + Genoa board pending. If probe misfires, tighten the test (require both write patterns to round-trip identically).

4. **8088/XT real-hardware capture** — Leading Edge Model D + 8088-1/8088-2/V20 × with/without 8087 swap matrix. First-ever real 8087 behavioral-fingerprint reference baseline. Requires CMOS battery repair on the 386 first (noted 2026-04-22 bench evening as the blocker that ended the session).

5. **286 board first run** — upgrades README claim from "untested" to "validated" once a capture lands.

## Open work — code-only, ready to resume

These are real `v0.9.0` M1 work with no hardware dependencies:

1. **0.9.0 M1.1 DGROUP reclaim (`__far const` DB migration)** — prototype on `fpu_db` proved the tactic works (+120 bytes reclaimed from a 15-entry table). Full implementation is 4-6 hours of refactor across `report_add_*` signatures, `result_t::v.s` pointer type, and all five DB tables (`cpu_db`, `fpu_db`, `video_db`, `audio_db`, `bios_db`). Scope documented at `docs/CERBERUS_0.9.0_PLAN.md` M1.1 section. Projected ~2,500 bytes total reclaim across the five tables, enough to clear the AT RISK yellow DGROUP status.

2. **0.9.0 M2.1 BIOS ROM hash** — new `src/detect/bios_ext.c` with `bios_rom_hash()` returning SHA-1 over `F000:0000` through `F000:FFFF`. Emits `bios.rom_hash`. SHA-1 code already linked (run signature); cost is just the 64 KB read + hex emit. Pure helper, fully host-testable.

3. **0.9.0 M3.3 MAC OUI lookup** — ~50-entry `__far const` table of IEEE OUIs for common retro NICs, `lookup_oui_vendor(mac[3])` pure function. Host-testable.

Pick any of these on a fresh session without needing bench access.

## Auto-memory references

- `project_cerberus.md` — status memory, updated to reflect parked state
- `project_cerberus_0_9_0_hw_id_roadmap.md` — 0.9.0 hardware-ID expansion scope brief
- `feedback_cerberus_smoketest_after_build.md` — smoketest-after-every-build durable rule
- `feedback_watcom_already_dedups_literals.md` — keys.h dead-end from the 0.9.0 M1.1 prototype pass

## How to resume

1. **Read this file** — if more than a couple of months stale, spot-check against `git log --oneline -20` for recent commits.
2. **Decide**: hardware-gated work (run the playbook) OR code-only work (pick from §Open work - code-only).
3. **Hardware session**: start with `docs/bench-session-playbook.md`. Binaries at `dist/` + `dist-investigation/`. Bench floppy content at `bench-floppy/`. Prerequisite: working CMOS battery on the 386 (see the 2026-04-22 session note — the Varta was desoldered, a CR2032 + 1N4148 diode replacement is the cheap fix).
4. **Code session**: run `wmake && wmake -f tests/host/Makefile run` to confirm a clean baseline. Pick a 0.9.0 M1 task. Follow the smoketest-after-every-build rule.
5. **Update this file** at the start of the resumed work: bump the date, update "Current tagged state" and "Open work" sections, commit.

## One-liner status for the landing README

> **Status: v0.8.1 released 2026-04-22. Development paused 2026-04-23 while the maintainer focuses on NetISA. See [PARKING.md](PARKING.md) for resume notes.**
