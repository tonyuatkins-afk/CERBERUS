# CERBERUS

**Retro PC System Intelligence Platform.** A unified DOS-native detection, diagnostic, and benchmark tool for IBM PC/XT through 486-class systems. Three heads, one answer: **what your hardware is**, **whether it's working**, and **how fast it actually is**.

Part of the [Barely Booting](https://barelybooting.com) / NetISA ecosystem.

## Status

**v0.1 (Skeleton).** Architecture and module layout complete. Timing, CPU detection, display, INI reporter, and BIOS info implemented. Memory detection partial. All other modules are structured stubs.

See [CERBERUS.md](CERBERUS.md) for the full master specification: architecture, methodology, three-head breakdown, consistency engine, NetISA upload path, target hardware matrix, and release plan.

## What makes it different

- **Not a toy benchmark.** Measures subsystems independently — no composite "score."
- **Every result carries a confidence level and the method used.**
- **The consistency engine cross-checks detection against diagnosis against benchmark.** When hardware disagrees with what it claims to be, CERBERUS says so.
- **Floor target is an 8088 with 256KB and MDA.** Scales to 486+VGA+FPU, never requires it.
- **First DOS tool designed from day one to integrate with NetISA** — TLS 1.3 results upload from real-mode DOS over WiFi.

## Target hardware

| Class | CPU | RAM | Display | Bus |
|-------|-----|-----|---------|-----|
| Floor | 8088 / 8086 / V20 / V30 | 256KB | MDA | ISA 8-bit |
| Common | 286 / 386SX / 386DX | 640KB | CGA / Hercules / EGA | ISA 16-bit |
| Ceiling | 486SX / DX / DX2 / DX4, RapidCAD, Cyrix, AMD | 4MB+ | VGA | VLB / PCI |

## Build

Requires [Open Watcom C/C++](http://open-watcom.github.io/) and [NASM](https://www.nasm.us/). From the project root:

```
wmake
```

Produces `CERBERUS.EXE` (DOS real-mode, target under 64KB).

## License

MIT. See [LICENSE](LICENSE).

## Author

Tony Atkins — [Barely Booting](https://barelybooting.com) — [@tonyuatkins-afk](https://github.com/tonyuatkins-afk)

---

*"What your hardware actually is, whether it's working, and where the bottlenecks are. Three heads. One answer."*
