# BEK-V409 M1 exit-fix capture

Date: 2026-04-21 20:39
Machine: BEK-V409 (Intel i486DX-2-66, AMI BIOS 11/11/92, S3 Trio64, Vibra 16S, 63 MB XMS)
Binary: CERBERUS.EXE 163,838 bytes, MD5 ecf8ac21eb85479fea1b25fa86dd1bf2
Version: 0.8.0-M1 (with _exit fix applied)
Invocation: `CERBERUS /NICK:BEK-V409 /NOTE:0.8.0-M1-NOUI /O:C:\CAPTURE.INI`
Note: /NOUI flag was NOT actually passed (the value string "0.8.0-M1-NOUI"
  was the /NOTE argument); the interactive summary DID render.
Exit: clean to DOS prompt after user pressed Q. No reboot.

## M1 invariants verified

- `fpu.whetstone_status=disabled_for_release`, no k_whetstones (M1.1)
- `upload.status=not_built` (M1.2)
- `nickname=BEK-V409` verbatim (M1.3)
- `class=486`, `vendor=GenuineIntel` separate keys (M1.4)
- `cpu_ipc_bench=pass` at 1,855,287 iters/sec in the widened 1.5M-10.5M range (M1.5)
- `bios.date=11/11/92` clean (NO stomp this run)
- `audio.opl=opl3` primary probe succeeded (per-run-variable; prior run had opl=none)

## Post-exit symptom (known issue W6)

After program returned to DOS, console displayed:

    *** NULL assignment detected

Watcom's libc canary: the first 32 bytes of DGROUP's _NULL segment
were modified during program execution. Indicates a NULL-pointer
write somewhere in CERBERUS. Same class of bug as the intermittent
BIOS date stomp. Tracked for M2 investigation. See
`docs/quality-gates/M1-gate-2026-04-21.md` §W6 and `docs/methodology.md`
"Final-exit _exit bypass (M1.7 resolution)".

## Files

- `capture.ini` — the full CERBERUS.INI output (4,047 bytes, 141 results)
- `cerberus.las` — absent (clean exit via _exit + crumb_exit pair)
