@ECHO OFF
REM CERBERUS bench-floppy AUTOEXEC — network-enabled variant.
REM Loads a packet driver (if drop-in present), offers mTCP DHCP config,
REM then drops into MENU.BAT.

PROMPT $p$g
PATH A:\;A:\NET

ECHO ====================================================================
ECHO  CERBERUS bench floppy (network-enabled) - v0.8.1
ECHO  Captures land on A:\. Network is optional; MENU runs either way.
ECHO ====================================================================
ECHO.

IF EXIST A:\NET\NET.BAT CALL A:\NET\NET.BAT

MENU
