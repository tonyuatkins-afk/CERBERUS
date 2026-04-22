# Bench Session Playbook: 2026-04-23 (evening)

Pre-staged during the 2026-04-22 autonomous block. Run this from top to bottom; each section says what to FTP, what to run, what to look for, and what the capture answers. Save time by running both boxes in parallel when possible (they're on different static IPs).

## Targets

| Box | IP | FTP login | Notes |
|---|---|---|---|
| **BEK-V409** (486 DX-2-66) | 10.69.69.160 | tony / netisa | AMI 11/11/92 + S3 Trio64 + Vibra 16S + 63 MB XMS. Source of the `*** NULL assignment detected` canary. |
| **386 DX-40** | 10.69.69.161 | tony / netisa | IIT 3C87 + Genoa ET4000 + Aztech ISA. Reference-clean on the NULL canary. |

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

## Things to bring to the bench

- USB serial or FTP access to both boxes (already set up)
- A way to take photos of the screen if the canary output is visually instructive
- Patience with cold reboot cycles — the BEK-V409 NULL bug does not always surface on warm boot (per the methodology doc's removal-at-a-time protocol)
