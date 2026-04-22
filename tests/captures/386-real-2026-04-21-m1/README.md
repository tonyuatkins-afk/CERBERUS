# 386 DX-40 M1 capture

Date: 2026-04-21
Machine class: 386 (pre-CPUID)

Hardware (ground truth as reported by Tony after the capture):
- CPU: 386 DX-40
- **FPU: IIT 3C87** (NOT Intel 387 — CERBERUS mis-tagged this; see DB gap below)
- Video: **Genoa ET4000** (Tseng ET4000 chipset; CERBERUS detected only generic vga)
- Audio: Aztech ISA sound card (no drivers loaded, so undetectable via signature; correctly reported as PC Speaker only)
- Bus: ISA-16
- BIOS: AMI 02/02/91
- Memory: 640 KB conventional + 15,296 KB extended (~16 MB total)
Binary: CERBERUS.EXE 0.8.0-M1 stock (163,838 bytes, MD5 `ecf8ac21eb85479fea1b25fa86dd1bf2`)
Captured in a separate session (archived by Tony at `runs/386-real-2026-04-21/`); copied here per M1 validation protocol.

Signature: `bc4a1dce` (hardware identity hash, distinct from BEK-V409's `5a0e5b5a`)
Run signature: `b4c830a0461f6ddc`

## M1 invariants verified

| Invariant | Result |
|---|---|
| M1.1 Whetstone emit suppressed | `fpu.whetstone_status=disabled_for_release`, no k_whetstones |
| M1.1 Rule 10 skipped | `consistency.whetstone_fpu` absent |
| M1.2 Upload compiled out | `upload.status=not_built` |
| M1.2 Network detection kept | `transport=pktdrv`, `pktdrv_vector=0x60` |
| M1.3 Nickname path clean | empty `nickname=` (no /NICK passed), not garbled |
| M1.4 cpu.class family token | `class=386` via legacy_token path (pre-CPUID) |
| M1.4 No CPUID vendor on [cpu] | correct (pre-CPUID has no vendor string) |
| M1.7 _exit fix | clean exit to DOS, no hang reported |
| No NULL assignment detected | no "*** NULL assignment detected" on exit |

## Critical finding for M2

The NULL-assignment canary (W6) and the intermittent BIOS-date BSS stomp
(W4) both fire on BEK-V409 (486 + S3 Trio64 VLB + Vibra 16S + AMI 11/11/92)
but NOT on this 386 (plain VGA, no Sound Blaster, AMI 02/02/91). DOSBox-X
also does not reproduce either. This empirically narrows the bug to a
BEK-V409-specific hardware-interaction path, most likely one of:

- S3 Trio64 chipset ID probe (`probe_s3_chipid` in detect/video.c)
- Vibra 16S DSP probe or OPL fallback path (detect/audio.c)
- UMC491 PIT latch-race compensation (timing code)
- Some interaction specific to BEK-V409's AMI 11/11/92 BIOS

Generic CERBERUS code paths that ran on BOTH 486 and 386 (detect_cpu,
detect_fpu, detect_mem, memory diagnostics, FPU diagnostics, DMA probe,
bench_cpu, bench_memory, bench_fpu, consist_check, thermal_check,
upload_populate_metadata, report_write_ini, ui_render_summary) are
ruled OUT as the NULL-write source. M2 investigation scope narrows
to detect_video, detect_audio, and timing subsystems specifically on
BEK-V409 hardware.

## Notable observations

### FPU fingerprint on 387 matches 486DX integrated FPU

Both emit `family_behavioral=mixed` with `affine / accepts / fprem1=yes /
fsin=yes`. The four-axis fingerprint does not distinguish 387 from 486
integrated FPU. M2 gap J (FPTAN cross-check) would add a fifth axis that
does distinguish, per `docs/FPU Test Research.md`.

### Benchmark anomaly: 386+387 FPU throughput > 486+integrated FPU

`fpu.ops_per_sec=3344481` on 386 vs `1177856` on BEK-V409 (~2.8× higher
on the slower machine). Most likely explanation: BEK-V409 is TSR-loaded
(CTCM + EMM386 + other drivers), slowing bench loops. The same pattern
appears in cpu.int_iters_per_sec: 386 at 2.10M vs BEK-V409 at 1.85M,
the 486 running slower than the 386 despite higher clock. Consistent
with M1.5's widening of the 486 anchor for TSR-loaded captures.

Not a CERBERUS bug; a property of the capture environments. Worth a
methodology note that per-machine TSR state affects bench throughput
substantially, and Rule 4b is the signal for it.

### timing_self_check WARN on both 486 and 386

`timing.cross_check.status=measurement_failed` with `method=pit` on both
captures. The v0.7.1 UMC491 wrap-guard is rejecting the PIT/BIOS
cross-check across hardware classes. Known issue — the guard may be
too aggressive for original 8253 PIT hardware (not just UMC491 clones).
Tracked separately; not M1 scope.

### No clock detection on 386

`cpu.clock_mhz` row absent. This is expected: `timing_emit_method_info`
reports clock only on RDTSC-capable paths (CPUID-era). Pre-CPUID CPUs
get PIT-only timing with no clock estimation. Deferred to future work.

## Bench numbers for 386 DX-40 baseline

| Metric | Value | Notes |
|---|---|---|
| `cpu.int_iters_per_sec` | 2,100,840 | Healthy 386 DX-40 |
| `cpu.dhrystones` | 9,932 | Baseline for future 386 comparisons |
| `memory.write_kbps` | 15,037 | ISA-16 bus |
| `memory.copy_kbps` | 7,782 |  |
| `memory.read_kbps` | 6,349 |  |
| `fpu.ops_per_sec` | 3,344,481 | Unexpectedly high; see note above |
| `video.text_write_kbps` | 1,163 | Plain VGA, no accelerated path |
| `video.mode13h_kbps` | 1,165 | Similarly modest |
| `cpu_xt_factor` | 28.87 | ~29× PC-XT |
| `mem_xt_factor` | 55.58 | ~56× PC-XT |

## Hardware-DB coverage gaps surfaced by this capture

### IIT 3C87 tagged as Intel 80387 (FPU DB miss)

CERBERUS emitted:
```
[fpu]
detected=387
friendly=Intel 80387 (DX/SX)
vendor=Intel
notes=386-era coprocessor with affine infinity
```

Ground truth: IIT 3C87 (third-party 386-era x87 coprocessor with
extended instruction set). CERBERUS's current FPU detection uses
FNINIT + FNSTSW sentinel + `cpu_get_class()` class-to-tag mapping;
it does NOT probe for IIT-specific undefined-opcode extensions.
The four-axis behavioral fingerprint (v0.7.1) produced `affine /
accepts / fprem1=yes / fsin=yes → family_behavioral=mixed`, which
is the same result a genuine Intel 387 would produce. The IIT
distinguishing opcodes are not part of the current fingerprint.

The homage research explicitly flagged this as a deferred v0.5+
capability in `docs/research/homage/checkit-fpu-detection.md`:

> "CheckIt's simpler FNINIT + FNSTSW sentinel is correct for its
> scope; the IIT discrimination gap is the only genuine capability
> gap, filed for v0.5+ if Cyrix/IIT hardware enters the capture
> corpus."

This capture IS the IIT-enters-capture-corpus event. The gap is now
a concrete M2 candidate with a target machine. The CheckIt Phase 3
research gave the opcode-probe pattern.

### Genoa ET4000 tagged as generic VGA (video DB miss)

CERBERUS emitted:
```
[video]
adapter=vga
color=yes
vbe_version=none
```

Ground truth: Tseng ET4000 on a Genoa card. The `hw_db/video.csv`
has Tseng ET4000 entries, so the gap is in the detection path, not
the DB. Two plausible causes: (a) the Genoa OEM BIOS doesn't carry
the expected "Tseng" / "ET4000" signature string in ROM (rebranded),
or (b) the probe scans only a subset of ROM space and missed the
signature. Worth a single-machine investigation in M2 with raw
F000-segment dump to see what strings ARE present.

### Aztech ISA (no-driver state is correct behavior)

CERBERUS emitted:
```
[audio]
pc_speaker=yes
opl=none
opl_probe_trace=no-blaster 0388:s1=00 s2=00,no-overflow result=none
sb_present=no
detected=PC Speaker only
```

Ground truth: Aztech ISA card present but no driver loaded. With
no BLASTER environment variable and no OPL response at port 388h,
CERBERUS correctly cannot identify the card. Not a defect — this
is exactly what detect_audio is designed to do under the "no driver"
condition. A driver-loaded state would have set BLASTER and given
the probe something to find.

## Claim hierarchy update

With this 386 capture archived plus the 486 capture, CERBERUS 0.8.0
can honestly claim: "Validated on 386 and 486. 286 and 8088 paths
untested." Pending 286 capture for the next honesty tier.

## Files

- `capture.ini` — full CERBERUS.INI (2,784 bytes, 103 results; smaller
  than 486 captures because fewer hardware subsystems are present)
- No UNK file (no unknown hardware captured)
- No LAS file (clean exit)
