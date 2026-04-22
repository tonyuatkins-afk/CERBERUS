# CERBERUS 0.8.0 M1 hardware validation protocol

Scope: produce the real-hardware captures that M1.8 and M1.9 require.
Owner: Tony (has the hardware). Claude supports ground-truth checking,
diff analysis, and archival.

Stock binary under test: `CERBERUS.EXE` built from the M1 tree (Whetstone
emit suppressed, runtime upload compiled out, nickname buffer fix, cpu.class
normalization, bench_cpu DB range widened, DGROUP audit tool in place).

## Ship-gate contract

Per 0.8.0 plan §4 ship criterion 1, the 0.8.0 tag is gated on one of:

- **Four captures.** 486 + 386 + 286-or-AT + 8088-or-XT all archived under
  `tests/captures/`. README can retain the "8088 floor" claim.
- **Three captures.** 486 + 386 + 286-or-AT archived. README retracts the
  floor claim to "286 through 486 validated, XT-class validation pending."

Both are ship-valid. Pick based on what hardware is in reach during the
0.8.0 window.

## Per-capture procedure

For each machine:

1. Ground truth sheet (before the run).
   - CPU silkscreen photo or documented type / clock.
   - FPU socketed + type OR "integrated" OR "none".
   - RAM conventional (KB) and extended (KB) per DOS `MEM`.
   - BIOS vendor / date from boot banner.
   - Video adapter and chipset (silkscreen or BIOS banner).
   - Audio card (silkscreen / driver).
   - Bus (ISA-8 / ISA-16 / VLB / PCI).
   - DOS version.

2. Cold boot. Write the ground-truth sheet first so the expected shape is
   captured before CERBERUS colors it.

3. Sneakernet `CERBERUS.EXE` (M1 stock build) to the machine. Run it as:

   ```
   CERBERUS /NICK:<machine-id> /O:C:\CAPTURE.INI
   ```

   The `/NICK` carries into the INI as `upload.nickname=<value>` so the
   capture self-identifies. Use a short stable token (e.g. `386-VLB`,
   `PS2-286`, `XT-CLONE`).

4. Observe exit. The program must return to DOS cleanly. Any reboot /
   hang / BIOS #05 is a ship-blocker regression from the M1 changes.

5. Retrieve `C:\CAPTURE.INI` and, if present, `C:\CERBERUS.UNK`.

6. File the capture:

   ```
   tests/captures/<class>-<machine-id>-<date>/
       capture.ini            the full CERBERUS.INI
       capture.unk            the CERBERUS.UNK if one was emitted
       ground-truth.md        the sheet from step 1
       README.md              narrative: what this capture proves
   ```

## Diff against ground truth

For each HIGH-confidence INI row that corresponds to a ground-truth field,
verify match. Any HIGH-confidence row that contradicts ground truth is a
gate failure. Medium-confidence rows get logged but do not block.

Canonical signature keys (all must be HIGH and match ground truth):

| Key | Expected pattern |
|---|---|
| `cpu.detected` | friendly CPU name |
| `cpu.class` | family token: `486` / `386` / `286` / `8086` / `v20` (M1.4 normalized) |
| `cpu.vendor` | CPUID vendor string or absent on pre-CPUID |
| `fpu.detected` | `integrated-486` / `387` / `287` / `8087` / `none` |
| `memory.conventional_kb` | matches DOS MEM |
| `memory.extended_kb` | matches DOS MEM |
| `bus.class` | `isa8` / `isa16` / `vlb` / `pci` |
| `video.adapter` | `vga` / `svga` / `ega` / `cga` / `mda` |

## Per-tier expectations

### 486 baseline (BEK-V409)

See `VALIDATION-486.md` for the full protocol. Regression check only.
Expected consistency verdicts all PASS on a healthy DX-2 with working
HIMEM + EMM386.

