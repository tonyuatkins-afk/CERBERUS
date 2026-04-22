@ECHO OFF
REM CERBERUS bench-floppy AUTOEXEC — offline (no network) variant.
REM Drops into MENU.BAT which prompts for a machine ID and runs CERBERUS.
REM All output INIs land on A:\ so removing the floppy yields the captures.

PROMPT $p$g
PATH A:\

ECHO ====================================================================
ECHO  CERBERUS bench floppy (offline) - v0.8.1
ECHO  Captures land on A:\. Pull the floppy afterward to read on a PC.
ECHO ====================================================================
ECHO.

MENU
