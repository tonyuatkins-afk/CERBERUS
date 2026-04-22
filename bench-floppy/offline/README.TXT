CERBERUS bench floppy (offline) - v0.8.1
==========================================

Contents:
  CERBERUS.EXE  - the tool, 170 KB
  AUTOEXEC.BAT  - boot handler, drops into MENU.BAT
  MENU.BAT      - interactive session picker
  README.TXT    - this file

To use:
  1. Insert the floppy in the bench machine.
  2. Power on (cold boot) with boot order: floppy first.
  3. AUTOEXEC runs MENU.BAT; pick 1-5 for a capture session.
  4. Capture writes to A:\CAP.INI + A:\CAP.CSV on the floppy.
  5. Remove the floppy, read it on a modern PC.

To make this floppy (on Windows):

  Option A: Rufus (https://rufus.ie).
    Select your USB floppy drive, choose "FreeDOS" image, write.
    Then copy AUTOEXEC.BAT, MENU.BAT, CERBERUS.EXE to the drive.

  Option B: DOSBox-X imgmake + imgmount.
    dosbox-x -c "imgmake A.IMG -t fd_360 -fs fat" -exit
    imgmount A A.IMG then copy files via dosbox
    (36  needs a DOS boot sector injected; Rufus is easier.)

  Option C: FreeDOS install disk.
    Format a FreeDOS floppy, delete the installer files, drop in
    AUTOEXEC/MENU/CERBERUS.EXE.

Disk size notes:
  720 KB  recommended fleet format. Tony's 8088 supports 720 KB
           only; AT-class boxes (286/386/486) read 720 KB fine too.
           Content ~305 KB, leaves ~415 KB headroom for INI output.
  1.44 MB works on 286 / 386 / 486 (not on the 8088).
  360 KB  5.25 DD: only if a box is limited to that format.
           Content ~305 KB fits with ~55 KB headroom.

Session choices from the MENU:

  1. Full calibrated run. Best when you have time and the CPU is
     fast enough (486+). 15-30 min on 486, impractical on XT.

  2. Detect + diagnose, no bench. Good for 286 / XT tier where
     benchmarks take too long. 30-90 sec total.

  3. Detect only. Fastest path. 10-30 sec on any CPU. Good for a
     first-pass identity capture when you are not sure the tool
     will complete on that machine.

  4. Mono-forced display. Use on MDA / Hercules / amber-phosphor
     color monitors where the color attribute logic mis-renders.

  5. Drop to DOS prompt. Run CERBERUS by hand with whatever flags
     you want. Type MENU to return to this picker.

After the capture, A:\CAP.INI and A:\CAP.CSV are the outputs. Both
are ASCII and safe to read on any platform.

Notes for XT tier (8088 / V20 with Leading Edge):
  - Use option 2 or 3. Full bench takes 15+ minutes.
  - Capture will run in ~60-90 seconds at option 2.
  - FPU probe (fpu_asm_probe) works on 8087 and reports "none" if
    absent. The behavioral fingerprint runs on 8087 and tags family
    LEGACY (projective infinity, no FPREM1, no FSIN).

Notes for 286 tier:
  - First CERBERUS real-hardware 286 capture. Whatever comes out is
    the reference baseline.
  - Use option 1 or 2 depending on patience.

Notes for 386 and 486 tier:
  - Any option works. If the box already has network + a prepared
    DOS, the network-enabled bench floppy (bench-floppy/network/)
    provides mTCP + FTP and can upload the capture directly.

CERBERUS version: 0.8.1 (commit bdfe95c on main)
Binary MD5: (see dist/CERBERUS.EXE on the repo host)
