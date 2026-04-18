# Host-side unit tests — runs on Windows dev box (Watcom-built Win32 console).
# Compiles portable CERBERUS modules with -DCERBERUS_HOST_TEST so the
# hardware-specific code is excluded, then exercises the pure-math paths.
#
# Prerequisites: OpenWatcom installed, WATCOM env set, binnt on PATH.
# Invoke via wmake (NOT gmake). The repo Makefile has a `test` target
# that delegates here.

CC       = wcl386
CFLAGS   = -zq -bt=nt -w3 -DCERBERUS_HOST_TEST -I..\..\src

TESTS = test_timing.exe test_consist.exe test_thermal.exe

all: $(TESTS) .SYMBOLIC

test_timing.exe: test_timing.c ..\..\src\core\timing.c ..\..\src\core\timing.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_timing.c

test_consist.exe: test_consist.c ..\..\src\core\consist.c ..\..\src\core\consist.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_consist.c

test_thermal.exe: test_thermal.c ..\..\src\core\thermal.c ..\..\src\core\thermal.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_thermal.c

run: $(TESTS) .SYMBOLIC
	@test_timing.exe
	@test_consist.exe
	@test_thermal.exe

clean: .SYMBOLIC
	-del *.exe
	-del *.obj
