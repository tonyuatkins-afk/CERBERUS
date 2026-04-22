@ECHO OFF
REM NET.BAT — packet driver + mTCP setup for the network-enabled bench
REM floppy. Called from AUTOEXEC.BAT. Bundled drivers + mTCP are under
REM A:\NET\ (see MTCP_GPL.TXT for mTCP license; Crynwr packet drivers
REM shipped as uploaded from crynwr.com/drivers/).
REM
REM This is a stub — pick the right packet driver for your NIC by
REM uncommenting ONE of the load lines below, then adjust the IRQ /
REM I/O base if your card is not on the default settings.
REM
REM Included packet drivers:
REM   3C509.COM     3Com 3C509 (ISA-16, auto-detect)
REM   3C503.COM     3Com 3C503 (older 8-bit/16-bit)
REM   NE2000.COM    NE2000 clones (most common)
REM   NE1000.COM    NE1000 (8-bit XT-class)
REM   WD8003E.COM   Western Digital / SMC 8003E
REM   SMC_WD.COM    SMC/WD alternative driver
REM
REM Included mTCP:
REM   DHCP.EXE      IP config via DHCP
REM   FTP.EXE       FTP client for upload
REM   HTGET.EXE     HTTP GET (for v0.9.0 upload-path revival)
REM   SNTP.EXE      NTP clock sync (useful for signature timestamping)
REM   PING.EXE      Connectivity test

SET PATH=%PATH%;A:\NET
SET MTCPCFG=A:\NET\WATTCP.CFG

REM --- Step 1: load a packet driver --- (uncomment ONE line)
REM
REM REM 3C509 auto-config (recommended for 3C509 cards):
REM A:\NET\3C509.COM 0x60
REM
REM REM NE2000 at IRQ 3, I/O 0x300 (the default most clones ship with):
REM A:\NET\NE2000.COM 0x60 3 0x300
REM
REM REM 3C503 at IRQ 3, I/O 0x300:
REM A:\NET\3C503.COM 0x60 3 0x300
REM
REM REM NE1000 (8-bit XT) at IRQ 3, I/O 0x300:
REM A:\NET\NE1000.COM 0x60 3 0x300
REM
REM REM WD/SMC 8003E at IRQ 3, I/O 0x280 (default jumpers):
REM A:\NET\WD8003E.COM 0x60

REM --- Step 2: make sure WATTCP.CFG exists ---
REM Copy the template if no config exists yet:
IF NOT EXIST A:\NET\WATTCP.CFG COPY A:\NET\WATTCP.CFG.TEMPLATE A:\NET\WATTCP.CFG > NUL

REM --- Step 3: get an IP via DHCP ---
REM Uncomment to run DHCP (requires packet driver loaded above):
REM A:\NET\DHCP.EXE

ECHO.
ECHO NET.BAT stub. Edit the ONE packet-driver line for your NIC,
ECHO then uncomment the DHCP line. Skipping network for this boot.
ECHO.
