# BEK-V409 M1 first-run capture (historical, pre-fix)

Date: 2026-04-21 18:42
Machine: BEK-V409 (Intel i486DX-2-66, AMI BIOS 11/11/92, S3 Trio64, Vibra 16S, 63 MB XMS)
Binary: CERBERUS.EXE 163,838 bytes (pre-em-dash-fix, pre-_exit-fix stock M1 build)
Invocation: `CERBERUS /NICK:BEK-V409 /NOTE:0.8.0-M1-baseline /O:C:\CAPTURE.INI`
Exit: HUNG after audio_scale visual. Required hardware reset.

## Why this capture is preserved

Historical evidence of the M1.7 end-of-run hang class on real iron.
The INI is fully written (`run_signature=c579a4d82a7a2aaa` at EOF) so
`report_write_ini` completed. Hang is downstream, in the post-INI
path. See `CERBERUS.LAS` trail via next-boot inspection; the exit-fix
commit replaced `return exit_code` with `_exit()` to bypass Watcom's
libc teardown.

Two artefacts worth noting in this INI:

### [bios] dree=    92 — BIOS date BSS stomp

Line 72 shows `dree=    92` instead of `date=11/11/92`. The 4-byte
key "date" got stomped to "dree"; the value "11/11/92" had its
first 6 characters replaced with one 0xFF-ish byte plus 5 spaces.

Path-dependent: the subsequent noui capture had `opl=opl3` and this
run had `opl=none` (OPL probe fell back). Different probe paths =
different BSS layout patterns = different stomp victim.

### Em-dash UTF-8 bytes in display (this binary only)

This build was the last to contain em-dash bytes `0xE2 0x80 0x94`
in string literals; they rendered as `ΓÇö` on the CP437 screen.
Fixed in the subsequent build (audio_scale.c, timing_metronome.c,
diag_bit_parade.c, bench_cache_char.c, unknown.c, cpu.c).

## M1 invariants observable in the capture

- `fpu.whetstone_status=disabled_for_release` (M1.1)
- `upload.status=not_built` (M1.2)
- `upload.nickname=BEK-V409` verbatim (M1.3)
- `cpu.class=486`, `cpu.vendor=GenuineIntel` separate (M1.4)
- `cpu_ipc_bench=pass` at 1,964,636 (M1.5 widened anchor)

## Files

- `capture.ini` - the INI on disk (3,892 bytes, 137 results)
