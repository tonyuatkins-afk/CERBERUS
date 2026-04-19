# Next Validation Platforms

> **Status:** Forward-looking planning doc. Captures known validation targets in Tony's possession beyond the BEK-V409 / i486DX-2 bench box, per-platform expected surfacing, and recommended validation order. Re-read at the start of the next real-iron session.

## Known validation targets beyond BEK-V409

### 386 DX-40 (with FPU)

Currently in storage. Vintage DX-40 with a separate 387 FPU socket populated. Most likely chipset classes: OPTi 495, SiS 461, UMC481 (sibling to the UMC491 on the 486 box but older-generation). Bus: ISA-16 + likely VLB, depending on motherboard.

**Why it's valuable.** The 386 DX-40 is structurally different from the 486 DX-2 in ways that exercise fresh detection / diagnostic / bench code paths:

- **No on-die cache.** 486 DX2 has integrated L1; the 386 has whatever the motherboard provides as L2 (often 64-128 KB of external SRAM with WB cache on sockets). CERBERUS's cache probe (`src/detect/cache.c`) uses class-inference today — that path gets exercised in a way DOSBox-X can't simulate meaningfully.
- **Separate 387 FPU detection.** The entire integrated-FPU vs external-FPU discrimination in `cpu.c` + `fpu.c` + consistency Rules 1 + 2 lives for this moment. Today only 486DX path is real-iron-validated.
- **Different 8254.** Not UMC491 — whatever OPTi / SiS / UMC ship in the older-generation chipset. The latch-race fingerprint from `eeba319` was UMC491-specific; the 386's 8254 may have an entirely different bias pattern, or work correctly with the shape-check guardrails from `6c3a023` in place.
- **Slower CPU exposes different timing scales.** `timing_wait_us` correctness at sub-µs resolution is untested below 486 DX-2 speed on real iron. An OPL probe that works on DX-2 via `timing_wait_us(100UL)` may need a different delay on a 40 MHz CPU with different bus latencies.

**Bug budget: 3–7.** Structurally confident that at least three new real-hardware-only failure modes surface — the 486 gate turned up five, and the 386's differences across cache, FPU, and chipset families are arguably larger than whatever the current DX-2 already exercises. Upper bound is speculation; lower bound is the minimum confidence level to plan resource allocation.

### IBM PS/1 Consultant (486-class)

Exact CPU TBD (likely 486SX or 486DX, pre-DX2 generation), chipset TBD, bus almost certainly MCA (IBM's post-ISA proprietary bus on PS/1 and PS/2 lines). IBM-proprietary BIOS almost certainly not in the current `bios_db.c`.

**Why it's valuable.**

- **First 486SX real-iron data point.** Rule 2 (`486sx_no_integrated_fpu`) has never seen a real SX — only synthetic scenarios in host tests. The silkscreen-vs-behavior check is load-bearing for counterfeit detection at v1.0.
- **MCA bus detection.** `src/detect/bus.c` today knows ISA-8 / ISA-16 / VLB-possible / PCI. MCA is a whole class — needs a probe (likely via a specific POS / PS/2 ID register read) and a DB schema extension. Catches any code that assumed "if not PCI then ISA-ish."
- **IBM BIOS family not in DB.** `bios_db.csv` has AMI / Award / Phoenix / MR-BIOS / DTK. PS/1 ships with an IBM BIOS that self-identifies differently. `CERBERUS.UNK` path gets its first real exercise.
- **Unfamiliar video chipset.** PS/1 Consultant shipped with various IBM-branded video controllers, some with oddities that DOSBox-X doesn't model faithfully. Fresh territory for `video_db.csv`.

**Bug budget: 2–5.** Smaller than the 386 because the PS/1 is still 486-class — CPU and FPU paths likely behave identically to the BEK-V409. The delta is concentrated in bus / BIOS / video / motherboard-signature rather than processor-class fundamentals.

## Recommended validation order

**BEK-V409 → 386 DX-40 → IBM PS/1 Consultant.**

Rationale:

1. **BEK-V409 is validated** (2026-04-18). Uses it as the known-good regression anchor for subsequent platform work — every 386 / PS/1 change gets a smoke test on the 486 box before committing.
2. **386 DX-40 next** because it's structurally more different from the 486 DX-2 than the PS/1 is. More different ⇒ more fresh code paths per bug discovered ⇒ better return on investigation effort. No-on-die-cache + separate-FPU alone exercise code that hasn't seen real silicon yet.
3. **PS/1 Consultant last** of these three. The 486-class CPU means most of the afternoon-gate's 486-specific fixes apply directly; the delta work concentrates in bus / BIOS / video which are more scoped. Reserves it for when the 386 has surfaced whatever chipset / cache / FPU bugs are going to surface, so the PS/1 session isn't bottlenecked on overlapping work.

## Preparation before each platform session

Per-box pre-flight (universal):

1. Cold-boot cycle with CMOS inspection — record BIOS date, CPU autodetect, memory count, drive geometry. These go into the session report as ground truth CERBERUS cross-checks against.
2. Capture a CERBERUS run on the BEK-V409 first (regression anchor), archived alongside the new platform's captures.
3. Pre-populate `hw_db/submissions/` with a markdown stub for the new platform so any `CERBERUS.UNK` captures land with context-ready notes.
4. Have `CERBBASE.EXE` + tip-of-tree `CERBERUS.EXE` on hand for A/B comparison if any bug surfaces that's ambiguous between tool regression and hardware behavior.

Per-platform-specific pre-flight:

### 386 DX-40

- Confirm the 387 FPU is socketed and responsive before CERBERUS runs (a DOS `DEBUG` session checking CR0 bit 2 / bit 3 states, or a Norton SI run, works).
- Check for a cache-enable jumper on the motherboard — some boards ship with cache disabled by default and have dramatically different bench numbers.
- Identify the chipset by part-number scan (OPTi 495, SiS 461, UMC481 — photograph the board and note the largest chip near the CPU socket).

### IBM PS/1 Consultant

- Identify the exact CPU — IBM branded the chip, but the silkscreen usually names the Intel/AMD part underneath. If SX, Rule 2 is load-bearing; if DX, Rule 1 is.
- Confirm MCA bus by inspection (no ISA slots visible; unique riser or proprietary expansion connector).
- Check if the machine has the IBM PS/1 diagnostic partition / reference disk accessible — that partition's IBM diagnostic reference numbers serve as our cross-check for bench results, analogous to the CheckIt 3.0 numbers captured on the BEK-V409.

## Session-planning reminders

- Each platform session gets its own `docs/sessions/SESSION_REPORT_<date>_<platform>.md` archive, modeled on the 2026-04-18 evening report structure.
- Each platform gets its own corpus directory: `tests/captures/386dx40-real-<date>/` or similar.
- Follow the 2026-04-18 discipline: commit the corpus BEFORE diagnosing bugs, so the "this iteration's INI" is a git-addressable artifact throughout the session.
- Each real-hardware bug fix includes an INI diff in the commit body showing the before/after, same as `eeba319` did for the 486.
