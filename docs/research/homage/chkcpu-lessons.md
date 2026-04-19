# CHKCPU — CPU identification reference

**Tool:** CHKCPU 1.27.1 (02/02/2023) (`Homage/CHKCPU/CHKCPU.EXE`)
**Author:** Jan Steunebrink (`J.Steunebrink@net.HCC.nl`)
**Homepage:** `http://web.inter.nl.net/hcc/J.Steunebrink/`
**Runtime:** 16-bit DOS real-mode EXE + DPMI-aware protected
mode path
**License (per docs):** "freeware"
**Source language (per docs):** pure assembly

## Documentation-only read

CHKCPU ships with `CHKCPU.TXT`, a thorough ~20 KB author-written
reference that documents the full CPU/FPU matrix it handles.
Decompile was not required; the .TXT file is authoritative.

## What CHKCPU covers

CPU vendor/model detection across the full x86 lineage, with
explicit handling for:

- **Pre-CPUID CPUs:** 8086/8088, NEC V20/V30, 80186/80188,
  80286, 386 (SX/DX), 486 (SX/DX/DX2/DX4) **including
  non-Cyrix 486 without CPUID** via custom instruction timing /
  probe sequences (from changelog "Added support for 386 and
  (non-Cyrix) 486 CPUs which do not know the CPUID").
- **Cyrix via Device Identification Register (DIR):** the
  vendor-specific pre-CPUID model/stepping channel. Covers
  Cyrix 486 SLC/DLC, 5x86, 6x86 / 6x86MX / MII, MediaGX / GXm,
  plus IBM/TI-branded parts.
- **AMD / Intel / VIA / Transmeta / NexGen / UMC / IDT / Rise /
  SiS / DMP:** all CPUID-supporting families.
- **FPU class detection:** the 1.16-era changelog line "Added
  detection of an additional 8087, 80287, or 387 class FPU"
  confirms explicit FPU-family probing separate from CPU family.

Clock speed measurement: either internal Time Stamp Counter
(RDTSC) on CPUs that support it, or instruction timing fallback.
The `/I` switch forces instruction timing if RDTSC returns
nonsense; `/C` is "CPUID-only, skip measurement" for safe
mode/troubleshooting.

Clock multiplier + bus clock decomposition works in real mode
only — CHKCPU says so explicitly and the `/R` switch forces the
real-mode path when DPMI would otherwise be used.

Cache mode detection: the author reports L1 Write-Back vs
Write-Through state on 486, Cyrix 5x86/6x86, and AMD K-family
CPUs. CERBERUS does not currently report this.

## What CERBERUS currently does

`detect/cpu.c` + `cpu_a.asm` + `cpu_db.c` / `.csv`. Handles the
486-through-Pentium range via:
- Legacy-path probes (pre-CPUID 486 detection via opcode timing)
- CPUID path for CPU families 5+ (Pentium onward)
- Vendor string detection via CPUID leaf 0

`detect/fpu.c` + `fpu_a.asm` — FNINIT/FNSTSW sentinel probe as
documented in `checkit-fpu-detection.md`. No FPU-family
discrimination beyond the CPU-class inference.

`detect/cpu_db.h` / `.csv` — curated database of ~120 entries
per Phase 1 memory.

## Gap analysis

### Coverage

CHKCPU covers **every x86 CPU family shipped 1978-2022** per its
own changelog. CERBERUS covers 8088/V20/V30 → Pentium with
explicit scope focus on 486 era. No defect — CERBERUS's scope is
narrower by design, not accidental.

### Cyrix DIR handling

CHKCPU reads the Cyrix Device Identification Register for
vendor-specific pre-CPUID discrimination on Cyrix 486 SLC /
DLC / 5x86 / 6x86. CERBERUS's current `cpu.c` path for pre-CPUID
486s uses legacy token `486-no-cpuid` (seen at
`bench_cache.c:330`) without DIR read. **This is a capability
gap for Cyrix-specific models** — a Cyrix 486DLC on CERBERUS
tags as generic `486-no-cpuid`; CHKCPU would show it as e.g.
`Cyrix 486DLC-33GP` (this specific example appears in the
CHKCPU changelog).

Whether this matters: depends on whether Cyrix 486 hardware
enters CERBERUS's capture corpus. Not a v0.4 concern, not a
defect, just a scope difference that becomes material if the
project grows Cyrix-486 targets.

### Clock multiplier + bus clock

CHKCPU decomposes internal CPU clock into `multiplier × bus
clock`. CERBERUS `bench_cpu` reports internal clock only. Adding
the decomposition is low-effort (one MSR read on appropriate
CPUs, or clock-timing vs DRAM-timing comparison on pre-MSR
parts) but is out of scope for v0.4 calibration-focused work.

### Cache Write-Back / Write-Through detection

CHKCPU reports L1 cache mode. CERBERUS does not have a
`detect.cache.l1_mode` key today. There is a related path in
`bench_cache.c:7-17` — the small/large naming deliberately
avoids assuming write-back semantics, so CERBERUS is aware of
the distinction but does not *report* it. Adding a read of
CR0.CD / CR0.NW (and the Cyrix-specific CCR2 bits for
486/5x86/6x86, per CHKCPU's doc) is a small detect-layer
addition. Out of scope for v0.4.

### Instruction-set extension flags

CHKCPU reports MMX / 3DNow / SSE / SSE2 / SSE3 / etc. presence
via CPUID leaf 1 EDX/ECX. CERBERUS does not currently surface
these; capability reads exist in `detect/cpu.c` for internal use
(`cpu_class_t` selection) but aren't published as INI keys.
**This is a straightforward addition if the capture corpus ever
wants it** — no algorithmic challenge, just a report_add_str
sweep over the CPUID feature bits.

## Recommended action

**v0.4: none.** CHKCPU has richer coverage but CERBERUS's
narrower scope is intentional.

**v0.5+ candidates, prioritized by likely real-iron capture
need:**

1. **Cyrix DIR-based vendor identification** — closes the
   `486-no-cpuid` generic-tag gap on Cyrix 486 hardware.
   CHKCPU's DIR handling in `CHKCPU.TXT` is enough reference to
   implement without decompile.
2. **Instruction-set extension flags in detect output** — cheap
   win if capture-corpus users want to filter on MMX / SSE
   availability.
3. **Cache mode (WB/WT) reporting** — requires CR0 + Cyrix CCR2
   reads; documented in CHKCPU.TXT.
4. **Clock multiplier decomposition** — informative but not
   blocking any known CERBERUS feature.

None of these is urgent; CERBERUS's existing detect output is
sufficient for the BEK-V409 bench box and the typical 486-era
capture target. Filed for future reference.

**Optional decompile:** not needed. `CHKCPU.TXT` covers every
detection approach in enough detail to reimplement from scratch.
If CERBERUS ever decides to add one of the above features, the
author's documentation is the first source, then CPUID / DIR /
CCR2 official vendor datasheets. CHKCPU's binary would be a last
resort.

## Attribution

CHKCPU 1.27.1 (c) 1997-2022 Jan Steunebrink. Freeware, assembly-
language. All coverage claims above are from the author's own
`CHKCPU.TXT` documentation (~600 lines, self-contained
changelog + usage reference). No decompile performed; no code
reproduced.
