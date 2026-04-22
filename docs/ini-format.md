# CERBERUS.INI format reference

**Scope**: Every section, every key, every value type emitted by
`report_write_ini()`. This is the machine-readable contract paired with
`docs/ini-upload-contract.md`.

**Format version**: `ini_format=1` (v0.7.0 baseline). Additive changes
(new keys, new sections) do not bump this. Breaking changes would,
and will not happen during the v0.x.x series.

## File structure

Standard Windows INI syntax:

- Sections: `[name]` on its own line.
- Key-value: `key=value`, one per line. No surrounding spaces.
- Comments: none (CERBERUS doesn't emit them; parsers may tolerate `;`
  or `#` line prefixes if desired).
- Line endings: the file is written with the C stdio runtime's native
  line ending (CRLF on DOS). Parsers should accept LF as well.
- Character encoding: ASCII only. No UTF-8, no CP437 glyphs. Values are
  bounded to the printable subset `0x20..0x7E`.

## Sections in emit order

Exact order is defined in `section_order[]` in `src/core/report.c`. The
server SHOULD parse sections in any order (INI semantics), but for
human readers the file layout is stable:

1. `[cerberus]` — run + build metadata
2. `[network]` — transport detection
3. `[environment]` — emulator / virtualization indicators
4. `[cpu]` — CPU detection
5. `[fpu]` — FPU detection
6. `[memory]` — memory map
7. `[cache]` — cache detection
8. `[bus]` — system bus
9. `[video]` — video hardware
10. `[audio]` — audio hardware
11. `[bios]` — BIOS identification
12. `[diagnose]` — per-subsystem diagnostic verdicts
13. `[bench]` — benchmark numbers
14. `[consistency]` — rule engine verdicts
15. `[upload]` — user metadata + post-submission state

Trailing `run_signature=<hex>` line (outside any section) closes the
file. The run signature is a SHA-1 (16-hex-char prefix) over the entire
file excluding the `run_signature=` line itself.

## [cerberus]

Mandatory. Written first.

| Key | Type | Example | Meaning |
|-----|------|---------|---------|
| `version` | string | `0.7.0` | CERBERUS client version (SemVer). |
| `schema_version` | string | `1.0` | Detection-schema version. Independent of `ini_format`. |
| `signature_schema` | string | `1` | Hardware-signature canonical-key-set version. |
| `ini_format` | integer | `1` | THE server parser switch. See `ini-upload-contract.md`. |
| `mode` | enum | `quick` or `calibrated` | Run mode. |
| `runs` | integer | `1` | Number of benchmark passes. |
| `signature` | 8-char hex | `a1b2c3d4` | Hardware-identity SHA-1 prefix. |
| `results` | integer | `87` | Number of result rows emitted below. |

## [network]

v0.7.0. Reports what network transport CERBERUS detected at startup.

| Key | Value | Meaning |
|-----|-------|---------|
| `transport` | `netisa` | NetISA custom card via INT 63h API (reserved for v0.8.0 TLS). |
| `transport` | `pktdrv` | Packet driver found at INT 60h-7Fh scan. |
| `transport` | `mtcp` | mTCP detected via `MTCP_CFG` env var. |
| `transport` | `wattcp` | WATTCP detected via `WATTCP` env var. |
| `transport` | `none` | No transport found. CERBERUS works offline. |

Absence of the `[network]` section in pre-v0.7.0 submissions means
`transport=none` semantically.

## [environment]

Emulator and virtualization detection.

| Key | Type | Values | Meaning |
|-----|------|--------|---------|
| `emulator` | string | `none`, `dosbox-x`, `86box`, `pcem`, etc. | Detected emulator name. `none` = real hardware. |
| `virtualized` | enum | `yes`, `no`, `unknown` | Virtualization indicator. |
| `confidence_penalty` | enum | `none`, `partial`, `full` | How much to discount measurements from emulated hosts. |

## [cpu]

| Key | Type | Meaning |
|-----|------|---------|
| `detected` | string | Human-readable CPU model. |
| `family_model_stepping` | `F/M/S` | CPUID leaf-01h family/model/stepping. |
| `class` | enum | CPU class: `8086`, `v20`, `286`, `386`, `486`, `pentium`, etc. |
| `bench_iters_low` | integer | Low bound of expected bench.cpu range for this model. |
| `bench_iters_high` | integer | High bound. |

(Plus additional per-vendor keys; see `src/detect/cpu.c` for the full set.)

## [fpu]

