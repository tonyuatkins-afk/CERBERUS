# Consistency Engine Rules

The consistency engine runs after DETECT, DIAGNOSE, and BENCHMARK have populated the result table. Each rule reads specific keys, compares them against each other or against expected patterns, and emits one `consistency.<rule_name>` row with a verdict of PASS, WARN, or FAIL.

This document is the methodology reference — what each rule can catch, what it cannot, and why.

## Design principles

- **Absence of a required key is never a fault.** A rule that depends on keys the detection phase didn't populate simply returns no output. "Rule not applicable" is a valid outcome, distinct from PASS.
- **Rules never mutate existing entries.** They add their own `consistency.*` rows. Detect and diagnose own their respective verdicts.
- **Every rule documents what it does and does not detect.** Opacity is the anti-pattern. If a rule can only catch asymmetric failures (where two measurement paths disagree), that's a limitation worth stating.
- **PASS rows are emitted for positive confirmation.** Absence of output would be ambiguous with a broken engine. A run that hits a rule and passes it says so explicitly in the INI.
- **Verdict semantics:**
  - **PASS** — the rule's prerequisites were present and the values agreed.
  - **WARN** — the rule's prerequisites were present but the values are suspicious rather than definitely wrong. Usually means detection gaps rather than genuine hardware faults.
  - **FAIL** — the rule's prerequisites were present and the values contradict each other physically or logically. This is the interesting case — it surfaces the "what the hardware claims vs what it actually is" gap that the project is built around.

## Active rules (v0.5)

Each rule below names the key it emits (`consistency.<name>`), the prerequisite result-table keys it reads, and the conditions for each verdict.

### Rule 1 — `consistency.486dx_fpu`

**Claim:** If the CPU is a 486DX-class part, the FPU must be integrated.

**Prerequisites:** `cpu.detected` contains "486DX" (but not "486SX"); `fpu.detected` present.

**Verdicts:**
- **PASS** — `fpu.detected` contains "integrated"
- **FAIL** — anything else

**What it catches:**
- A 486SX with a faked "486DX" silkscreen being detected as DX but failing the FPU check.
- A legitimate 486SX misdetected as DX somewhere in the probe chain.

**What it does not catch:**
- A real 486DX with a broken FPU. That's the `diag_fpu` domain — the FPU will still report as "integrated" via CPUID, but the diagnostic will flag it as FAIL.
- A 486SX + external 487 coprocessor combination. The 487 actually disables the host 486SX and takes over as a full 486DX, so CPUID reports DX and the rule passes correctly.

### Rule 2 — `consistency.486sx_fpu`

**Claim:** If the CPU is a 486SX, the FPU must not be integrated.

**Prerequisites:** `cpu.detected` contains "486SX"; `fpu.detected` present.

**Verdicts:**
- **PASS** — `fpu.detected` is anything other than "integrated..."
- **FAIL** — `fpu.detected` contains "integrated"

**What it catches:**
- Detection confusion where a 486SX reports integrated FPU (likely a CPUID feature-bit misread or 487 socket misidentification).

**What it does not catch:**
- A 486SX + 487 coprocessor setup. The 487 takes over and reports as a 486DX via CPUID, so `cpu.detected` would be 486DX not 486SX, and rule 2 doesn't apply.

### Rule 3 — `consistency.386sx_bus`

**Claim:** A system with a 386SX CPU must have an ISA-16 or better bus.

**Prerequisites:** `cpu.detected` contains "386SX"; `bus.class` present.

**Verdicts:**
- **PASS** — `bus.class` is not "isa8"
- **FAIL** — `bus.class` equals "isa8"

**What it catches:**
- Detection disagreement where a bus probe misclassified a 386SX system as XT-class.

**What it does not catch:**
- Anything today. The current `cpu_db` legacy-class path only has a generic "386" entry; no friendly names contain "386SX" until per-variant entries land. Rule 3 is correctly dormant until the DB gains that specificity. A future contributor adding "Intel i386SX-16" and "Intel i386SX-20" entries to `hw_db/cpus.csv` will activate this rule automatically.

### Rule 5 — `consistency.fpu_diag_bench`

**Claim:** If the FPU diagnostic passes, the FPU benchmark must produce a non-zero result. If the diagnostic fails or is skipped, the benchmark must also have been skipped.

**Prerequisites:** At least one of `diagnose.fpu.compound` or `bench.fpu.ops_per_sec` present.

**Verdicts:**
- **PASS** — diagnostic pass-state and benchmark presence agree.
- **WARN** — they disagree. Diag passed but bench has no result, or vice versa.

**What it catches:**
- Cross-phase disagreement on FPU liveness — one head thinks it works, another couldn't exercise it.

**What it does not catch:**
- Correlated faults — if both diagnose and benchmark share a code path that fails the same way (e.g., a Watcom runtime bug), they will both fail equally and this rule will report PASS. The rule detects asymmetry, not absolute correctness.

### Rule 6 — `consistency.extmem_cpu`

**Claim:** Reported extended memory (`memory.extended_kb > 0`) implies the CPU class is 286 or later.

