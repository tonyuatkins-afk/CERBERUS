# TOPBENCH — database model and community contribution pattern

**Tool:** TOPBENCH (`Homage/TOPBENCH/TOPBENCH.EXE`)
**Author:** Jim "Trixter" Leonard (`trixter@oldskool.org`)
**Runtime:** 16-bit DOS MZ + PKLITE packed (not unpacked for
this read — docs + database were sufficient)
**Database:** `Homage/TOPBENCH/DATABASE.INI`, plain-text INI

## Documentation-only read

No decompile. The header comment in DATABASE.INI is
self-documenting:

> This file contains fingerprinting information about various
> computers for the TOPBENCH benchmark. Hand-edit at your peril!
> If you add new systems, please consider sending this file to
> trixter@oldskool.org.

This establishes the tool as **community-contribution-driven**.
The corpus grows by users submitting their machines' scores.

## What TOPBENCH measures (per in-binary strings)

Five numeric tests per system:

- `MemoryTest`
- `OpcodeTest`
- `VidramTest` — video RAM throughput
- `MemEATest` — effective-address memory test
- `3DGameTest`

Plus a rolled-up `Score` (appears to be a normalized composite).

## Fingerprinting data structure

Each machine is a separately-keyed INI section. The UID combines
a serial number with a CRC16 of the BIOS string, binding the
record to the exact BIOS ROM:

```
[UID7F5C71D]
MemoryTest=5926
OpcodeTest=3584
VidramTest=3373
MemEATest=4392
3DGameTest=3490
Score=2
CPU=Intel 8088
CPUspeed=4.77 MHz
BIOSinfo=COPR. IBM 1981,1983 (06/01/83, rev. 86)
BIOSdate=19830601
BIOSCRC16=7F5C
VideoSystem=CGA
VideoAdapter=IBM PCjr
Machine=IBM PCjr
Description=Stock, 128KB RAM. ...
Submitter=trixter@oldskool.org
```

Every key is human-readable. `BIOSCRC16` is the identity
fingerprint; `BIOSinfo` and `BIOSdate` are the human-readable
echo of what that CRC represents.

## Relevance to CERBERUS

### Reference numbers for issue #6 (slow VLB bandwidth)

The database contains multiple 486-class machines with VLB video.
A scan for "VLB" / "VL-Bus" / "Trio" in DATABASE.INI surfaces the
following near-matches to CERBERUS's bench box context:

- **486 DX2-66 clone with VLB Cirrus CL-GD5422, 128 K L2 cache,
  20 MB RAM.** Score 109; `VidramTest` and other numbers recorded.
  This is the closest single record to CERBERUS's issue #6 subject
  (486 DX-2, VLB video). Video chip differs (Cirrus, not Trio64),
  but bus class matches.
- **Am486 DX4 + S3 Trio 64 + 2 MB EDO** (Akhter P150, MS-4144
  mobo). Likely PCI not VLB based on mobo generation, but
  documents S3 Trio 64 throughput on 486-class silicon.
- **486 DX4-100 (Asus VL/I-486SV2GX4) + Number Nine GXE64 / S3
  Vision864, 2 MB, VLB.** 486 DX4 on VLB, different S3 part.

**There is no exact 486 DX-2 + S3 Trio 64 VLB record in the
database.** The nearest hops are DX2 + Cirrus VLB, DX4 + S3 Trio
(likely PCI), or DX4 + S3 Vision VLB. Any two of those bracket
the missing combination but don't replace it.

**Empirical implication:** for issue #6, TOPBENCH gives CERBERUS
a set of *neighboring* VLB 486 numbers to compare CERBERUS's own
`bench_video` output against — not a direct equivalence. If
CERBERUS captures a BEK-V409-class machine and the VidramTest
equivalent is dramatically below the Cirrus VLB entry, that's
evidence the slow-VLB behavior is non-reproducible and the
chipset / memory-timing explanation is more likely; if it lands
near the Cirrus VLB number, slow-VLB is likely typical for the
era and CERBERUS is measuring reality.

### DCE-safety concern

No visible indication in the database or binary strings that
TOPBENCH uses a Watcom-style CFLAGS_NOOPT defense for DCE in its
inner loops. The same class of measurement error that CERBERUS's
`bench_cache` module guards against (compiler elimination of
"dead" read/write kernels) would affect any TOPBENCH build
compiled with optimizing flags. TOPBENCH is ~16-bit DOS so it
probably uses Turbo C or Borland C — whose DCE aggressiveness
is well below Watcom's `-ox`. This is not proof of safety; it is
absence-of-evidence of the concern.

### Contribution model

TOPBENCH's data format is **machine-readable but still INI**
(human-scannable, hand-editable, grep-friendly). CERBERUS's
capture INI format (the result tables emitted by
`report_add_*`) is the same shape. A CERBERUS → TOPBENCH
converter — mapping `bench.cache.small_read_kbps`,
`bench.video.text_mode_cps`, etc. to TOPBENCH's
`MemoryTest` / `OpcodeTest` / `VidramTest` — is a small
script, not a rewrite. If the CERBERUS project wants to
contribute real-iron captures back to the retro-computing
community, TOPBENCH's database is a natural home.

## Recommended action

**None for v0.4.** Contributor-pattern note filed for later
discussion. TOPBENCH's scoring methodology is not compatible
with CERBERUS's `bench_*` keys 1:1, so contribution would
require a mapping document and a minimum of per-machine hand
verification.

**Opportunistic:** if and when CERBERUS captures a BEK-V409
`bench_video` number that's anomalously slow per issue #6, the
Cirrus CL-GD5422 VLB record in TOPBENCH is the closest neighbor
for sanity-checking.

## Attribution

TOPBENCH by Jim "Trixter" Leonard, `trixter@oldskool.org`.
Database is user-contributed, format described in its own header
comment. No code reproduced; all observations are from the INI's
self-documentation and embedded binary strings.
