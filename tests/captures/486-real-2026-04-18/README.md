# 486 real-hardware run corpus — 2026-04-18

Six `CERBERUS.INI` captures from the first full real-iron validation session on the bench box. Each file is a diffable point-in-time snapshot of what the tool produced at that iteration step.

## Bench hardware

- Motherboard: BEK-V409 (UMC491 chipset, ISA + VLB)
- CPU: Intel i486DX-2 66 (family 4, model 3, stepping 5)
- RAM: 64 MB (63,076 KB extended)
- Video: S3 Trio64 on VLB
- Audio: SB Vibra 16S PnP (CT2800/2900 class, DSP 4.13)
- NIC: 3Com 3C509 (ISA)
- BIOS: AMI 11/11/92

## Per-run narrative

- **v1.ini** — First real-iron run, zero real-hardware fixes. `extended_kb=0`, video mis-detected as `IBM VGA`, `sb_present=no`, PIT/BIOS timing 49% divergent. Every bug the emulator couldn't reproduce surfaced on contact.
- **v2.ini** — After `eeba319`. HIMEM XMS-entry-point fix lands (ext=63,076). S3 CR30 chip-ID probe lands. Vibra DSP v4.13 read correctly from base+0x0E. Audio T-key still unmatched; OPL still `none`.
- **v3.ini** — Stabilization. `extmem_cpu=pass` appears. CPU bench drops 8.4M → 2.2M iters/sec — CTCM PnP driver stealing cycles, not a regression.
- **v4-turbo.ini** — Audio DB mis-hits `Sound Blaster AWE64 (CT4500)` on a Vibra. Timing guardrails now surface `measurement_failed` instead of the biased 49% WARN.
- **v5-hung.ini** — After `7e4bdcb`. AWE64 misattribution fixed. Rule 4b (`cpu_ipc_bench`) WARN fires correctly at 1.96M below 4.7M floor. **INI is complete** (2,124 bytes, closing `run_signature`) — the hang was in UI alert-box rendering *after* `report_write_ini` returned. CTRL-ALT-DEL required.
- **v6-noui.ini** — Same build + `/NOUI`. Bytewise-identical shape to v5-hung, microsecond-level bench variance. Returned to DOS cleanly — confirms the hang is UI-phase, not exit-path.

## Worked diff: v1 → v3 (HIMEM extended-memory fix landing)

Key lines from `diff v1.ini v3.ini`:

```
< extended_kb=0            (v1: HIMEM intercepts INT 15h, BIOS returns 0)
> extended_kb=63076        (v3: XMS entry point via INT 2Fh AX=4310h)

< vendor=IBM                (v1: BIOS scan matched "IBM VGA" string)
< chipset=IBM VGA
> vendor=S3 Incorporated    (v3: S3 CR30 chip-ID probe authoritative)
> chipset=S3 Trio64

< sb_present=no             (v1: probe_sb_dsp read wrong port)
> sb_dsp_version=4.13       (v3: polled base+0x0E status register)
```

The 49% PIT/BIOS WARN persists through v3 — that's the 8254 latch race, not fully landed until `eeba319` → refined in `6c3a023`.

## Secrets

None to strip. Captures contain only hardware-detection output. No WiFi (3C509 wired only), no hostnames, no user data.