Delta from v0.7.1: `cpu.class=486` (was `cpu.class=GenuineIntel`),
`cpu.vendor=GenuineIntel` now present as a separate key, `upload.status=
not_built` (was `upload.status=offline` or uploaded attempt), no
`bench.fpu.k_whetstones` row, `bench.fpu.whetstone_status=disabled_for_release`
present instead. `nickname=<value>` must be verbatim (issue #9).

### 386 capture

New. Targets: 386DX with external 387, 386DX no FPU, 386SX with or
without 387SX, Am386 or Cyrix 486DLC, RapidCAD, ULSI.

Expected INI shape for a generic 386DX with 387:

- `cpu.class=386`
- `cpu.vendor=` absent (CPUID arrived mid-486-era; most 386s lack it)
- `fpu.detected=387` or `integrated` or `none`
- `memory.extended_kb` typically 1,024 to 16,384
- `bus.class=isa16`
- `video.adapter=` vga / ega
- `cache.present=no` unless the board has external L2 SRAM

Consistency rules that should fire:
- Rule 6 (`extmem_cpu`) PASS — 386 can access extended
- Rule 9 (`8086_bus`) no-op — not an XT-class CPU
- Rule 11 (`dma_class_coherence`) no-op — not XT-class
- Rule 1 and 2 (486dx_fpu, 486sx_fpu) no-op — not a 486

Cache characterization probes self-skip on no-cache hardware per v0.7.1
design.

What 386 validation proves: pre-CPUID detection path, external-FPU socket
path, 32-bit register usage by timing backend with PIT (no TSC on 386).

### 286 capture

New. Targets: IBM PC/AT class, Compaq Deskpro 286, any AT clone with 286
and 287 socket. Empty socket is fine and is itself a useful path to test.

Expected INI shape:

- `cpu.class=286`
- `cpu.vendor=` absent
- `fpu.detected=287` or `none`
- `memory.extended_kb=` typically 0 to 16,384 (INT 15h AH=88h path)
- `bus.class=isa16`
- `video.adapter=` ega / cga / mda
- `cache.present=no`

Consistency rules:
- Rule 6 (`extmem_cpu`) PASS regardless of extmem value
- Rule 11 (`dma_class_coherence`) no-op — slave DMA is expected on AT
- Rule 4a timing self-check should produce valid pit_us / bios_us; the
  original 8254 is the reference PIT, not the UMC491 clone the 486
  bench-box uses

What 286 validation proves: pre-CPUID, 8087 socket path, AT-side bus,
slave DMA present, 8042 keyboard controller, RTC via port 70h/71h.

### 8088 / XT capture (load-bearing for the floor claim)

New and architecturally distinct from 286. Targets: IBM PC, IBM PC/XT,
any XT clone with 8088 or 8086 or NEC V20 or V30. MDA or CGA adapter.
ISA-8 bus. Typical 256 KB or 640 KB conventional.

Expected INI shape:

- `cpu.class=8088` (or `8086` if promoted by higher layer)
- `cpu.vendor=` absent
- `fpu.detected=none` on most machines, `8087` if a socketed coprocessor
- `memory.conventional_kb` 256 to 640
- `memory.extended_kb=0` (INT 15h AH=88h returns 0 or CF set)
- `bus.class=isa8`
- `video.adapter=` mda / cga
- `cache.present=no`

Consistency rules:
- Rule 6 (`extmem_cpu`) PASS — extmem=0 is consistent with 8088
- Rule 9 (`8086_bus`) PASS — XT-class and ISA-8 agree
- Rule 11 (`dma_class_coherence`) PASS — `dma.ch5_status=skipped_no_slave`,
  no contradiction with XT class

What 8088 validation proves: floor-hardware detection path, empty-8087
FPU-absent path, XT-class bus + slave-DMA absence, MDA mono rendering
path in `ui.c`, original 8253 PIT (not a clone), crumb round-trip on
4.77 MHz INT 21h, scroll-viewport rendering at slow REP STOSW, intro
splash on non-VGA adapters.

If this capture does not happen in the 0.8.0 window: the README's
"validated on ..." claim retracts to "286 through 486", and "8088 is
the design floor" is removed. Both honest. See plan §10 claim
hierarchy.

## What to do after the captures land

Post the INIs plus ground-truth sheets to the repo. Claude will:

1. Diff each INI against ground truth.
2. Flag any HIGH-confidence mismatches as gate failures.
3. Identify any unknown hardware for DB submission.
4. Update CHANGELOG with capture findings.
5. Update README claim hierarchy to match what was captured.

## Known things that might be interesting but are not ship-blockers

- Audio detection on a genuine AdLib (OPL2 only, no DSP) will produce
  `audio.detected=adlib` which the audio DB may or may not distinguish
  from SB1. Either outcome is fine; it's a DB coverage question.
- Pre-CPUID machines with uncommon FPU sockets (IIT 2C87/3C87) will
  land as generic `287` or `387`. CheckIt-style IIT discrimination is
  explicitly deferred to 0.9.0+; do not treat the lack of IIT tagging
  as a defect in 0.8.0.
- `upload.status=not_built` on every capture is correct for the 0.8.0
  stock build. Research builds (`wmake UPLOAD=1`) would emit a different
  value but are not the shipping artifact.

## Acceptance rollup for M1

| Item | Source |
|---|---|
| M1.1 Whetstone emit suppressed | Every capture INI: `whetstone_status=disabled_for_release`, no k_whetstones |
| M1.2 Upload compiled out | Every capture INI: `upload.status=not_built` |
| M1.3 Nickname leak fixed | Every capture INI: `nickname=<value>` verbatim |
| M1.4 cpu.class normalized | 486 capture: `cpu.class=486` not vendor string |
| M1.5 bench_cpu range widened | 486 capture: Rule 4b PASS (was WARN) |
| M1.6 DGROUP audit | `wmake dgroup-report` shows OK |
| M1.7 EMM386 crumb chain | Every capture: clean exit to DOS, no reboot |
| M1.8 Real-hardware captures | Four captures archived (or three + claim retraction) |
| M1.9 8088 / XT decision | Capture archived, or README claim retracted |

Once this table is all green, M1 exits and M2 (research-gap probes)
can begin.
