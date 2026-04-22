# BEK-V409 M1 second-run capture (historical, post-em-dash-fix, pre-_exit-fix)

Date: 2026-04-21 20:30
Machine: BEK-V409 (Intel i486DX-2-66, AMI BIOS 11/11/92, S3 Trio64, Vibra 16S, 63 MB XMS)
Binary: CERBERUS.EXE 163,822 bytes (em-dash-fix M1 stock build, pre-_exit-fix)
Invocation: `CERBERUS /NICK:BEK-V409 /NOTE:0.8.0-M1-NOUI /O:C:\CAPTURE.INI`
Note: "/NOTE:0.8.0-M1-NOUI" was the /NOTE VALUE, not a /NOUI flag.
  Interactive summary did render. User pressed Q to exit.
Exit: HUNG after Q. Required hardware reset.

## Why this capture is preserved

This is the capture that produced the definitive diagnostic:
`CERBERUS.LAS=main.return`. That's the intentional final crumb with
NO paired `crumb_exit()`. Its survival on disk after the hang
proves the hang is AFTER the `return exit_code` statement in main,
in Watcom's libc exit teardown (atexit, FPU state, stdio close,
_NULL signature check).

This diagnostic drove the M1.7 fix: replace `return exit_code`
with `crumb_exit() + _exit((int)exit_code)` to bypass Watcom's
libc teardown. See `src/main.c` end-of-main and
`docs/methodology.md` "Final-exit _exit bypass".

## M1 invariants observable in the capture

- All M1.1-M1.5 invariants present and clean (this build had the
  em-dash fixes applied, so `[bios] date=11/11/92` is correct this run)
- `audio.opl=opl3` (primary probe path succeeded, shorter probe trace)
- No BIOS date stomp this run - path-dependent stomp narrative aligns
- `run_signature=c195f441a7999853` complete in INI (report_write_ini
  ran to completion before the hang)

## Files

- `capture.ini` - the INI on disk (4,047 bytes, 141 results)
- `cerberus.las` - the crumb trail, contents: "main.return"
