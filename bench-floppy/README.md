# CERBERUS bench floppy

Self-contained content to drop onto a bootable DOS floppy so any vintage bench machine can run CERBERUS end-to-end from a single disk. Two variants:

- **`offline/`** — no network. Fits 360 KB (Leading Edge XT), 720 KB, 1.2 MB, 1.44 MB. ~305 KB with DOS boot files. Captures land on `A:\CAP.INI` and `A:\CAP.CSV`; read them on a modern PC after pulling the floppy.
- **`network/`** — with packet driver + mTCP drop-in slots. Fits 720 KB and up (does NOT fit 360 KB). Adds optional FTP upload-by-hand after capture, and readies for the 0.9.0 upload-path revival.

## Which to use

| Bench machine | Recommended floppy | Format |
|---|---|---|
| Leading Edge Model D (XT, 5.25" DD) | offline | 360 KB |
| 286 AT-class (5.25" HD) | offline or network | 1.2 MB |
| 386 DX-40 (3.5" HD) | network (already configured though) | 1.44 MB |
| IBM 486 (3.5" HD) | network (best for upload) | 1.44 MB |
| BEK-V409 486 (3.5" HD) | network (already configured though) | 1.44 MB |

## How to make the floppy

### Option A: Rufus (simplest on Windows)

1. Download [Rufus](https://rufus.ie).
2. Insert a formatted floppy into your USB floppy drive.
3. Rufus → "Create a bootable disk using" → FreeDOS.
4. Once Rufus finishes, copy the contents of `offline/` or `network/` onto the drive via Explorer.

### Option B: Existing DOS boot disk

1. Format a floppy from a DOS machine (or DOSBox-X) with `format a: /s`.
2. Copy the contents of `offline/` or `network/` onto the floppy.

### Option C: DOSBox-X `imgmake`

```
dosbox-x -c "imgmake A.IMG -t fd_360 -fs fat" -exit
```

Then mount the image via `imgmount` and copy the files. For bootable you'll need to install a DOS boot sector; Rufus is simpler.

## Auto-run contents

On boot:

1. `AUTOEXEC.BAT` runs.
2. Network variant: `NET\NET.BAT` loads a packet driver (if configured) and runs DHCP.
3. `MENU.BAT` offers five session choices:
   - Full calibrated run (486+ recommended)
   - Detect + diagnose, no bench (good for 286 / XT)
   - Detect only (fastest, any CPU)
   - Mono-forced display (MDA / Hercules)
   - Drop to DOS prompt

Captures land on `A:\CAP.INI` and `A:\CAP.CSV`.

## What's bundled, what's not

**Bundled:**
- `CERBERUS.EXE` (v0.8.1, 170 KB, stock build)
- `AUTOEXEC.BAT`, `MENU.BAT`, `README.TXT`
- For network variant: `NET\NET.BAT` stub + `NET\WATTCP.CFG.TEMPLATE`

**Not bundled (network variant needs these dropped in):**
- Packet drivers per NIC (3C509PD.COM, NE2000.COM, WD8003EP.COM, etc.)
- mTCP binaries (DHCP.EXE, FTP.EXE, HTGET.EXE)

Sources documented in `network/README.TXT`. The packet drivers and mTCP are both free / BSD but not redistributed as part of CERBERUS.

## Smoketest

DOSBox Staging config at `devenv/smoketest-floppy-offline.conf` exercises the offline floppy content non-interactively (picks detect-only, emits to `D:\FLOPPY.INI`). Run with:

```
"C:\Users\...\DOSBox Staging\dosbox.exe" --exit -conf devenv/smoketest-floppy-offline.conf
```

Confirms `CERBERUS.EXE` runs from `A:\` and emits both INI + CSV. Per the smoketest-after-every-build rule this should run after any change to the floppy content.
