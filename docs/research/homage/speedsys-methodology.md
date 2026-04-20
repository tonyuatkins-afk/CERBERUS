# SPEEDSYS, calibrated-reference system benchmark

**Tool:** SPEEDSYS (System Speed Test) v2.00 through v4.78
**Author:** Vladimir Afanasiev (Russia)
**Contact (per binary/history):** `dxover@email.ru`, homepage
`http://user.rol.ru/~dxover/speedsys`
**Source binary:** `Homage/SPEEDSYS/SPEEDSYS.EXE` (16-bit DOS
MZ, 97,767 bytes) plus `SPEEDSYS.HIS` (422-line version history)
**CERBERUS diff target:** methodology comparison for v0.5+
calibration + reference-machine rendering.

## Attribution correction from Phase 1 planning notes

Phase 1 planning materials had this tool attributed to Roedy
Green. Incorrect. Both the binary and the accompanying
`SPEEDSYS.HIS` changelog carry the explicit attribution:

```
Vladimir Afanasiev.

Home page: http://user.rol.ru/~dxover/speedsys
E-mail   : dxover@email.ru
```

Afanasiev is a Russian-language author; the early SPEEDSYS
versions (2.0x, 2.02, 2.05) have translation artifacts in
the changelog ("Eliminated starvation in an effort start on
80286 processors") that confirm a non-native English author.
Version 3.00's changelog line "Now this only English version
of SYSTEM SPEED TEST" indicates the tool previously shipped
with a Russian UI path. Attribution corrected for the record.

## What we were looking for

T9 and T10 asked whether SPEEDSYS's measurement methodology
or its detection techniques inform CERBERUS beyond what was
already extracted during Phase 1 from the `.HIS` file.

Phase 1 established that the `.HIS` was sufficient for the
top-level understanding CERBERUS needs. This phase does a
deeper read of the changelog for methodology hooks and a
strings-level scan of the binary for anything the changelog
did not surface.

## What we found

**SPEEDSYS is a calibrated-reference system benchmark,**
structurally similar to CheckIt (Phase 2 T1) but with a much
later reference machine and a far broader feature matrix.

**Reference machine (from `.HIS` v3.15):**

> Changed algorithm of determination of Memory Speed Index
> and leveled to 100.00 for EDO DRAM on the charge ASUSTEK TXP4
> rev1.02, processor Intel PentiumMMX(tm)- 200 MHz and
> installation BIOS by default.

So SPEEDSYS's Memory Speed Index of 100.00 is calibrated to a
Pentium MMX 200 on an ASUS TXP4 rev1.02 motherboard with EDO
DRAM at default BIOS. Every other machine SPEEDSYS tests gets
a Memory Speed Index relative to that anchor. Direct parallel
to CheckIt's IBM PC-XT anchor, about a decade later in the
timeline.

In v4.76 this index was **removed** and replaced by "Memory
Peak Bandwidth" (absolute MB/s). That's a methodology
evolution worth noting: relative-indices are display-convenient
but absolute-bandwidth numbers are more portable across time
and platform.

## Feature matrix spanning v2.00 to v4.78

The changelog is dense. Relevant category summary:

- **CPU detection.** Cyrix via "5/2 method" (v3.00), AMD K6 /
  K6-2 / K6-III, NexGen Nx586, Intel Celeron, PIII-M, Tualatin,
  P4 (v4.75). Pre-CPUID work includes 80286 startup-glitch
  workarounds (v3.00 "Eliminated starvation in an effort start
  on 80286 processors").
- **CPU clock detection.** v3.00 remade the frequency-
  determination procedure. v3.10 added 32-bit code path.
- **Clock multiplier decomposition.** Per-CPU multiplier
  detection for Intel P2/P3/P4, AMD Athlon/Duron/K6,
  VIA C3, etc. Documented as the v0.5+ candidate in
  `chkcpu-lessons.md`; SPEEDSYS is a second-opinion source.
- **Cache detection.** v2.02 changed the "algorithm of
  determination a caches of processor"; v3.15 changed cache
  size and speed determination for correct Pentium II behavior.
- **Memory detection.** v3.15 added detection of memory >64 MB.
  v3.15 also added DMI 2.0+ based memory-type detection.
  v4.75 added SPD reading for DDR-SDRAM modules with
  dictionary-compressed PCI database (the bank of known
  module manufacturer codes per JEP-106 standard).
- **SMBus support for SPD reading.** v3.15 added VIA VT8233,
  v4.74 added VT8235, v4.75 added Intel ICH2-M and VT8233C,
  v4.77 added nForce2, v4.78 added ICH5. Each SMBus host
  supported lets the tool talk directly to DDR DIMMs' EEPROMs
  to read their SPD (Serial Presence Detect) tables.
- **Storage detection.** IDE/ATA/ATAPI parameters, seek-time
  tests, read- and write-speed tests (full and quick
  variants).
- **Screenshots.** PCX file format via a home-written
  370-byte implementation (v3.10), replacing a 14 KB library
  from Genus Microprogramming's PCX Programmer's Toolkit.

## Consequence for CERBERUS

**Out of primary scope.** SPEEDSYS's feature matrix lives in
the Pentium through early-P4 range (v3.00 onwards require
Pentium-class CPU for most features). CERBERUS's 8088-486
target range only overlaps SPEEDSYS's coverage through the
pre-CPUID CPU detection. That overlap is already well covered
by CHKCPU (Phase 2 T12).

**Useful methodology parallels for CERBERUS v0.5+:**

1. **Calibrated Memory Speed Index.** The SPEEDSYS approach of
   "pick an anchor machine and present every result relative
   to that anchor" is the same mechanism as CheckIt's PC-XT
   rating. Both produce relative numbers that are display-
   convenient but age poorly. SPEEDSYS itself abandoned this
   in v4.76 in favor of absolute peak bandwidth. CERBERUS
   already emits absolute numbers; no regression to a
   relative index is worth considering.
2. **Per-test quick / full variants.** v3.10 added a /T2 switch
   that ran a faster but less thorough hard-disk test, with
   the note that "comparative results are removed for the full
   test." This is the same pattern as CERBERUS's `/Q` vs
   `/C:n` calibrated-mode split. Worth reviewing the
   comparative-results-omission policy for thermal tracking:
   long-run bench data might drop the PC-XT ratio emit for
   the same reason SPEEDSYS did.
3. **Dictionary compression for big databases.** v4.75 mentions
   a "dictionary-based compression of PCI database." CERBERUS
   currently ships 128 seed entries across 5 CSV databases,
   which uncompressed at about 15 KB of total static data in
   DGROUP. Compression is not needed at current scale; if
   CERBERUS ever wants a comprehensive PCI database (thousands
   of entries per PCIIDS.org), a similar dictionary-
   compression scheme would be the right answer.
4. **Non-reproducibility note.** v3.10 removed a conservation
   artifact in favor of an in-house implementation (PCX
   writer). The principle: keep dependencies tight. CERBERUS
   already follows this - every hardware database is a
   human-editable CSV with a Python build script rather than
   a third-party lib.

## Recommended action

**No changes to CERBERUS from this research.** SPEEDSYS is a
later-generation peer; its overlap with CERBERUS's 486-era
scope is minimal. Keep the `.HIS` as a methodology reference;
cite Afanasiev for the calibrated-reference concept. Filed
and closed.

If CERBERUS ever expands toward Pentium-era targets (v0.7+ per
the current roadmap intent), SPEEDSYS becomes a primary
cross-tool comparison candidate. Until then, CHKCPU covers the
CPU side and CheckIt / PCPBENCH / 3DBENCH cover the benchmark
side.

## Attribution

SPEEDSYS v2.00 through v4.78 (c) Vladimir Afanasiev. Freeware,
DOS real-mode MZ (no packer; plain MZ header, image size
97,767 bytes, header 32 bytes). Findings from byte-level scans
of the binary plus the 422-line `SPEEDSYS.HIS` changelog
(~16 KB). No code reproduced; `.HIS` quoted verbatim where
cited for version context.
