@ECHO OFF
REM MENU.BAT — interactive session picker for the bench floppy.
REM Uses CHOICE (DOS 6.0+). If CHOICE is not present, each prompt waits
REM for a keypress instead.

:TOP
ECHO.
ECHO Pick the session:
ECHO.
ECHO   1. Quick detect + diagnose + bench (calibrated, full capture)
ECHO   2. Quick: detect + diagnose only (no bench, for slow CPUs)
ECHO   3. Detect only (fastest, <30 sec on any CPU)
ECHO   4. Mono-forced display (for MDA / amber / green phosphor)
ECHO   5. DOS prompt (run CERBERUS by hand)
ECHO   Q. Quit to DOS prompt (no run)
ECHO.
CHOICE /C:12345Q /N Choose session [1-5, Q]:
IF ERRORLEVEL 6 GOTO END
IF ERRORLEVEL 5 GOTO MANUAL
IF ERRORLEVEL 4 GOTO MONO
IF ERRORLEVEL 3 GOTO DET_ONLY
IF ERRORLEVEL 2 GOTO DET_DIAG
IF ERRORLEVEL 1 GOTO FULL

:FULL
SET CNICK=BENCH
ECHO.
ECHO Enter a short machine nickname (alnum-hyphen, max 12 chars),
ECHO or press Enter to use BENCH as default:
ECHO.
SET /P CNICK=Nickname:
CERBERUS /NICK:%CNICK% /NOTE:0.8.1-full-offline /O:A:\CAP.INI /CSV /NOUI
GOTO SHOW

:DET_DIAG
SET CNICK=BENCH
ECHO.
SET /P CNICK=Nickname:
CERBERUS /NICK:%CNICK% /NOTE:0.8.1-noBEnch /SKIP:BENCH /O:A:\CAP.INI /CSV /NOUI
GOTO SHOW

:DET_ONLY
SET CNICK=BENCH
ECHO.
SET /P CNICK=Nickname:
CERBERUS /NICK:%CNICK% /NOTE:0.8.1-det /ONLY:DET /O:A:\CAP.INI /CSV /NOUI
GOTO SHOW

:MONO
SET CNICK=BENCH
ECHO.
SET /P CNICK=Nickname:
CERBERUS /NICK:%CNICK% /NOTE:0.8.1-mono /MONO /SKIP:BENCH /O:A:\CAP.INI /CSV /NOUI
GOTO SHOW

:MANUAL
ECHO.
ECHO OK, dropping to DOS prompt. Run CERBERUS by hand:
ECHO   CERBERUS /NICK:your-id /O:A:\CAP.INI /CSV
ECHO Type MENU to re-enter this picker.
GOTO END

:SHOW
ECHO.
ECHO Capture written to A:\CAP.INI (and A:\CAP.CSV).
ECHO Results summary:
ECHO.
TYPE A:\CAP.INI | FIND "="
ECHO.
ECHO ====================================================================
ECHO  Run another capture?  Type MENU to restart, or pull the floppy.
ECHO ====================================================================

:END
