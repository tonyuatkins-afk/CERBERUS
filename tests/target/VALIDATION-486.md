# Phase 1 Validation — 486 VLB bench machine

Task 1.10 real-hardware gate. First real-iron run of CERBERUS.EXE.

## Machine under test

Fill in what you know BEFORE the run — this is the ground truth CERBERUS's output gets compared against.

| Field | Known value |
|---|---|
| CPU | _(e.g. 486DX2-66, Am5x86-P75)_ |
| FPU | _(integrated / external 487 / none)_ |
| Clock (MHz) | _(nominal speed)_ |
| RAM conventional (KB) | _(usually 640)_ |
| RAM extended (KB) | _(total minus 1024)_ |
| Bus | VLB (ISA-16 + VESA Local Bus) |
| Video chipset | _(S3 — if you can narrow to 86C911 / Trio64 / Vision864 etc., note it)_ |
| Video RAM | _(in KB)_ |
| Sound | _(SB-compat? AdLib? none?)_ |
| BIOS vendor | _(AMI / Award / Phoenix — printed at boot)_ |
| BIOS date | _(the date from the boot banner)_ |
| DOS version | MS-DOS 6.22 |

## Run procedure

1. Copy `CERBERUS.EXE` + `PHASE1.BAT` to a directory on the target (floppy root or C:\CERBERUS works).
2. From that directory: `PHASE1`
3. The batch prints the INI contents as it completes. Note any alerts that render on screen.
4. Sneaker-net `BENCH486.INI` (and `CERBERUS.UNK` if one was produced) back.

## Compare against ground truth

For each row in the emitted INI at CONF_HIGH, check whether it matches the known spec above. A **HIGH-confidence value that contradicts the ground truth is a gate failure** per plan §Task 1.10.

### Canonical signature keys (gate-critical)

These seven feed the hardware-identity signature. All should be HIGH confidence AND match ground truth:

- `cpu.detected` — should name the CPU family
- `cpu.class` — "intel" / "amd5x86" / "cyrix" / etc. (CPUID-capable) or legacy token (pre-CPUID)
- `fpu.detected` — "integrated-486" for a real DX
- `memory.conventional_kb` — matches DOS `MEM` output
- `memory.extended_kb` — matches DOS `MEM` output
- `video.adapter` — "vga" / "svga" / "mda" / "cga" / "ega"
- `bus.class` — "isa16" (if VLB isn't separately probed) or an indication of VLB

### Phase 4 consistency rules — expected verdicts on this machine

| Rule | Expected |
|---|---|
| `consistency.486dx_fpu` | PASS (assuming CPU is a real DX) |
| `consistency.extmem_cpu` | PASS (486 with >0 extended) |
| `consistency.fpu_diag_bench` | PASS (diag + bench agree) |
| `consistency.timing_independence` | PASS (the big one — the bug fixed in R1 of the quality gate would WARN here) |
| `consistency.timing_self_check` | absent (self-check succeeded) |

Any WARN or FAIL verdict is worth examining — either a real hardware quirk worth documenting, or a CERBERUS bug.

### Thermal (7-pass calibrated run will populate)

- `thermal.cpu` — PASS or WARN
- `thermal.cpu.s` — signed S statistic
- `thermal.cpu.direction` — "up" / "down" / "flat". On a 486 that's reached operating temp, expect "flat" or low-magnitude "down" (slight residual warmup).

### Timing self-check (rule 4a backbone)

- `timing.cross_check.pit_us` — ≈ 220000
- `timing.cross_check.bios_us` — ≈ 219700
- `timing.cross_check.status` — "ok"
- Divergence between pit_us and bios_us should be < 15%. If not, we have a real rule-4a hit.

## Known unknowns

CERBERUS has only a GENERIC "S3 generic" entry in video_db — your chipset won't be narrowed to a specific model unless the ROM signature happens to match one of the 28 video_db entries. A `CERBERUS.UNK` file being generated is EXPECTED and useful — send it back and we'll add the specific chip to the DB.

## After the run

Paste the full `BENCH486.INI` back in the chat. We'll:
1. Diff against this ground-truth table
2. Flag any HIGH-confidence mismatches as gate failures
3. Update hardware databases with any newly-identified chips
4. Decide whether v0.2.0 is ready to tag
