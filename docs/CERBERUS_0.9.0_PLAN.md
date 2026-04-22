# CERBERUS 0.9.0 Plan

Drafted 2026-04-22 after `v0.8.1` tag. Supersedes the "deferred to 0.9.0" items scattered across the 0.8.0 release notes, 0.8.1 release notes, and `project_cerberus_0_9_0_hw_id_roadmap.md` in the auto-memory.

## 1. Identity

**0.9.0 is the community activation release.** The three main threads that converge: the upload pipeline lights up (stack-safe offline fallback + live `barelybooting.com`), the hardware-identification reach deepens (BIOS and expansion ROM fingerprinting, network-card detection, MAC OUI lookup, UART and peripheral probes), and the cache-depth story completes (128 KB / 256 KB L2 sweeps via huge-pointer arithmetic, Whetstone retirement via per-instruction microbenchmarks).

"Community activation" because the pieces are individually moderate but together unlock the feedback loop the project was designed around: submitters on arbitrary vintage hardware upload INIs with rich identity hashes, the server aggregates them, the hash-to-machine mappings accrue, and the next submitter's hardware identifies itself by matching. That loop is dark in 0.8.x. 0.9.0 turns it on.

## 2. Governing principles (carried from 0.8.x)

The three principles the 0.8.0 plan set stay in force:

1. **Trust beats reach.** Every shipped feature lands with real-iron validation on at least two distinct hardware generations before the 0.9.0 tag. No "it compiles so we ship it." 0.8.1's `/CSV`-flag smoketest regression is the local cautionary tale: host tests plus `wmake rc=0` is not sufficient proof. The durable smoketest-after-every-build rule is in force.
2. **Near-data is a budget.** DGROUP sits at 61,824 / 62,000 soft target with `STATUS: AT RISK` yellow at v0.8.1 tag. 0.9.0 scope is significantly larger than 0.8.1's. A DGROUP reclaim pass is a **prerequisite** for the expansion work, documented as M1.1 below.
3. **Cuts stay cut unless their reason is removed.** Whetstone emit stays suppressed until per-instruction microbenchmarks replace it (M5). Upload stays compiled out until the stack-overflow failure mode is eliminated and `barelybooting.com` is live (also M5). The cut items are not re-enabled by calendar pressure; they are re-enabled by the conditions that justified cutting being demonstrably gone.

## 3. What is deferred into 0.9.0

### From v0.8.0 release notes (explicitly tagged 0.9.0)

- Per-instruction FPU microbenchmarks (research gap N). Whetstone replacement.
- Upload path revival. Contingent on `barelybooting.com` going live + stack-safe offline fallback.
- Full CUA shell (menus, dropdowns, modal dialogs). 0.8.x landed CUA-lite (F-key legend + F1 help + F3 exit + `/MONO`); the full window chrome is 0.9.0.
- Disk throughput via INT 13h raw sector reads.
- 8088 / XT real-hardware capture. Until then the README "Validated on 386 and 486" claim holds.

### From v0.8.1 scope-cut

- L2 sweep at 128 KB / 256 KB working sets. Requires huge-pointer `stride_read_huge()` + `halloc`-backed buffers.
- Improved L2 inference from multiple-point sweeps above the L1 boundary.

### From the 2026-04-22 hardware-ID roadmap recap

Filed in `project_cerberus_0_9_0_hw_id_roadmap.md` in the auto-memory. The full scope:

