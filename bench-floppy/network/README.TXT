CERBERUS bench floppy (network-enabled) - v0.8.1
==================================================

Contents:
  CERBERUS.EXE       - the tool, 170 KB
  AUTOEXEC.BAT       - boot handler: loads network (optional), runs MENU
  MENU.BAT           - interactive session picker (same as offline)
  NET\NET.BAT        - packet-driver + mTCP loader stub (edit before use)
  NET\WATTCP.CFG.TEMPLATE - mTCP config template
  README.TXT         - this file

NOT INCLUDED (drop in per your NIC mix):
  NET\3C509PD.COM    - 3Com 3C509 packet driver
  NET\NE2000.COM     - NE2000 clone packet driver
  NET\WD8003EP.COM   - WD / SMC 8003 packet driver
  NET\DHCP.EXE       - mTCP DHCP client
  NET\FTP.EXE        - mTCP FTP client (for uploading captures)
  NET\HTGET.EXE      - mTCP HTGET client (for v0.9.0 upload path revival)

Fetch these from:
  Packet drivers:  http://crynwr.com/drivers/
                    or bundled with mTCP (www.brutman.com/mTCP/)
  mTCP:            http://www.brutman.com/mTCP/
                   Download mtcp-<latest>.zip, extract and copy the
                   binaries into A:\NET\.

Sizing:
  CERBERUS + DOS boot + scripts  = ~305 KB  (same as offline floppy)
  Packet driver (~10 KB) + mTCP (~80 KB) + config (~5 KB) = ~100 KB
  Total                         = ~405 KB
  Fits 720 KB, 1.2 MB, 1.44 MB. Does NOT fit 360 KB — use the offline
  variant for XT-class 5.25" DD floppies.

Once packet drivers + mTCP are in place:

  1. Edit NET\NET.BAT: uncomment the loader line for your NIC.
  2. Copy NET\WATTCP.CFG.TEMPLATE to NET\WATTCP.CFG and edit the
     network settings (DHCP or static).
  3. Boot the bench machine from the floppy.
  4. AUTOEXEC calls NET.BAT, which loads the packet driver and runs
     DHCP (if configured). If DHCP succeeds you'll see your IP on
     screen.
  5. MENU runs as usual. Captures land on A:\CAP.INI.
  6. After the capture, upload with:
       FTP your-server.com
     or, when the v0.9.0 upload path is live:
       HTGET -f A:\CAP.INI http://barelybooting.com/cerberus/submit

Hardware-specific notes:

  IBM 486 Value Point / PS-ValuePoint:
    NE2000 clone usually works. Use NE2000.COM.

  386 DX-40 + Aztech card:
    Already configured; this floppy is not needed for that box unless
    you want a self-contained bench run.

  286 AT-class with 3Com 3C509 or NE2000:
    Should work. Confirm IRQ / I/O base via the card's setup utility.

  Leading Edge Model D (XT, 8-bit ISA only):
    Most 16-bit NICs (NE2000, 3C509) will NOT work. 8-bit options
    are rare: 3Com 3C503, NE1000, IBM PCnet. If you don't have an
    8-bit NIC for the XT, use the offline floppy instead.

Session choices from the MENU are identical to the offline floppy.

CERBERUS version: 0.8.1 (commit bdfe95c on main)
