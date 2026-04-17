@ECHO OFF
REM Phase 0 manual smoke test — run inside DOSBox-X (any preset).
REM Usage: from the CERBERUS root inside DOSBox-X, type PHASE0

ECHO === Phase 0 scaffold smoke test ===
ECHO.

ECHO --- Test 1: /? prints help, exits 0 ---
CERBERUS /?
ECHO ERRORLEVEL=%ERRORLEVEL% (expected: 0)
ECHO.

ECHO --- Test 2: default run writes CERBERUS.INI ---
DEL CERBERUS.INI 2>NUL
CERBERUS
IF EXIST CERBERUS.INI (ECHO INI WRITTEN: OK) ELSE (ECHO INI MISSING: FAIL)
ECHO ERRORLEVEL=%ERRORLEVEL% (expected: 0)
ECHO.

ECHO --- Test 3: /U against disabled-upload stub returns 2 ---
CERBERUS /U
ECHO ERRORLEVEL=%ERRORLEVEL% (expected: 2)
ECHO.

ECHO --- Test 4: INI content sample ---
TYPE CERBERUS.INI
ECHO.

ECHO --- Test 5: unknown option rejected ---
CERBERUS /BOGUS
ECHO ERRORLEVEL=%ERRORLEVEL% (expected: 1)
ECHO.

ECHO === Done ===