- **BIOS ROM 64 KB hash** from `F000:0000`. Date and machine-model byte already extracted in v0.8.x; the SHA-1 hash of the full 64 KB is the unique-revision fingerprint that gains real value once the server aggregates submissions.
- **Video BIOS hash** from `C000:0000` (typically 32 KB starting with `55 AA`). Orthogonal to the chipset string-match and port-probe work already in place.
- **Expansion card ROM walk** from `C800:0000` through `DFFF:FFFF`, scanning at 2 KB alignment for `55 AA` signatures. Extract strings and hashes from each hit.
- **Sound card I/O gaps**: GUS at `0x340` RAM test, MPU-401 at `0x330` status register. Most of the SB/AdLib work is already done in 0.7.x.
- **Network card detection**: 3C509 ID port protocol at `0x110` (EEPROM data non-destructive), NE2000 DP8390 signature at base+0x00, WD/SMC checksum byte at base+0x07, packet-driver query for interface type and MAC.
- **MAC OUI lookup**: first three bytes of MAC → manufacturer per IEEE public registry. 50-entry `__far const` table at ~500 bytes FAR data (zero DGROUP cost).
- **Serial UART FIFO type**: BIOS data area at `0040:0000` lists ports; FIFO probe distinguishes 8250 (no FIFO), 16550A (16-byte FIFO), 16750 (64-byte FIFO). Scratch register at base+7 separates 8250 from 8250A.
- **Parallel port ECP/EPP** via BIOS data area at `0040:0008` with mode detection at base+0x402.
- **Game port** at `0x201` (upper nibble read).
- **RTC presence** via CMOS port `0x70/0x71` register `0x0A` (absent on XT-class).

### From 0.8.1 hardware-gated carry-ins (folded into 0.9.0 M1 rather than a separate 0.8.2)

- **M4 BEK-V409 BSS overwrite root-cause**. Defect localized to three suspected probe paths (S3 Trio64, Vibra 16S / OPL fallback, UMC491 PIT). Removal-at-a-time protocol documented in `docs/methodology.md`.
- **M3.1 IIT 3C87 real discriminator**. DB row and routing are in place; the actual FNSAVE-or-opcode signature needs a 386 DX-40 + IIT 3C87 capture.
- **M3.2 Genoa ET4000 probe validation**. The 3CDh read-write-readback algorithm is documented; real-iron confirmation awaits the 386 DX-40 + Genoa capture.

### Deferred infrastructure

- **DGROUP reclaim via keys.h**. Watcom does not cross-TU dedup string literals. Top duplicated keys (`fpu.detected` × 12, `cpu.detected` × 12, `cpu.class` × 8, `bus.class` × 10, etc.) add up to ~1-2 KB of CONST that a centralized `keys.h` header would reclaim. Prerequisite for the expansion work.
- **Consistency Rule 4b activation**. Blocked on per-class IPC anchors in `cpu_db`. Adding `class_ipc_low_q16` / `class_ipc_high_q16` columns to `hw_db/cpus.csv` unblocks the rule. Small DGROUP cost; practical high-value rule (catches thermal throttle, TSR overhead, cache disabled in BIOS).

### Deferred out of 0.9.0 (stays 1.0.0+)

- **Address-to-chip QARAM-style physical translator.** Needs board-map data format research; out of scope.
- **Dashboard-default landing screen** (the CheckIt "launch with SysInfo populated" pattern). Depends on CUA shell (M5); if CUA shell lands cleanly it rides with M5, otherwise 1.0.0.

## 4. Milestone structure

Seven milestones, ordered by dependency. M1 is a prerequisite for everything else; M2 / M3 expand hardware-ID reach; M4 closes cache depth; M5 is the big user-visible one (upload revival + CUA shell); M6 replaces Whetstone; M7 is release prep.

### M1 — Foundations (DGROUP reclaim + 0.8.2 carry-ins)

**M1.1 DGROUP reclaim via keys.h centralization.**
- New `src/core/keys.h` with `#define KEY_FPU_DETECTED "fpu.detected"` etc. for the top-20 duplicated literals.
- Sweep `src/**/*.c` replacing literals with the defines. Single source in CONST per key.
- Target: recover at least 1,500 bytes DGROUP headroom before any expansion code lands. Blocks M2 start.
- Post-reclaim baseline: `tools/dgroup_check.py` status must be `OK` not `AT RISK`.

**M1.2 M4 BEK-V409 BSS overwrite root-cause.**
- Hardware-gated. Tony builds instrumented binary with crumb markers isolating S3 Trio64 / Vibra 16S OPL-fallback / UMC491 PIT paths. Runs on BEK-V409, cold-reboots, compares `CERBERUS.LAS` trails across crash-vs-clean runs.
- Once the offending probe is identified, single-step through it in the DOS debugger; pinpoint the instruction writing near DGROUP:0.
- Fix is either a pointer-type error, buffer-bound error, or segment-prefix error. Document in `docs/methodology.md`, remove the `*** NULL assignment detected` canary fire on BEK-V409.

