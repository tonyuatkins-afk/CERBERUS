# CERBERUS Measurement Methodology

This document records the measurement method, confidence rationale, and known failure modes for every result CERBERUS reports. Added to as each Phase 1 / 3 subsystem is implemented.

## Timing

- PIT Channel 2 gate-based measurement, ≈838ns resolution
- `timing_ticks_to_us` uses `unsigned long` throughout to avoid the 16-bit overflow trap
- Rollover compensation: PIT counts DOWN, so `stop > start` indicates wrap
- XT-clone PIT C2 sanity probe runs at `timing_init()`; fallback to BIOS tick counter flags all results LOW confidence

## Pending subsystems

- CPU detection (Task 1.1)
- FPU detection (Task 1.2)
- Memory detection (Task 1.3) — INT 15h AX=E820h deliberately excluded (ACPI-era, not on pre-Pentium BIOSes)
- Cache detection (Task 1.4)
- Bus detection (Task 1.5)
- Video detection (Task 1.6)
- Audio detection (Task 1.7)
- BIOS info (Task 1.8)