**Prerequisites:** `memory.extended_kb` > 0; `cpu.class` present.

**Verdicts:**
- **PASS** — CPU class is not in {8086, 8088, v20, v30}.
- **FAIL** — CPU class is in that set.

**What it catches:**
- A detection bug where extended memory is reported on a physically-impossible host (8086-class CPUs cannot access memory above 1MB).

**What it does not catch:**
- Wrong extended-memory values on a correctly-detected 286+ system. Size verification requires actual memory probing under diagnostic pattern testing, which is `diag_mem`'s domain.

### Rule 7 — `consistency.audio_mixer_chip`

**Claim:** When the audio database names an expected mixer chip for the matched row, the hardware mixer probe must observe the same chip.

**Prerequisites:** `audio.mixer_chip_expected` (from `audio_db` row) and `audio.mixer_chip_observed` (from `probe_mixer_chip` at BLASTER-base + 4 / + 5 reg 0x80) both present.

**Verdicts:**
- **PASS** — `expected` equals `observed` (e.g. both `CT1745`).
- **WARN** — `expected` is `unknown` (DB row has not yet been populated with verified mixer data) but `observed` is a named chip. Logged so real-hardware contributions feeding back into `hw_db/audio.csv` are visible.
- **FAIL** — `expected` is a named chip and `observed` is `none` or a different named chip.
- **no-op** — one or both keys absent (e.g. the audio card has no mixer to probe, or `/ONLY:DIAG` skipped the audio probe).

**What it catches:**
- A DB row mis-assigned to a given BLASTER-base/DSP-version composite key — the OPL+DSP family alone cannot fully discriminate SB16 / Vibra 16S / AWE32 / AWE64, and the CT1745 mixer presence splits the SB16 / Vibra 16 family from the older CT1335 / no-mixer Sound Blaster Pro family.
- Counterfeit rebrands where the DSP fingerprint matches a card that should carry a CT1745 but the mixer probe doesn't find one.

**What it does not catch:**
- Two CT1745-bearing cards from the same family (e.g. two SB16 variants with different CTxxxx mixer silicon revisions). Mixer-chip granularity is family-level, not part-number-level.
- A CT1745 mixer whose register map was altered by a PnP driver after CERBERUS's probe — the observed value is a single snapshot.

### Rule 4a — `consistency.timing_independence`

**Claim:** Two timekeeping paths that share a crystal but use different PIT channels should agree on the duration of any given real-time interval.

**Prerequisites:** `timing.cross_check.pit_us` and `timing.cross_check.bios_us` present. These are written by `timing_self_check()` during startup; it spans a fixed real-time interval (4 BIOS ticks ≈ 220 ms) and reports elapsed µs computed from PIT Channel 2 (what `timing.c` uses for every measurement) and from the BIOS system-tick counter at `0040:006Ch` (driven by PIT Channel 0). Both channels share the 1.193182 MHz crystal.

**Verdicts:**
- **PASS** — `|pit_us − bios_us| ≤ bios_us × 15 %`
- **WARN** — divergence exceeds 15 %

The BIOS tick is the reference denominator — PIT is tested against it. Absolute delta is compared against 15% of BIOS-derived µs, not of the mean. An equivalent delta reads as tighter when BIOS is smaller.