**M1.3 M3.1 IIT 3C87 real discriminator.**
- Hardware-gated. Tony captures FNSAVE dump output from 386 DX-40 + IIT 3C87; compare against reference Intel 80387 dump to find the discrimination byte pattern.
- Replace the `fpu_probe_iit_3c87()` stub with the actual signature check.
- Validate: INI from 386 DX-40 emits `fpu.vendor=IIT`, `fpu.friendly=IIT 3C87`.

**M1.4 M3.2 Genoa ET4000 probe validation.**
- Hardware-gated. Tony runs 0.8.1 binary on 386 DX-40 + Genoa ET4000; observe whether `probe_et4000_chipid()` fires correctly.
- If yes: no code change, mark validated.
- If no: refine threshold (require both patterns to round-trip identically) or add an alternative Tseng-specific register probe.

**M1.5 quality gate + commit.**

**M1 exit criteria:**
- DGROUP status `OK` not `AT RISK`. Headroom >= 2,500 bytes vs 62 KB soft target.
- BEK-V409 runs 10 consecutive times without the NULL-assignment canary firing.
- 386 DX-40 capture emits `fpu.vendor=IIT` and `video.chipset=ET4000`.
- Host tests still green across all 12 suites.
- Smoketest passes.

### M2 — Hardware identification expansion

**M2.1 BIOS ROM fingerprint** (~400 bytes DGROUP).
- New `src/detect/bios_ext.c` with `bios_rom_hash()` returning SHA-1 of `F000:0000` through `F000:FFFF`. Emit `bios.rom_hash` at CONF_HIGH.
- SHA-1 already linked for the run signature; cost is just the 64 KB read + hex emit.
- Also add manufacturer string scan if not already present: "American Megatrends", "Award", "Phoenix" substring search across the 64 KB.

**M2.2 Video BIOS fingerprint** (~200 bytes DGROUP).
- Extend `detect_video` to hash 32 KB from `C000:0000` (stops at first `0xAA 0x55` followed by zero-tail to avoid reading past end). Emit `video.rom_hash`.
- Orthogonal to the existing text-scan + chip-ID probes; adds identity fidelity on cards with shared BIOS-text but different ROM patches.

**M2.3 Expansion ROM walk** (~400 bytes DGROUP).
- New `src/detect/expansion_rom.c`. Scan `C800:0000` through `DFFF:FFFF` at 2 KB alignment for `55 AA`. For each hit, read the third byte (size in 512-byte units), extract any embedded string, compute a short hash.
- Emit per-hit rows: `expansion_rom.<seg>.signature=hex`, `expansion_rom.<seg>.string=<ascii>`.

**M2.4 Serial UART FIFO probe** (~200 bytes DGROUP).
- Read COM port list from BDA at `0040:0000`. For each port, probe the FIFO control register at base+2: no-FIFO → 8250; 16-byte FIFO → 16550A; 64-byte FIFO → 16750.
- Scratch register at base+7 separates 8250 from 8250A.
- Emit `serial.com1.uart=8250|8250a|16550a|16750|absent` etc.

**M2.5 Parallel ECP/EPP + game port + RTC** (~300 bytes DGROUP).
- LPT list from BDA at `0040:0008`. For each port, ECP/EPP mode detection at base+0x402.
- Game port: `inp(0x201)` upper nibble indicates presence.
- RTC: write 0x0A to port `0x70`, read `0x71`; non-0xFF value indicates CMOS/RTC present (absent on XT-class).
- Emit `parallel.lpt1.mode`, `peripherals.game_port`, `peripherals.rtc`.

**M2.6 Missing sound card probes** (~100 bytes DGROUP).
- GUS at `0x340` RAM test (write-readback pattern).
- MPU-401 at `0x330` status register.
- Emit `audio.gus_present`, `audio.mpu401_present`.

**M2.7 quality gate + real-iron validation.**

**M2 exit criteria:**
- BEK-V409 and 386 DX-40 captures emit the full set of new keys with plausible values.
- Host tests cover the pure-inference helpers (UART FIFO classifier, ECP/EPP mode detector).
- DGROUP delta within 1,600 bytes. Post-M2 headroom >= 900 bytes.

### M3 — Network stack + OUI lookup

