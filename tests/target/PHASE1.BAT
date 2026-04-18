@ECHO OFF
REM Phase 1 real-hardware validation run — Task 1.10
REM Usage: from the directory containing CERBERUS.EXE, type PHASE1
REM
REM Defaults to calibrated 7-pass mode so thermal + rule 4a exercise.
REM Edit the /O: target name to match your machine before running.

ECHO === CERBERUS Phase 1 validation run ===
ECHO.

ECHO --- Running CERBERUS (calibrated 7-pass) ---
DEL BENCH486.INI 2>NUL
CERBERUS /C:7 /O:BENCH486.INI
SET RC=%ERRORLEVEL%
ECHO ERRORLEVEL=%RC% (expected: 0)
ECHO.

IF NOT EXIST BENCH486.INI (
    ECHO FAIL: BENCH486.INI not written
    GOTO END
)

ECHO --- INI written. Contents: ---
TYPE BENCH486.INI
ECHO.

ECHO --- Checking for CERBERUS.UNK (unknown hardware capture) ---
IF EXIST CERBERUS.UNK (
    ECHO CERBERUS.UNK present -- sneaker this back with the INI
    TYPE CERBERUS.UNK
) ELSE (
    ECHO No unknown hardware captured.
)
ECHO.

ECHO === Done ===
ECHO Sneaker-net BENCH486.INI back to the dev box for analysis.

:END
