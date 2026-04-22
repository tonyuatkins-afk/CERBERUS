CERBERUS bench floppy (network-enabled) - v0.8.1
==================================================

A single 1.44 MB floppy: boot any 8088 through 486 bench box,
get network, capture, FTP back.

Contents:
  A:\
    CERBERUS.EXE    - the tool, 170 KB (v0.8.1 stock)
    AUTOEXEC.BAT    - boot handler: loads network, runs MENU
    MENU.BAT        - interactive session picker
    README.TXT      - this file
  A:\NET\
    NET.BAT         - packet-driver + mTCP loader (edit before use)
    WATTCP.CFG.TEMPLATE - mTCP config template (DHCP + static examples)
    3C509.COM       - 3Com 3C509 packet driver (16-bit ISA)
    3C503.COM       - 3Com 3C503 packet driver (8-bit / 16-bit)
    NE2000.COM      - NE2000 clone packet driver (most common)
    NE1000.COM      - NE1000 packet driver (8-bit XT-class)
    WD8003E.COM     - Western Digital / SMC 8003E
    SMC_WD.COM      - SMC/WD alternative driver
    DHCP.EXE        - mTCP DHCP client (IP config)
    FTP.EXE         - mTCP FTP client (upload captures)
    HTGET.EXE       - mTCP HTTP GET (for 0.9.0 upload-path revival)
    SNTP.EXE        - mTCP NTP client (clock sync)
    PING.EXE        - mTCP ping (connectivity test)
    00_mtcp.txt     - mTCP readme
    MTCP_GPL.TXT    - mTCP GPL v3 license

Size budget on the floppy:
    DOS boot          ~130 KB
    CERBERUS + scripts ~180 KB
    NET\ payload       ~290 KB
    Total              ~600 KB
    Fits 720 KB (~120 KB spare), 1.2 MB, 1.44 MB. Does NOT fit
    360 KB; use the offline variant (bench-floppy/offline/) for
    360 KB floppies or for any box without a NIC.
    720 KB is the recommended fleet format: Tony's 8088 supports
    720 KB only, and AT-class (286/386/486) reads 720 KB fine.

License + attribution:
    mTCP (dhcp.exe, ftp.exe, htget.exe, sntp.exe, ping.exe):
      Copyright Michael B. Brutman. Released under GPL v3.
      Source: http://www.brutman.com/mTCP/
      License text on disk as MTCP_GPL.TXT.
    Crynwr packet drivers (*.COM):
      Public-domain / Crynwr-free redistribution (typical DOS-era
      packet-driver licensing).
      Source: http://crynwr.com/drivers/
    CERBERUS itself: MIT license (see main repo LICENSE file).

To make the floppy (on Windows):
  Option A: Rufus (easiest).
    1. Insert a 1.44 MB formatted floppy in your USB floppy drive.
    2. Rufus - "Create a bootable disk using" - FreeDOS.
    3. After Rufus finishes, copy the entire contents of this
       "network/" directory to the drive via Explorer.

  Option B: existing DOS boot disk.
    1. Format: FORMAT A: /S (from a real DOS machine or DOSBox-X).
    2. Copy the entire contents of this "network/" directory to A:.

  Option C: disk-imaging from a bootable source.
    Use a known-good FreeDOS 1.44 MB image, drop in the files via
    mtools (mcopy) or DOSBox-X imgmount.

Using the floppy:

  1. Insert in the bench box. Boot (cold or warm).
  2. AUTOEXEC loads PROMPT, PATH, then calls A:\NET\NET.BAT.
  3. NET.BAT is a stub on first boot: comes up saying "edit the
     packet-driver line for your NIC." Edit NET.BAT to uncomment
     one driver + DHCP line.
     (You can also do this before physical-floppy write, in which
     case NET.BAT will run DHCP automatically.)
  4. MENU.BAT runs — pick session 1-5 or drop to DOS prompt.
  5. Capture writes to A:\CAP.INI + A:\CAP.CSV.
  6. To upload via FTP:
         FTP your-server.com
         user / password
         put CAP.INI
         bye
     Or wait for the 0.9.0 upload-path revival which will call
     HTGET.EXE against barelybooting.com automatically.

Per-NIC configuration tips:

  3Com 3C509 (ISA-16, common on 486 / 386):
    3C509.COM 0x60
    (Auto-detects. If it fails, specify IRQ / I/O base.)

  NE2000 clones (very common on 486 / 386):
    NE2000.COM 0x60 <IRQ> <I/O>
    IRQ 3, I/O 0x300 is the most common default. Confirm via
    the card's setup utility or jumper card.

  NE1000 (8-bit XT-class for Leading Edge):
    NE1000.COM 0x60 <IRQ> <I/O>
    IRQ 3 or 5, I/O 0x300. 8-bit PACKET.COM for most setups.

  3C503 (8-bit/16-bit, older 3Com):
    3C503.COM 0x60 <IRQ> <I/O>
    IRQ 3 or 5, I/O 0x300.

  WD 8003E / SMC 8003E:
    WD8003E.COM 0x60
    (Auto-detects in most cases.)

Per-box recommendation for tonight (2026-04-23):

  Leading Edge Model D (XT, 8-bit ISA):
    If you have an 8-bit NIC (NE1000 / 3C503), use network floppy
    with that driver. If not, use the offline floppy.

  286 board:
    Depends on installed NIC. If NE2000 or 3C509 is present, use
    network floppy. Otherwise offline.

  IBM 486:
    Most likely has a common NIC. Try 3C509.COM or NE2000.COM.

  386 DX-40 + BEK-V409:
    These are already network-configured. The floppy is a backup
    for when the existing config is disrupted.

CERBERUS version: 0.8.1 (commit bdfe95c on main)