**M3.1 NIC detection probes** (~500 bytes DGROUP).
- 3C509: ID port protocol at `0x110` reads EEPROM non-destructively. Extract MAC, product ID, I/O base.
- NE2000: DP8390 register signature at base+0x00. Default base `0x300`.
- WD / SMC: checksum byte at base+0x07 sums with prior seven to 0xFF. Default base `0x280`.
- Emit `network.nic.type=3c509|ne2000|wd_smc|absent`, `network.nic.base`, `network.nic.mac`.

**M3.2 Packet driver query** (~200 bytes DGROUP).
- INT 60h-7Fh scan for packet driver signature. On hit, query for interface type and MAC via documented API (`access_type` + `get_address` functions).
- Emit `network.packet_driver.vector`, `network.packet_driver.iftype`, `network.packet_driver.mac`.

**M3.3 MAC OUI lookup** (~300 bytes DGROUP for key strings, ~500 bytes FAR for table).
- New `src/detect/oui_db.c` with 50-entry `__far const` OUI table covering common vintage NIC manufacturers (3Com, Western Digital, SMC, Novell, Intel, etc.).
- `lookup_oui_vendor(mac[3])` returns vendor string or "unknown".
- Emit `network.nic.oui_vendor`.

**M3.4 quality gate + commit.**

**M3 exit criteria:**
- Real-iron capture from the 3C509-equipped 486 emits `network.nic.type=3c509` with correct MAC and OUI vendor.
- Host tests cover OUI lookup + WD/SMC checksum classifier.
- DGROUP delta within 1,000 bytes.

### M4 — Cache depth completion + consistency Rule 4b

**M4.1 Huge-pointer stride_read** (~200 bytes DGROUP).
- New `stride_read_huge()` in `bench_cache_char.c` taking `unsigned char __huge *`. Handles segment-boundary crossing via Watcom's transparent normalization.

**M4.2 L2 sweep at 128 / 256 KB** (~200 bytes DGROUP).
- `halloc(131072L, 1)` and `halloc(262144L, 1)` for the two buffer sizes. Free after use.
- Emit `bench.cache.char.size_128kb_kbps`, `bench.cache.char.size_256kb_kbps`.
- Update `bench_cc_infer_l2_size()` to use 32 / 64 / 128 / 256 KB plateau analysis.
- `bench.cache.char.l2_size_kb` now meaningful.

**M4.3 DRAM ns re-derivation** (~0 bytes DGROUP).
- Use the largest successful sweep size (256 KB, confidently past any L2) as the DRAM-dominant rate.
- Upgrade `bench.cache.char.dram_ns` confidence from MEDIUM to HIGH when 256 KB measurement is available.

**M4.4 Consistency Rule 4b activation** (~400 bytes DGROUP).
- Add `class_ipc_low_q16` / `class_ipc_high_q16` columns to `hw_db/cpus.csv` for every CPU DB entry.
- Regenerate `cpu_db.c`.
- Implement Rule 4b in `src/core/consist.c`: compare `bench.cpu.iters_per_sec` against `cpu.clock_mhz * class_ipc_range` from DB.
- Possible-causes narration per the 0.8.0 M4.1 pattern.

**M4.5 quality gate + real-iron validation.**

**M4 exit criteria:**
- BEK-V409 capture emits `size_128kb_kbps`, `size_256kb_kbps`, `l2_size_kb` = 0 (486 DX-2-66 has no L2), `dram_ns` at HIGH confidence.
- Pentium-class capture (if available) emits `l2_size_kb` = 256 or 512.
- Rule 4b activates with non-trivial output on a capture with known thermal / TSR interference.

### M5 — Upload revival + full CUA shell

**M5.1 Stack-safe offline fallback** (~100 bytes DGROUP).
- Audit `upload.c` for large-on-stack buffers that contributed to the v0.7.1 stack overflow when `barelybooting.com` was unreachable. Move big buffers to static or FAR pool.
- Bump the linker `STACK = 4096` directive to 8192 as belt-and-suspenders.
- Add a healthcheck circuit breaker: if the HTGET handshake doesn't return within N seconds, abort with `upload.status=server_unreachable` rather than looping.