| Key | Type | Meaning |
|-----|------|---------|
| `detected` | string | FPU family: `none`, `287`, `387`, `integrated`, `iit`. |
| `friendly` | string | Human-readable FPU description. |

## [memory]

| Key | Type | Meaning |
|-----|------|---------|
| `conventional_kb` | integer | Conventional memory KB (0-640). |
| `extended_kb` | integer | Extended memory KB. |

## [cache]

| Key | Type | Meaning |
|-----|------|---------|
| `present` | enum | `yes`, `no`, `unknown`. |

## [bus]

| Key | Type | Meaning |
|-----|------|---------|
| `class` | enum | `isa8`, `isa16`, `vlb`, `pci`, `unknown`. |

## [video]

| Key | Type | Meaning |
|-----|------|---------|
| `adapter` | enum | `MDA`, `CGA`, `EGA`, `VGA`, `MCGA`, etc. |
| `chipset` | string | Detected chipset name if known. |

## [audio]

| Key | Type | Meaning |
|-----|------|---------|
| `detected` | string | Audio card friendly name. |
| `sb_present` | enum | `yes`, `no`. |
| `sb_dsp_version` | string | e.g. `4.04`. |
| `opl` | enum | `opl2`, `opl3`, `none`. |

## [bios]

| Key | Type | Meaning |
|-----|------|---------|
| `family` | string | e.g. `Award Modular`, `Phoenix`, `AMI`. |
| `date` | string | mm/dd/yy. |

## [diagnose]

Per-subsystem diagnostic verdicts. Keys follow `diagnose.<subsys>.<test>`
pattern.

Example:
```
cache.status=pass
dma.summary=pass
cpu.alu=pass
```

## [bench]

Benchmark numbers.

```
cpu.int_iters_per_sec=1964636
cpu.dhrystones=32131
fpu.ops_per_sec=1119820
fpu.whetstone_status=disabled_for_release
memory.write_kbps=15384
memory.read_kbps=16260
memory.copy_kbps=7220
cpu_xt_factor=93.4000
mem_xt_factor=51.5700
fpu_xt_factor=16.6600
```

## [diagnose] FPU fingerprint rows (M2 additions)