**Status key (`timing.cross_check.status`):** the self-check also emits a status row so a reader of the INI can distinguish "skipped" from "attempted but unusable" from "attempted and passed/warned":
- `"ok"` — self-check ran to completion; `pit_us` and `bios_us` are present and rule 4a compared them.
- `"measurement_failed"` — self-check attempted but its wrap-sanity check failed (PIT poll loop missed wraps, or the HW layer's upper wrap cap tripped). Rule 4a then no-ops on the missing `pit_us`/`bios_us` keys, and a separate `consistency.timing_self_check` WARN row is emitted so the UI alert renderer (which filters on the `consistency.` prefix) surfaces the problem to the user instead of burying it in the INI.
- absent — user passed `/SKIP:TIMING`, so `timing_self_check` was not called at all.

**What it catches:**
- Bugs in `timing.c`'s math, including the 16-bit integer overflow trap that recurs whenever someone writes `ticks * 838` without `UL` promotion (see `feedback_dos_16bit_int.md`). PIT C2 then reads low while the BIOS-tick path stays honest, producing asymmetric disagreement.
- PIT C2 wrap-counter miscounts (too few → PIT reads low; too many → PIT reads high).
- A TSR that has hooked INT 8 and drops ticks without reissuing them — BIOS path then reads low.

**What it does not catch:**
- A crystal running off-spec. Both channels use the same oscillator, so both report the same wrong µs. This is a limitation inherent to the methodology; detecting off-spec crystals requires a truly independent reference (wall-clock sync, network NTP), which is out of scope for a DOS diagnostic.
- Any failure that biases PIT C2 and BIOS-tick identically. By construction, this rule catches only *asymmetric* timing bugs.

**Verdict is WARN not FAIL.** A disagreement says the two paths observed the same interval differently; it doesn't say which is the liar. Downstream timing-derived measurements should be viewed with added skepticism, but the run is not invalidated outright.

### Rule 9 — `consistency.8086_bus`

**Claim:** An 8086-class CPU must be on an ISA-8 bus.

**Prerequisites:** `cpu.class` in {8086, 8088, v20, v30}; `bus.class` present.

**Verdicts:**
- **PASS** — `bus.class` is "isa8"
- **WARN** — `bus.class` is "unknown" (suspicious but not definitely wrong — probe may have just missed)
- **FAIL** — `bus.class` is anything else (isa16 / vlb / pci)

**What it catches:**
- A detection bug where the bus probe returns a bus class physically incompatible with the 8086-class CPU — PCI or VLB on an 8086 is electrically impossible.

**What it does not catch:**
- Any correctly-detected 8086-class system. The rule is a bounds check, not a diagnostic.

### Rule 10 — `consistency.whetstone_fpu`

**Claim:** The `bench_whetstone` completion state must agree with the `detect_fpu` report.

**Prerequisites:** `fpu.detected` and `bench.fpu.whetstone_status` both present.

**Verdicts:**
- **PASS** — FPU present (`fpu.detected != "none"`) and Whetstone ran to completion (`whetstone_status == "ok"`) with a non-zero `k_whetstones`.
- **PASS** — FPU absent (`fpu.detected == "none"`) and Whetstone status is exactly `skipped_no_fpu` → PASS. Any other non-`ok` non-`inconclusive_*` status on the FPU-absent-detect side → no-op (rule not applicable — unknown state).
- **WARN** — FPU present and Whetstone ran but `k_whetstones == 0` (suspicious but not proof of fault).
- **FAIL** — FPU present but Whetstone reports skipped (or the mirror case: detect says "none" but Whetstone produced a number). Classic case for the latter: a socketed 8087 the FNINIT/FNSTSW probe under-reported.
- **no-op** — whetstone_status starts with `"inconclusive"` (e.g. `inconclusive_elapsed_zero` — measurement loop finished inside one PIT tick, an emulator artifact already surfaced by the WARN verdict attached to the status row itself; not a detection-consistency issue). Also no-op when either prerequisite key is absent.

**What it catches:**
- `detect_fpu` under-reporting a present FPU. The benchmark successfully executed x87 instructions, which cannot happen without hardware or a coprocessor emulator loaded — either way, "no FPU" is wrong.
- Memory-corruption-class bugs where one head sees the FPU and the other does not.

**What it does not catch:**
- A broken FPU that completes Whetstone with wrong numbers. Rule 10 treats "completed" as PASS; bit-exact correctness is `diag_fpu`'s domain and covered by Rule 5.
- A software FPU emulator (correctly classified as "present" at the ISA level). Consumers needing hardware-vs-emulator distinction read `fpu.vendor` / `fpu.friendly` directly.

**Slot rationale:** the plan originally reserved Rule 8 for cache-stride vs CPUID-leaf-2 cross-check (still reserved, see Deferred rules). Rule 9 was already taken by `8086_bus`. Rule 10 is the next free slot — same off-by-one pattern as Rule 7.

## Deferred rules

These are noted in the plan and will land as their prerequisite phases complete. Each is correctly no-op today.

- **Rule 4b — MIPS within class_ipc range.** Compare `bench.cpu.iters_per_sec` against `cpu.clock_mhz × class_ipc_low..high` from `cpu_db`. Blocked on (a) `cpu_db` rows gaining `class_ipc_low_q16` and `class_ipc_high_q16` fields, and (b) Phase 3 calibrated-mode MIPS-equivalent reporting.
- **Rule 8 — Cache stride-knee locations match CPUID leaf 2 descriptors.** Blocked on (a) cache-bandwidth stride tests in `bench_cache`, and (b) CPUID leaf 2 descriptor decode on Pentium+. Slot remains reserved even though Rule 10 has since been assigned above it.

## Adding a new rule

1. Open `src/core/consist.c`.
2. Add a `static void rule_<name>(result_table_t *t)` function following the existing pattern:
   - Call `find_key()` for each prerequisite.
   - Return early if any prerequisite is absent — no emit.
   - Compare values; call `report_add_str(t, "consistency.<name>", ...)` with the appropriate verdict.
   - Place the function near the other rules in order of plan-assigned number.
3. Add a comment block above the function documenting **what it catches AND what it cannot**. The "cannot" section is as important as the "catches" section — opacity is the anti-pattern.
4. Call the function from `consist_check()`.
5. Add scenarios to `tests/host/test_consist.c` covering: positive case (PASS), negative case (FAIL or WARN), and prerequisite-absent case (no emit).
6. Add a section to this document explaining the rule.

**Key-uniqueness contract:** `report_add_*` has no dedup (see the note at the top of `src/core/report.h`). Two rules that emit the same `consistency.<name>` key will both render in the INI and in any UI view that iterates the table — the user sees a visual duplicate. Pick a `<name>` that does not collide with any existing rule in this document, and grep `src/core/consist.c` for `consistency.` before picking a new name.

Contributions of new rules via pull request are welcome — especially rules grounded in specific real-hardware observations that existing rules would have missed.