**M5.2 Upload re-enable** (~200 bytes DGROUP).
- Change the stock build to include upload (`CERBERUS_UPLOAD_ENABLED` becomes the default).
- `wmake NO_UPLOAD=1` inverts for users who want a stripped build.
- `/NOUPLOAD` flag behavior unchanged.

**M5.3 Full CUA shell** (~2,000 bytes DGROUP).
- New `src/core/menu.c` with a Turbo-Vision-style menu bar (File / View / Tests / Help).
- Dropdown rendering with keyboard navigation + highlighted hotkey letters.
- Modal dialog primitives: `dialog_confirm`, `dialog_message`, `dialog_input`.
- Dashboard-default landing screen (the CheckIt SysInfo pattern): on startup with no flags, CERBERUS populates + renders the three-heads summary immediately, with menu bar for follow-up actions.

**M5.4 quality gate + real-iron validation.**

**M5 exit criteria:**
- Upload round-trip succeeds on a 486 DX-2-66 with packet driver + mTCP against live `barelybooting.com`.
- CUA shell renders cleanly on MDA / CGA / EGA / VGA tiers.
- Dashboard-default landing: first-time user sees populated data within 10 seconds of `CERBERUS` invocation.

### M6 — FPU per-instruction microbenchmarks (Whetstone retirement)

**M6.1 Per-instruction kernel** (~400 bytes DGROUP).
- New `src/bench/bench_fpu_insn.c` + `bench_fpu_insn_a.asm`.
- Per-instruction loops: FADD / FSUB / FMUL / FDIV / FSQRT / FSIN / FCOS, each measuring cycles-per-instruction via RDTSC (or PIT-C2 fallback on pre-Pentium).
- Emit `bench.fpu.fadd_cycles`, `bench.fpu.fmul_cycles`, etc.

**M6.2 Whetstone retirement** (~0 bytes DGROUP).
- Remove `bench_whet_fpu.asm` + `bench_whetstone.c` compilation from the stock Makefile.
- Keep the files in-tree for archaeological reference; add a header comment pointing to `bench_fpu_insn.c` as the replacement.

**M6.3 Methodology doc update.**
- `docs/methodology.md` "Why Whetstone is not in 0.8.0" section gets a post-script: "Replaced in 0.9.0 by per-instruction microbenchmarks. See bench.fpu.<op>_cycles keys."

**M6 exit criteria:**
- BEK-V409 capture emits plausible cycles-per-instruction for each of the six x87 ops (FADD ~3-5 cycles on 486, ~1-3 on Pentium, etc.).
- `bench.fpu.whetstone_status=retired` as an archival marker.

### M7 — Disk throughput + 8088/XT validation + release prep

**M7.1 Disk throughput via INT 13h** (~300 bytes DGROUP).
- New `src/bench/bench_disk.c`. Sequential read of N cylinders starting at track 0, head 0. Time aggregate read, compute KB/s.
- Safety: read-only (AH=02h only, never AH=03h write). Skip if drive count from BDA is 0.
- Emit `bench.disk.seq_read_kbps`.

**M7.2 8088 / XT real-hardware capture.**
- Hardware-gated. Tony boots an 8088 or XT-clone (if available) and runs CERBERUS. Full INI capture archived under `tests/captures/8088-real-<date>-m7/`.
- Update README: upgrade "Validated on 386 and 486" to "Validated on 8088, 386, and 486."

**M7.3 Release prep.**
- CHANGELOG.md entries for 0.8.0, 0.8.1, 0.9.0 (if not already current).
- `docs/releases/v0.9.0.md` release notes.
- CERBERUS.md + README.md refresh to include all 0.9.0 new keys.
- methodology.md extended with per-milestone methodology sections.

**M7.4 Quality gate + tag + push + site update.**

**M7 exit criteria:**
- 8088/XT capture archived (or the README claim stays as-is with documented reason).
- `v0.9.0` tag on main with full release notes.
- `barelybooting.com` site updated with v0.9.0 entry.

## 5. DGROUP budget gate

Current v0.8.1 headroom: 1,664 bytes vs 62 KB soft target, 3,712 bytes vs 64 KB hard ceiling. `STATUS: AT RISK`.