Per 0.8.0 plan §6 M2, five new keys join the behavioral fingerprint
(v0.7.1's `infinity_mode` / `pseudo_nan` / `has_fprem1` / `has_fsin`):

```
fpu.fptan_pushes_one=yes|no
fpu.rounding_nearest=2,-2
fpu.rounding_down=1,-2
fpu.rounding_up=2,-1
fpu.rounding_truncate=1,-1
fpu.rounding_modes_ok=yes|no
fpu.precision_modes_ok=yes|no
fpu.exceptions_raised=N_of_6
```

- `fptan_pushes_one` (M2.2, research gap I): 387+ always pushes 1.0
  onto ST(0) after FPTAN; 8087/287 leaves cos(θ)-ish denominator. Adds
  a fifth behavioral axis to `family_behavioral` inference.
- `rounding_nearest/down/up/truncate` (M2.3, gap J): FISTP(1.5) and
  FISTP(-1.5) under each RC mode, as "pos,neg". The four expected
  pairs per IEEE-754: (2,-2), (1,-2), (2,-1), (1,-1). All distinct.
- `rounding_modes_ok`: yes if all four modes match the IEEE table.
  Tagged VERDICT_PASS or VERDICT_WARN accordingly.
- `precision_modes_ok` (M2.4, gap K): yes if 1.0/3.0 computed under
  PC=single / PC=double / PC=extended produced three bytewise-distinct
  10-byte extended results (proves PC actually changes precision).
- `exceptions_raised` (M2.6, gap M): count of x87 exceptions (out of
  6: IE/DE/ZE/OE/UE/PE) that raised their expected status-word bit
  when deliberately triggered. Healthy FPU: `6_of_6`.

## [diagnose] memory rows (M2 additions)

Per 0.8.0 plan §6 M2 + QA-Plus homage pattern:

```
memory.checkerboard=pass|fail
memory.inv_checkerboard=pass|fail
```

Catches adjacent-cell coupling faults (bit-line shorts, row/column
decoder leaks) that walking-1s/0s miss. 0x55/0xAA and 0xAA/0x55
alternating patterns on the DGROUP-resident 4 KB diagnostic buffer.

## [bench] cache.char stride sweep extended (M2.1)

```
cache.char.stride_128_kbps=<N>
```

Added to the stride sweep to enable inference of line=32 (Pentium)
and line=64 (Pentium Pro+). The `line_bytes` inference function
n_strides cap raised from 5 to 6 accordingly; pre-M2 stride sweeps
with 5 points still work (backward compatible via internal cap).

### `fpu.whetstone_status` enum

- `disabled_for_release` — stock 0.8.0 build; Whetstone kernel is compiled but emit is suppressed. No `fpu.k_whetstones` row. See `docs/methodology.md` "Why Whetstone is not in 0.8.0".
- `ok` — research build (`wmake WHETSTONE=1`) where the kernel ran cleanly. `fpu.k_whetstones` row present.
- `skipped_no_fpu` — FPU absent; kernel never ran. Only emitted by research builds.
- `inconclusive_elapsed_zero` / `inconclusive_sub_ms` / `inconclusive_sub_kwhet` — research build where the kernel ran but the measurement was below the usable threshold. `VERDICT_WARN` attached.
- `inconclusive_runtime_exceeded` — research build where the kernel exceeded the 30 sec wall-clock cap and was aborted.

Consistency Rule 10 (`whetstone_fpu`) skips when status is `disabled_for_release` or any `inconclusive_*`.

## [consistency]

Consistency-rule verdicts. Keys follow `consistency.<rule_name>` with
values being the narration string (including the `pass (...)` / `WARN:`
/ `FAIL:` prefix).

Example:
```
cpu_ipc_bench=pass (bench within expected range for this CPU)
timing_self_check=pass (PIT C2 and BIOS tick agree within 15%)
```

Forensic sub-keys (v0.5.0+):
```
cpu_ipc_bench.measured=1964636
cpu_ipc_bench.expected_low=4700000
cpu_ipc_bench.expected_high=10500000
```

## [upload]

v0.7.0. User metadata + upload outcome.

| Key | Type | Source | Meaning |
|-----|------|--------|---------|
| `nickname` | string (max 32) | `/NICK` flag | User-provided nickname. Empty if not set. |
| `notes` | string (max 128) | `/NOTE` flag | User-provided note. Empty if not set. |
| `status` | enum | client | See status values below. |
| `submission_id` | 8-char hex | server response | Populated on `uploaded` only. Absent / empty on every other status. |
| `url` | string | server response | Public view URL. Populated on `uploaded` only. Absent / empty otherwise. |

**`status` values** (exhaustive; client may emit any one of these):

| Value | When | Terminal? |
|---|---|---|
| `not_built` | 0.8.0 stock build — runtime upload compiled out. No transmission attempted. | Yes |
| `uploaded` | 200 received, 2-line response parsed, `submission_id` + `url` populated (research build only) | Yes |
| `offline` | `network.transport=none` — no network detected, no POST attempted (research build only) | Yes |
| `skipped` | User declined the Y/n prompt, or `/NOUPLOAD` was passed (research build only) | Yes |
| `no_client` | Network detected but `HTGET.EXE` not on PATH (research build only) | Yes |
| `failed` | HTGET invoked, non-zero exit (research build only) | Yes |
| `bad_response` | HTGET returned 0, but the body wasn't the contract-specified 2 lines (research build only) | Yes |

The `[upload]` section is always emitted (even when all fields are
empty / `not_built`) so the server parser can treat its absence as a
client-error signal. The server parser MUST tolerate the full enum
above; unrecognized values should be logged but not rejected.

**Build gating (v0.8.0 change):** stock binaries compile runtime
upload out (`#ifdef CERBERUS_UPLOAD_ENABLED` gates the HTGET shell-out
and `upload_execute` body). Stock builds emit `status=not_built` and
never attempt transmission. Research builds (`wmake UPLOAD=1`) enable
the full flow and emit any of the remaining status values. See
`docs/methodology.md` for the 0.8.0 doctrine.

## Trailer: `run_signature=<hex>`

SHA-1 over the entire INI contents excluding the `run_signature=` line
itself. 40-char full digest. Distinguishes two runs on the same
hardware: same `signature` + different `run_signature` = repeat run.

## Backward compatibility commitment

From v0.7.0 forward:

- **Never rename a key.** `cpu.detected` stays `cpu.detected` forever.
- **Never change a value format.** If `bench.cpu.dhrystones` is an
  integer in v0.7.0, it's an integer in every subsequent version.
- **Never remove a key.** A key may become deprecated (stop being
  populated) but the server MUST tolerate its absence.
- **Adding new keys or sections is always OK** and does not bump
  `ini_format`.
- **Breaking changes** (if ever needed) bump `ini_format` and run in
  parallel with the old format at the server.
