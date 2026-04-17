# Contributing to CERBERUS

## Coding conventions

Follow the DOS development conventions:
- Small/medium memory model only (no 32-bit protected mode)
- Explicit stack size via `option stack=` in Makefile
- Prefer `unsigned long` over `int` for any arithmetic that might exceed 16 bits
- CR+LF line endings on any file shipped to DOS
- 8.3 filenames for anything that might land on a FAT volume

## Hardware database contributions

The fastest way to contribute is adding a row to `hw_db/*.csv`:
- `hw_db/cpus.csv` — CPU signatures and friendly names
- `hw_db/fpus.csv` — FPU types
- `hw_db/video.csv` — video chipset BIOS signatures
- `hw_db/audio.csv` — audio chipset identification
- `hw_db/bios.csv` — BIOS family identification
- `hw_db/motherboards.csv` — motherboard OEM strings

If CERBERUS didn't recognize your hardware, it wrote `CERBERUS.UNK` — attach that to a GitHub issue using the "hardware submission" template.

## AI assistance disclosure

Per the policy in CERBERUS.md §14: every commit that used AI assistance carries an `Assisted-by: AGENT:MODEL` tag. The human author bears full responsibility for every line committed.

## Pull requests

1. Fork and branch from `main`
2. Run `wmake` — ensure the build is clean
3. Add/update host-side unit tests for any new pure-logic code
4. Run the Phase 1 gate checklist if your change touches detection
5. Include a one-line rationale in the commit message (the "why")
