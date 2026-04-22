# CERBERUS bench floppy

Self-contained content to drop onto a bootable DOS 1.44 MB floppy so any bench machine (8088 through 486) can run CERBERUS end-to-end from a single disk. All of Tony's target boxes support 1.44 MB, so 1.44 MB is the recommended format across the fleet.

Two variants:

- **`offline/`** — no network. Fits every format 360 KB and up. ~180 KB of floppy content + DOS boot. Captures land on `A:\CAP.INI` and `A:\CAP.CSV`; pull floppy, read on a modern PC. Use when the box has no NIC, or you don't want to mess with network config.

- **`network/`** — bundled packet drivers + mTCP, ready to go. Fits 720 KB and up (NOT 360 KB). ~600 KB of floppy content + DOS boot. Includes 6 packet drivers (3C509, 3C503, NE2000, NE1000, WD8003E, SMC_WD) and 5 mTCP binaries (DHCP, FTP, HTGET, SNTP, PING). Pick the NIC driver, uncomment one line in `NET\NET.BAT`, boot — DHCP handles the rest.

## Which to use tonight

| Bench machine | Recommended floppy | Why |
|---|---|---|
| BEK-V409 (486) | FTP path (already configured) — bench floppy is backup | FTP over existing network setup is the established flow |
| IBM 486 (NIC unknown) | network, fall back to offline | Try NE2000 or 3C509 first; if the NIC isn't in the bundled set, offline still captures |
| 386 DX-40 | FTP path (already configured) — bench floppy is backup | Same as BEK-V409 |
| 286 board | network if a supported NIC is present, else offline | Bundled drivers cover 3C509 / NE2000 / WD/SMC; should hit most AT-era cards |
| Leading Edge Model D (XT) | offline, OR network if an 8-bit NIC is present | 8-bit NE1000 / 3C503 are bundled for this case |

## How to make the floppy

### Option A: Rufus (simplest on Windows)

1. Download [Rufus](https://rufus.ie).
2. Insert a 1.44 MB formatted floppy in your USB floppy drive.
3. Rufus → "Create a bootable disk using" → FreeDOS.
4. Once Rufus finishes, copy the entire contents of `offline/` or `network/` onto the drive via Explorer.

### Option B: existing DOS boot disk

1. `FORMAT A: /S` from a real DOS machine or DOSBox-X.
2. Copy the contents of `offline/` or `network/` to `A:`.

### Option C: mtools (Linux / macOS / WSL)

```
mformat -f 1440 A:
mcopy -i floppy.img offline/* ::
```

## What's bundled

### offline/
- `CERBERUS.EXE` (v0.8.1 stock, 170 KB — copy of `dist/CERBERUS.EXE`, regenerated before each floppy write)
- `AUTOEXEC.BAT` — PROMPT, PATH, call MENU
- `MENU.BAT` — interactive session picker with five choices
- `README.TXT` — floppy-making instructions and session notes

### network/
Everything in `offline/`, plus `NET\` subdirectory containing:
- **Packet drivers**: `3C509.COM`, `3C503.COM`, `NE2000.COM`, `NE1000.COM`, `WD8003E.COM`, `SMC_WD.COM` (from Crynwr)
- **mTCP binaries**: `DHCP.EXE`, `FTP.EXE`, `HTGET.EXE`, `SNTP.EXE`, `PING.EXE` (UPX-compressed, GPL v3)
- **Config**: `NET.BAT` loader stub (uncomment one line per NIC), `WATTCP.CFG.TEMPLATE`
- **Licensing**: `MTCP_GPL.TXT` on-disk for the GPL v3 license text, `00_mtcp.txt` for the mTCP readme

### License attribution

- **CERBERUS** (MIT): see repo `LICENSE`.
- **mTCP** (GPL v3): source at [brutman.com/mTCP](http://www.brutman.com/mTCP/). License text on floppy as `MTCP_GPL.TXT`. Redistributed with source-link per GPLv3 §6.
- **Crynwr packet drivers** (public-domain / free-redistribute): source at [crynwr.com/drivers](http://crynwr.com/drivers/).

## Auto-run flow

On boot:

1. `AUTOEXEC.BAT` runs — sets PROMPT + PATH.
2. Network variant only: `NET\NET.BAT` stub runs — packet driver load + DHCP (if configured).
3. `MENU.BAT` offers five session choices:
   1. Full calibrated run (486+ recommended, 15-30 min)
   2. Detect + diagnose, no bench (286 / XT, 30-90 sec)
   3. Detect only (fastest, any CPU, 10-30 sec)
   4. Mono-forced display (MDA / Hercules / amber / green)
   5. DOS prompt (run CERBERUS by hand)

Captures land on `A:\CAP.INI` + `A:\CAP.CSV`.

## Smoketest

DOSBox Staging config at `devenv/smoketest-floppy-offline.conf` exercises the offline floppy content non-interactively (picks detect-only, emits to `D:\FLOPPY.INI`).

```
"C:\Users\...\DOSBox Staging\dosbox.exe" --exit -conf devenv/smoketest-floppy-offline.conf
```

Confirms `CERBERUS.EXE` runs from `A:\` and emits both INI + CSV. Per the smoketest-after-every-build rule, run this after any change to the floppy content.
