@ECHO OFF
REM NET.BAT — packet driver + mTCP setup for the network-enabled bench
REM floppy. Called from AUTOEXEC.BAT if this file exists.
REM
REM This is a stub; the actual packet-driver .COM files and mTCP
REM binaries are NOT checked into the CERBERUS repo. Drop in the files
REM you need per your NIC mix:
REM
REM   A:\NET\3C509PD.COM     (Crynwr 3Com 3C509)
REM   A:\NET\NE2000.COM      (NE2000 clones)
REM   A:\NET\WD8003EP.COM    (WD / SMC 8003)
REM   A:\NET\DHCP.EXE        (mTCP DHCP client)
REM   A:\NET\WATTCP.CFG      (mTCP config file — edit per network)
REM
REM Source of packet drivers:
REM   Crynwr packet driver collection (public-domain)
REM   http://crynwr.com/drivers/  (retro-mirror; or bundled with mTCP)
REM
REM Source of mTCP:
REM   Michael Brutman, BSD-licensed
REM   http://www.brutman.com/mTCP/
REM
REM Once the files are in place, uncomment the loader below and pick the
REM packet driver for your NIC:

REM Load 3Com 3C509 packet driver at INT 60h, IRQ 10, I/O 0x300:
REM A:\NET\3C509PD.COM 0x60

REM Load NE2000 packet driver at INT 60h, IRQ 3, I/O 0x300:
REM A:\NET\NE2000.COM 0x60 3 0x300

REM Load WD/SMC 8003 packet driver at INT 60h:
REM A:\NET\WD8003EP.COM 0x60

REM Configure mTCP via DHCP (requires WATTCP.CFG present):
REM SET MTCPCFG=A:\NET\WATTCP.CFG
REM A:\NET\DHCP.EXE

ECHO NET.BAT: stub. Edit and uncomment the right packet driver for your NIC.
ECHO  Skipping network config for this boot.
ECHO.
