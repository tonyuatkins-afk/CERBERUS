# CERBERUS Hardware Database

This directory holds the human-editable source of truth for every hardware identification table in CERBERUS. Python build scripts in this directory regenerate `src/detect/*_db.c` files at build time.

## Files (populated in Phase 1)

- `cpus.csv` — CPU identification table (~80–120 entries at v0.2)
- `fpus.csv` — FPU identification table
- `video.csv` — video chipset BIOS-signature table
- `audio.csv` — audio chipset identification
- `bios.csv` — BIOS family identification
- `motherboards.csv` — motherboard OEM strings
- `build_*_db.py` — regeneration scripts

## Contributing

See `submissions/README.md` for the CERBERUS.UNK submission workflow.

## Why CSVs

Version-controllable, human-editable, and reviewable in a GitHub PR without C knowledge. One-line addition extends the database; the build script regenerates the C source. This is deliberately the lowest-friction extension point in the codebase.
