@echo off
cd /d C:\Development\CERBERUS
set WATCOM=C:\WATCOM
set PATH=C:\WATCOM\BINNT64;C:\WATCOM\BINNT;C:\Program Files\NASM;%PATH%
set INCLUDE=C:\WATCOM\H;C:\WATCOM\H\NT
del REPSTOSD.EXE 2>nul
del REPSTOSD_MAIN.OBJ 2>nul
del REPSTOSD_ASM.OBJ 2>nul
echo === Assembling REPSTOSD.ASM === > _repstosd.log
nasm -f obj REPSTOSD.ASM -o REPSTOSD_ASM.OBJ >> _repstosd.log 2>&1
echo NASM-RC=%ERRORLEVEL% >> _repstosd.log
echo === Compiling REPSTOSD.C === >> _repstosd.log
wcc -0 -ms -ox -w3 -zq -bt=dos -fo=REPSTOSD_MAIN.OBJ REPSTOSD.C >> _repstosd.log 2>&1
echo WCC-RC=%ERRORLEVEL% >> _repstosd.log
echo === Linking === >> _repstosd.log
wlink system dos name REPSTOSD.EXE option stack=2048 file { REPSTOSD_MAIN.OBJ REPSTOSD_ASM.OBJ } >> _repstosd.log 2>&1
echo LINK-RC=%ERRORLEVEL% >> _repstosd.log
