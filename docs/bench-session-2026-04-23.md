# Bench Session Playbook: 2026-04-23 (evening)

Pre-staged during the 2026-04-22 autonomous block. Run this from top to bottom; each section says what to FTP, what to run, what to look for, and what the capture answers. Save time by running both boxes in parallel when possible (they're on different static IPs).

## Targets

| Box | Tier | Transport | Notes |
|---|---|---|---|
| **BEK-V409** (486 DX-2-66 VLB) | 486 | FTP 10.69.69.160 (tony/netisa) | AMI 11/11/92 + S3 Trio64 + Vibra 16S + 63 MB XMS. Source of the `*** NULL assignment detected` canary. |
| **IBM 486** (model TBD at bench) | 486 | Path TBD | Second 486 tier sample. First CERBERUS run on this box. |
| **386 DX-40** | 386 | FTP 10.69.69.161 (tony/netisa) | IIT 3C87 + Genoa ET4000 + Aztech ISA. Reference-clean on the NULL canary. |
| **286 board** | 286 | Floppy (presumed) | First CERBERUS run on 286-class hardware. Retracts the README claim that 286 is "untested." |
| **Leading Edge Model D** | XT | Floppy | V20 (NEC) currently. Plus swap matrix: 8088-1 (4.77 MHz), 8088-2 (8 MHz), 8087 (install + remove). First CERBERUS run on XT-class hardware; first validation of FPU behavioral fingerprint against real 8087 silicon. |

FTP workflow reference: `project_netisa_486.md` + `reference_486_ftp_workflow.md` in auto-memory. Quick template:

```
ftp 10.69.69.160
user tony
<password>
lcd C:\Development\CERBERUS\dist
cd /drive_c/cerberus
binary
put CERBERUS.EXE
ls
bye
```

## Binaries pre-staged

- `C:\Development\CERBERUS\dist\CERBERUS.EXE` — stock v0.8.1 (170,722 bytes, commit `bdfe95c` on `main`)
- `C:\Development\CERBERUS\dist-investigation\CERBERUS.EXE` — v0.8.1 + BEK-V409 investigation hooks (170,946 bytes, commit `f49a1d2` on `investigation/bek-v409-null-write`). Adds `/SKIP:s3probe`, `/SKIP:et4000probe`, `/SKIP:biosscan`, `/SKIP:oplfb` selective-skip flags plus crumb markers on each probe site.

## Bench floppy (single-disk, bootable, 1.44 MB across the fleet)

Tony confirmed every bench box supports 1.44 MB floppies — use 1.44 MB across the board as the common format. Kit staged at `C:\Development\CERBERUS\bench-floppy\`:

- **offline** — ~180 KB content. No network. Captures to `A:\CAP.INI` + `A:\CAP.CSV`; pull floppy, read on PC. Five interactive session choices at boot. Use when the box has no NIC, or when you don't want to mess with network config.
- **network** — ~470 KB content, **fully turnkey**: bundled 6 packet drivers (3C509, 3C503, NE2000, NE1000, WD8003E, SMC_WD from Crynwr) + 5 mTCP binaries (DHCP, FTP, HTGET, SNTP, PING from brutman.com, GPL v3). Uncomment one line in `A:\NET\NET.BAT` for your NIC + one DHCP line; boot auto-configures. FTP back the capture when done.

See `bench-floppy/README.md` for floppy-making instructions (Rufus recommended). All six bundled packet drivers cover the common ISA / 16-bit / 8-bit NIC families. If a box has a NIC not in the bundled set, fall back to the offline floppy + remove-by-hand workflow.

**Floppy use in tonight's sessions:**
- Sessions 1, 2, 3, 4 (BEK-V409, 386 DX-40): FTP is already configured; floppy is only backup if FTP breaks.
- Session 5 (IBM 486): network floppy with `3C509.COM` or `NE2000.COM` (most likely candidates).
- Session 6 (286): network if the 286 has a supported NIC; offline otherwise. Run conservative first: `/Q /SKIP:BENCH /NOUI`.
- Sessions 7, 8 (Leading Edge XT): network floppy with `NE1000.COM` or `3C503.COM` if the box has an 8-bit NIC installed; offline otherwise. XT's 5.25" 1.44 MB would need an unusual DD→HD upgrade — if only 5.25" 360 KB is available on this box, use offline and write to 360 KB separately (offline fits).

## Session 1: 0.8.1 validation parade (both boxes, ~20 min each)

**Binary:** stock `dist\CERBERUS.EXE`. Same binary for both boxes.

**Invocation per box:**

```
cd C:\CERBERUS
CERBERUS /NICK:BEK-V409 /NOTE:0.8.1-validation /O:C:\V081.INI /CSV
```

(replace `/NICK:BEK-V409` with `/NICK:386-DX40` on the 386 box.)

**FTP back afterward:**

```
get /drive_c/cerberus/V081.INI C:\Development\CERBERUS\tests\captures\<box>-2026-04-23-v081\capture.ini
get /drive_c/cerberus/V081.CSV C:\Development\CERBERUS\tests\captures\<box>-2026-04-23-v081\capture.csv
```

**What to observe in `V081.INI`:**

| Key | Expected value on BEK-V409 | Expected value on 386 DX-40 | Question answered |
|---|---|---|---|
| `[cerberus] version` | `0.8.1` | `0.8.1` | Correct binary ran (post-CERBERUS_VERSION fix) |
| `diagnose.fpu.edge_cases_ok` | `14_of_14` | `14_of_14` (assumes IIT conformant) | M1.1 IEEE-754 edges on real silicon |
| `bench.cache.char.l1_ns` | ~15-30 | ~50-80 (386 no L1) | M2.1 pointer-chase resolution on real iron |
| `bench.cache.char.size_64kb_kbps` | populated | populated | M2.2 L2 reach probe |
| `bench.cache.char.dram_ns` | populated | populated | M2.3 DRAM derivation |
| `bench.cache.char.l2_status` | `ok` | `ok` or `no_far_mem` | `_fmalloc(64K)` reliability |

**Observe in `V081.CSV`:**
Row count should match the INI's `results=N` line. Schema: `key,value,confidence,verdict` header then one data row per emitted result.

**Questions answered by this session:** every 0.8.1 probe produces real numbers on real hardware, and those numbers are plausible. If any probe is wildly off on real iron versus DOSBox Staging's baseline (DOSBox Staging: `edge_cases_ok=14_of_14`), that's the regression-catch signal.

## Session 2: M3.1 IIT 3C87 signature capture (386 DX-40, ~5 min)

**Goal:** determine the FNSAVE-byte or opcode pattern that discriminates IIT 3C87 from Intel 80387. Currently `fpu_probe_iit_3c87()` stubs to 0; we need a real signature.

**Binary:** stock `dist\CERBERUS.EXE` (the M3.1 hooks are in the detection path; we just observe what the 386+IIT capture emits).

**What's in the existing V081.INI from Session 1 that's relevant:**

```
[fpu]
detected=387          (stub path; we want to confirm this by hand)
friendly=Intel 80387 (SX/DX)  (generic — should say IIT 3C87)
```

**Manual FNSAVE capture** (requires DOS DEBUG or equivalent, or a small custom C program). If you have a DEBUG session going:

```
-a 100
fninit
fnsave [200]
ret
-g=100
-d 200 L70
```

Compare the first 14 bytes (FPU state header) and bytes 14..94 (the 8 x 10-byte registers) between BEK-V409 (Intel i487) and 386 DX-40 (IIT). Key candidate signature: bytes beyond offset 94 in a 108-byte buffer. Per the research, IIT writes matrix-mode state there; Intel leaves it unchanged from the host buffer's initial value.

**Simpler approach if DEBUG is inconvenient:** any known IIT-discriminator program (older CHKCPU, IITID.EXE if available) run against the 386 DX-40 is a reference; transcribe what it reports as the discriminator byte/value.

**What to capture back:** a text note with whatever bytes differ. Paste into `docs/research/iit-3c87-signature.md` on the main branch after the session; that feeds the 0.9.0 M1.3 fpu_probe_iit_3c87() signature.

## Session 3: M3.2 Genoa ET4000 probe validation (386 DX-40, ~3 min)

**Goal:** confirm `probe_et4000_chipid()` fires correctly on the Genoa OEM variant of the ET4000 silicon.

**From Session 1's V081.INI:**

```
[video]
adapter=vga
vendor=???      (what the probe returned)
chipset=???     (should be ET4000 if probe fires)
```

**If `video.vendor=Tseng, video.chipset=ET4000`**: probe works, M3.2 validated, no code change needed.

**If `video.vendor=unknown` or a different chipset**: the 3CDh read-write-readback probe didn't fire on this board. Two paths forward:
1. Tighten the probe — currently requires `rb1==0x55 && rb2==0xAA` round-trip. If the Genoa variant handles 3CDh differently, refine.
2. Fall back to BIOS-ROM scan tweaks — look for Genoa-specific strings in the C000:0000 ROM (easy follow-up).

**What to capture back:** just observe `video.vendor` + `video.chipset` in V081.INI. That's the one-bit answer.

## Session 4: M4 BEK-V409 NULL-write root-cause (BEK-V409, ~40-60 min)

**Binary:** `dist-investigation\CERBERUS.EXE` (scratch-branch build, instrumented). Not the stock 0.8.1 binary.

**Investigation protocol (removal-at-a-time)**:

### Step 4.1: Baseline

Run stock v0.8.1 once more. Confirm the `*** NULL assignment detected` canary still fires at exit. If it doesn't fire on this run, the bug has moved or is non-deterministic; stop and note that — the investigation needs a reliable reproduction.

```
CERBERUS /NICK:BEK-V409 /NOTE:baseline /O:C:\BASELINE.INI
```

Observe: canary fires? Yes/No. Note it.

### Step 4.2: Full investigation binary, clean run

Deploy `dist-investigation\CERBERUS.EXE`. Run:

```
CERBERUS /NICK:BEK-V409 /NOTE:instrum-all /O:C:\INSTR1.INI
```

Observe: canary fires? (Expected: yes — same probes run, just with crumb markers.)

### Step 4.3: Skip OPL fallback

```
CERBERUS /NICK:BEK-V409 /NOTE:skip-oplfb /SKIP:oplfb /O:C:\INSTR2.INI
```

Observe: canary fires? If **no**, the 0x388 OPL fallback was the trigger — either the probe itself or the trace buffer it appends to (`audio_opl_trace`). Move to 4.7.

If **yes**, the fallback is not the sole trigger; continue.

### Step 4.4: Skip S3 probe

```
CERBERUS /NICK:BEK-V409 /NOTE:skip-s3 /SKIP:s3probe /O:C:\INSTR3.INI
```

Canary fires? No → S3 CR30 probe was the trigger; move to 4.7. Yes → continue.

### Step 4.5: Skip BIOS scan (video BIOS text match in C000:0000)

```
CERBERUS /NICK:BEK-V409 /NOTE:skip-bios /SKIP:biosscan /O:C:\INSTR4.INI
```

Canary fires? No → the scan_video_bios loop (byte-walking C000:0000 with `__far` reads) is the trigger. This would be surprising but possible if there's an off-by-one buffer access. Move to 4.7.

### Step 4.6: Skip timing self-check

```
CERBERUS /NICK:BEK-V409 /NOTE:skip-timing /SKIP:TIMING /O:C:\INSTR5.INI
```

Canary fires? No → `timing_self_check` / `timing_dual_measure` on the UMC491 PIT is the trigger. Move to 4.7.

If **all four** individual skips still trigger the canary, the bug is not in any of these three paths (or is in a combination). Try pairwise skips (`/SKIP:oplfb /SKIP:s3probe` etc.). If no combination clears it, the bug is elsewhere and the investigation has to widen.

### Step 4.7: Identified — fetch LAS trails

For whichever step cleared the canary, FTP back:
- The INI from the clean run (e.g. `INSTR2.INI`)
- The `CERBERUS.LAS` or `CERBERUS.LAS` from the crashing runs (whichever exist)

```
get /drive_c/cerberus/INSTR2.INI C:\Development\CERBERUS\tests\captures\bek-v409-2026-04-23-m4\clean.ini
get /drive_c/cerberus/CERBERUS.LAS C:\Development\CERBERUS\tests\captures\bek-v409-2026-04-23-m4\last.las
```

The LAS file shows the last crumb before abort. Crumb names from this instrumentation are:
- `detect.video.s3`
- `detect.video.et4000`
- `detect.video.biosscan`
- `detect.audio.oplfb`
- `timing.self_check`

A LAS ending in `detect.audio.oplfb` (for example) pinpoints the offending probe.

### Step 4.8: Root-cause and fix

Once the offending probe is identified, inspect the probe code in `src/detect/<file>.c`. Common suspects:
- **Wild `__far` pointer write**: probe writes to a far pointer that's computed wrong (e.g. `MK_FP(0, 0xffff)` accidentally).
- **Off-by-one in a static buffer**: scan_video_bios's `need_len[]` array is `[32]` but has `for (s = 0; s < video_db_count && s < 32; s++)`. If `video_db_count > 32` silently, the array overruns.
- **Uninitialized-read path**: probe reads from a static buffer before initializing it; on BEK-V409 the stack layout happens to leave garbage near DGROUP:0 there.
- **OPL fallback probe's trace buffer append**: `opl_trace_append` writes into `audio_opl_trace[128]` with `while (cur < cap - 1 && *fragment != '\0')`. If `cur` wraps (it's `unsigned int`, so it won't) or if a prior fragment corrupted the buffer's NUL terminator, the append could run out of bounds.

Fix the specific defect, rebuild stock, confirm canary no longer fires on BEK-V409 across 10 consecutive cold-boot runs.

### Step 4.9: Capture the fix

Once the fix is validated, cherry-pick or reimplement on `main`. The scratch branch's instrumentation can stay as a separate diagnostic-tools branch or be merged back if the crumb markers are judged durable.

## After the session: what lands on main

1. If M3.1 IIT signature captured: open `docs/research/iit-3c87-signature.md` with the byte pattern; schedule the 0.9.0 M1.3 single-function edit to `fpu_probe_iit_3c87()`.
2. If M3.2 Genoa ET4000 confirmed: mark the plan item validated, no code change.
3. If M4 root-caused: land the fix on main, delete the `investigation/bek-v409-null-write` branch or keep it as reference.
4. Update v0.8.1 real-iron-validation status: replace the "hardware-gated" markers on M1.1, M2.1, M2.2, M2.3 with actual numbers from the 2026-04-23 captures.
5. File new capture archives under `tests/captures/486-BEK-V409-2026-04-23-v081/` and `tests/captures/386-DX40-2026-04-23-v081/`.
6. Refresh `project_cerberus.md` in auto-memory with the session outcome.

## Session 5: IBM 486 parallel validation (~15 min)

**Binary:** stock `dist\CERBERUS.EXE`. Same flags as Session 1.

**Invocation:** same shape as Session 1, different nickname:

```
CERBERUS /NICK:IBM486 /NOTE:0.8.1-validation /O:C:\V081.INI /CSV
```

**Transport:** depends on which IBM 486 this is.
- **PS/1 or ValuePoint** with a supported ISA NIC and packet driver: FTP same as the other 486.
- **PS/2 Model 56/57/70 (MCA)**: no ISA slot for NE2000/3C509. MCA NICs are rare; default to floppy.
- **Any other variant**: try FTP, fall back to floppy. The binary is 170,722 bytes, fits on a 1.44 MB floppy with room; 360 KB would need ARJ.

**What the capture answers:** second 486 sample for consistency cross-check. If this IBM box emits the same `diagnose.fpu.edge_cases_ok=14_of_14` and similarly-shaped `bench.cache.char.*` numbers as BEK-V409, the 0.8.1 probes are stable across 486 silicon variants, not BEK-specific. Differing DGROUP BSS-overwrite symptoms between the two 486s isolate that bug further (BEK-V409-specific vs 486-family-wide).

**Key diff to note**: does the IBM 486's capture contain `*** NULL assignment detected` at exit? If yes, the BSS bug is 486-family-wide and the M4 investigation generalizes. If no, it stays BEK-V409-specific (empirically matches M1 finding).

## Session 6: 286 board first-ever real-iron run (~30 min)

**Goal:** produce CERBERUS's first real-hardware capture on 286-class silicon. Whatever comes out, it's a README claim upgrade from "286 path untested" to "validated."

**Binary:** stock `dist\CERBERUS.EXE`.

**Deployment:**
1. ARJ or LHA the binary down to a 360 KB / 720 KB floppy if needed (most 286s boot 1.2 MB 5.25" or 1.44 MB 3.5" — both hold the 167 KB EXE comfortably).
2. Boot 286 from DOS 6.22 or whatever's available. MS-DOS 3.3+ is the minimum per the CERBERUS claim.
3. If MDA / CGA / Hercules output: `/MONO` may improve readability; skip if color works.

**Invocation (conservative first run):**

```
CERBERUS /Q /SKIP:BENCH /NOUI /NICK:286 /NOTE:0.8.1-first-286 /O:C:\V081.INI /CSV
```

`/Q` = quick mode, `/SKIP:BENCH` = skip benchmarks (they run multiple minutes on 286), `/NOUI` = batch text output. Total runtime estimate: 30-90 seconds.

**What the capture answers:**
- `cpu.class=286` confirms the CPU-class waterfall works on real 286 silicon.
- `fpu.detected=287` (if 287 present) or `none` (if absent) validates the 287 detection path.
- `memory.conventional_kb` and `memory.extended_kb` validate the XMS / INT 15h path. Most 286s have 1-4 MB of extended.
- `video.adapter` validates the MDA / CGA / EGA / Hercules waterfall on era-appropriate display hardware.
- `bus.class=isa16` (AT-class bus).
- Any diagnostic failures reveal real 286-era detection gaps that DOSBox Staging masked.

**Capture back:** put the floppy in a modern machine, copy `V081.INI` + `V081.CSV` to `tests/captures/286-<board-id>-2026-04-23-v081/`. The board-id can be descriptive ("at-clone-ami" or similar).

**Follow-up run if the conservative one succeeds:**

```
CERBERUS /NOUI /NICK:286 /NOTE:0.8.1-full /O:C:\V081F.INI /CSV
```

Runs the full suite. Expected runtime: 15-30 minutes. Detection + diagnosis + benchmarks + consistency. This is the canonical 286 validation capture. Diff against DOSBox Staging's 286-profile run to spot emulator-vs-real divergence.

**Known risks on first 286 run:**
- CERBERUS's bench path has skip rules for 286 (same as XT-class) per `cpu_too_slow_for_char`. If the skip misfires, bench runs forever; `/SKIP:BENCH` is the escape hatch.
- `_exit()` libc bypass (M1.7) was a BEK-V409 specific fix. 286 + different DOS version may not need it; may also have its own libc-teardown quirks.
- `diag_fpu_fingerprint` runs the 5-axis probes. On 287 these behave differently from 387+ — infinity mode is projective (not affine), no FPREM1, no FSIN. All expected; the probes tag these as `FP_FAMILY_LEGACY`.

## Session 7: Leading Edge Model D baseline with current CPU/FPU (~20 min)

**Goal:** first CERBERUS run on XT-class real hardware. Whatever's currently installed in the Model D (V20, maybe with/without 8087).

**Binary:** stock `dist\CERBERUS.EXE`.

**Deployment:** 360 KB 5.25" floppy is the Leading Edge's native format. ARJ/LHA the 167 KB binary to fit without the test-run INIs filling the disk.

**Pre-flight:**
- Make sure MDA / CGA is stable before running. CERBERUS's CGA snow gate (M3.1) should handle composite CGA; MGA doesn't snow.
- Leading Edge has ~640 KB conventional, no extended memory (unless someone added EMS). CERBERUS in 170 KB + stack should have plenty of headroom.
- DOS 3.3 is the floor claim; Model D typically ships with DOS 2.x or 3.x; upgrade to at least 3.3 if available.

**Invocation (conservative first run):**

```
CERBERUS /Q /SKIP:BENCH /NOUI /NICK:LE-ModelD /NOTE:xt-baseline /O:V081.INI /CSV
```

Runtime estimate: 30-60 seconds on 4.77 MHz V20.

**What the capture answers:**
- `cpu.class=8088` (V20 currently tags as 8088 — see §Known DB gap below).
- `fpu.detected=8087` or `none`.
- Memory sizing on 640 KB conventional.
- Video adapter (MGA presumably; CGA if a color card is installed).
- `bus.class=isa8`.
- Consistency rule 9 (`8086_bus`) PASS-validates the 8088+ISA-8 pairing.
- Rule 6 (`extmem_cpu`) validates extended-memory-absent correctness.

**Follow-up full run if conservative succeeds:**

```
CERBERUS /NOUI /NICK:LE-ModelD /NOTE:xt-full /O:V081F.INI /CSV
```

Skip the interactive UI but run full DET+DIAG+BENCH. Benchmark pass on 4.77 MHz V20 may take 15-30 minutes (bench_cpu iterates, bench_memory, bench_fpu against 8087 if present). This is the first ever real-silicon bench number for XT-class CERBERUS.

## Session 8: Leading Edge CPU / FPU swap matrix (~60-90 min, 4-6 cold boots)

**Goal:** validate CERBERUS detection and FPU behavioral fingerprint across the actual XT-class chip combinations.

**Capture each variant to its own INI:**

| Variant | Invocation | Expected `cpu.class` | Expected `fpu.detected` |
|---|---|---|---|
| V20 + 8087 | `/NICK:LE-V20-8087 /O:V20_87.INI /CSV` | 8088 (V20 not discriminated) | 8087 |
| V20, no 8087 | `/NICK:LE-V20-only /O:V20.INI /CSV` | 8088 | none |
| 8088-1 (4.77 MHz) + 8087 | `/NICK:LE-8088-1-FPU /O:88_1_87.INI /CSV` | 8088 | 8087 |
| 8088-1, no 8087 | `/NICK:LE-8088-1 /O:88_1.INI /CSV` | 8088 | none |
| 8088-2 (8 MHz) + 8087 | `/NICK:LE-8088-2-FPU /O:88_2_87.INI /CSV` | 8088 | 8087 |
| 8088-2, no 8087 | `/NICK:LE-8088-2 /O:88_2.INI /CSV` | 8088 | none |

All invocations: `/Q /SKIP:BENCH /NOUI`.

**Cold-reboot protocol:** between each CPU/FPU swap, power off the Model D, do the swap (CPU chip or 8087 socket), power on, boot DOS, run CERBERUS. Powering off is important because 8087 presence bits in some early-PC BIOS latch on cold boot and aren't re-read.

**What the capture matrix answers:**

1. **V20 vs 8088 DB gap.** V20 and 8088 both tag as `cpu.detected=8088`. The `run_signature` (SHA-1 of full INI) will differ between V20 and 8088 captures because subtle bench/diag numbers differ, but the `signature` (hardware identity over 7 canonical keys) will be identical. That's the exact pathology consistency Rule 9 documents: **signature collision across measurably-different hardware** = DB gap.

2. **Real 8087 FPU behavioral fingerprint.** The 5-axis fingerprint (`fpu.fp_*` keys) has only ever been validated on 386+ FPUs. Against 8087 silicon: infinity mode should be **projective** (not affine), pseudo-NaN should be **silently accepted** (FP_FAMILY_LEGACY), FPREM1 should be **absent** (#UD on opcode), FSIN should be **absent** (#UD), FPTAN (M2.2) should **not push 1.0** on stack (early 8087 behavior differs from 387+). IEEE-754 edges (M1.1) may or may not reach 14/14; the 8087 is IEEE-754 pre-final-standard and may diverge on a few edges. Document what it produces; this sets the reference baseline for future 8087 captures.

3. **8088-1 vs 8088-2 speed delta.** Same detect identity, different bench timings. Confirms bench_cpu calibration discriminates clock speeds when BIOS date / other signature keys are identical.

4. **/ONLY:DIAG run against 8087.** After the matrix, one more run with `/ONLY:DIAG /NICK:LE-diag-8087` exercises every diagnostic probe against the 8087 specifically. `diag_fpu` bit-exact tests should pass (8087 is IEEE-754-ish on basic ops). `diag_fpu_fingerprint` reports LEGACY. `diag_fpu_edges` (IEEE-754 edges) may show gaps; those are real 8087 coverage we want to catalog.

**Capture back:** copy every `*.INI` + `*.CSV` to `tests/captures/xt-LE-ModelD-2026-04-23/`. File names document the variant.

## Known DB gaps surfaced by tonight's session

The bench session is expected to expose these DB/detection gaps. None block tonight's work; all file as 0.9.0 scope or earlier:

1. **V20 / V30 discrimination from 8088 / 8086.** Documented deferred in `src/detect/cpu.c:17` ("NEC V20/V30 disambiguation from 8086/8088 is also deferred — the TEST1 instruction probe needs another INT 6 handler"). Tonight's Session 8 is the direct justification to land it. A TEST1 probe is ~50 lines of asm + C; fits in a 0.9.0 M3 follow-up.
2. **IBM 486 family variants.** `cpu_db` has AMD and generic Intel 486 entries; IBM 486SLC / SLC2 / BL variants may not be covered. Tonight's Session 5 capture will tell us which entry matched (or if it tagged generic). File 0.9.0 M3.4 DB extension.
3. **286 stepping granularity.** `cpu_db` has a generic `286` legacy entry. If Session 6's 286 has a specific stepping signature CERBERUS could surface, we learn it empirically tonight.
4. **8087 behavioral fingerprint reference.** The 5-axis fingerprint has no 8087-real-silicon reference today. Session 8's captures set that baseline. File as 0.9.0 docs update.
5. **Leading Edge BIOS signature.** Leading Edge-specific BIOS strings may not be in `bios_db`. Session 7 / 8 captures will surface that via `bios.family=unknown` or `bios.vendor=unknown`. File 0.9.0 M3.5 BIOS DB extension.

## Things to bring to the bench

- USB serial or FTP access to BEK-V409 + 386 DX-40 (already set up)
- 360 KB and 1.44 MB floppies, at least 4 pre-formatted
- ARJ or LHA on a floppy if target DOS doesn't have them already (for size-constrained disks)
- A way to take photos of the screen if any output is visually instructive
- Patience with cold reboot cycles — BEK-V409 NULL bug does not always surface on warm boot; 8087-install detection often requires cold boot
- Good lighting for the CPU socket swaps on the Leading Edge (ZIF socket? or LIF? 8088 is usually LIF with the gold-leg DIP, requires careful extraction)
- Anti-static wrist strap for the chip swaps

## Expected total time

- Session 1 (both 486s + 386, validation parade): ~60 min
- Session 2 (386 + IIT signature): ~5 min
- Session 3 (386 + Genoa validation): ~3 min
- Session 4 (BEK-V409 NULL investigation, removal-at-a-time): ~45 min
- Session 5 (IBM 486 validation): ~20 min
- Session 6 (286 first run): ~30-45 min
- Session 7 (Leading Edge baseline): ~20 min
- Session 8 (XT swap matrix): ~60-90 min

**Total: ~4-5 hours of bench work.** Realistically this spans two or three evenings; tonight is as much as you can fit.

**Priority order if time-constrained:**
1. Sessions 1-4 (tagged tier, scheduled 0.8.1 validation) — highest priority.
2. Session 5 (IBM 486) — quick, parallel with 486 work already running.
3. Session 7 (Leading Edge baseline) — cheap, unlocks XT-class README claim upgrade.
4. Session 6 (286) — if time remains.
5. Session 8 (XT swap matrix) — stretch goal; most of this could slip to a future evening without blocking 0.9.0 work.