**Per-milestone projected DGROUP cost (before M1.1 reclaim):**
- M1.2-1.4 (0.8.2 carry-ins): ~100 bytes
- M2 (HW-ID expansion): ~1,600 bytes
- M3 (Network): ~1,000 bytes
- M4 (Cache + Rule 4b): ~800 bytes
- M5 (Upload + CUA shell): ~2,300 bytes
- M6 (FPU microbench): ~400 bytes
- M7 (Disk + release): ~300 bytes

**Total projected: ~6,500 bytes.** Current headroom: 1,664 bytes. **Deficit without M1.1 reclaim: ~4,800 bytes.**

**M1.1 reclaim target: at least 2,500 bytes.** This plus M1.2-1.4's minor 100-byte delta leaves ~4,000 bytes headroom before M2 starts; each subsequent milestone stays within its budget and we end 0.9.0 around 1,000-1,500 bytes headroom.

If M1.1 comes in under target, the plan scope contracts in this order: drop M7.1 disk throughput → defer CUA shell modal dialogs to 1.0.0 (keep menu bar + dropdowns only) → defer M2.3 expansion ROM walk → defer M3.3 OUI table.

## 6. Real-hardware validation matrix

| Milestone | Minimum hardware | Covers |
|---|---|---|
| M1 foundations | BEK-V409 (486 DX-2-66) + 386 DX-40 + IIT 3C87 + Genoa ET4000 | NULL-assignment bug, IIT signature, ET4000 confirmation |
| M2 HW-ID expansion | Any two of 8088/XT / 286 / 386 / 486 / Pentium | BIOS hash diversity, UART types, expansion ROM coverage |
| M3 Network | 486 with 3C509 (on-hand) + any NE2000-class second NIC if available | Live NIC detection + MAC OUI |
| M4 Cache depth | BEK-V409 (no L2) + Pentium 133 or 200 MMX if available | L2 presence and absence contrast, DRAM ns at HIGH confidence |
| M5 Upload + CUA | 486 with packet driver + mTCP + live `barelybooting.com` | End-to-end POST round-trip |
| M6 FPU microbench | 8087 (if available) + 486 DX-2-66 + Pentium if available | Per-instruction cycles across x87 generations |
| M7 Disk | BEK-V409 (DOS 6.22 on FAT16) | Sequential read baseline; 8088 / XT validation if hardware available |

## 7. Ordered execution

1. Plan approval + task creation.
2. **M1.1 DGROUP reclaim** (autonomous, this is the gate). Drops status from AT RISK to OK.
3. **M1.2/M1.3/M1.4** (hardware-gated bench session). Tony + bench boxes; ~half-day.
4. **M2 hardware-ID expansion** (autonomous). Largest single chunk; ~6-8 hours of code work.
5. **M3 Network stack** (autonomous for code + validation via 3C509 real-iron run).
6. **M4 Cache + Rule 4b** (autonomous code + real-iron capture for Rule 4b narration).
7. **M5 Upload + CUA shell** (autonomous; final real-iron session for upload round-trip). This is the big user-visible pass.
8. **M6 FPU microbench** (autonomous code + real-iron calibration).
9. **M7 Disk + 8088/XT + release prep**. Final session.
10. Tag `v0.9.0`, push, site update.

## 8. What 0.9.0 explicitly does not ship

- **Address-to-chip QARAM-style translator**. Stays 1.0.0+. Needs board-map data format research.
- **Pentium-specific consistency rules**. Requires Pentium-class real-hardware validation that's out of the current hardware envelope.
- **TUI redraw optimization** (partial-update renderer). CUA shell in M5 will do full-screen redraws; incremental is 1.0.0 polish.
- **Per-instruction microbench on 8087 / 287**. Pre-387 FPUs lack most of the instructions M6 measures; coverage on those generations is the behavioral fingerprint, not the per-instruction bench.
- **Multi-threaded anything**. DOS, single-threaded; does not apply.

The 0.9.0 envelope is large but bounded: lighting up the community feedback loop, deepening hardware-identification reach to match upload value, and completing the cache depth + FPU benchmark stories that 0.8.x deferred. Once 0.9.0 tags, CERBERUS has covered its original three-heads charter at shipping quality across detection, diagnosis, and benchmark, plus the upload pipeline the project was designed around.
