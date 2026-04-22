# Host-side unit tests — runs on Windows dev box (Watcom-built Win32 console).
# Compiles portable CERBERUS modules with -DCERBERUS_HOST_TEST so the
# hardware-specific code is excluded, then exercises the pure-math paths.
#
# Prerequisites: OpenWatcom installed, WATCOM env set, binnt on PATH.
# Invoke via wmake (NOT gmake). The repo Makefile has a `test` target
# that delegates here.

CC       = wcl386
CFLAGS   = -zq -bt=nt -w3 -DCERBERUS_HOST_TEST -I..\..\src

TESTS = test_timing.exe test_consist.exe test_thermal.exe test_diag_fpu.exe &
        test_diag_fpu_fingerprint.exe test_diag_fpu_edges.exe              &
        test_diag_cache.exe test_diag_dma.exe test_bench_cache.exe         &
        test_bench_cache_char.exe test_report_csv.exe

all: $(TESTS) .SYMBOLIC

test_timing.exe: test_timing.c ..\..\src\core\timing.c ..\..\src\core\timing.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_timing.c

test_consist.exe: test_consist.c ..\..\src\core\consist.c ..\..\src\core\consist.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_consist.c

test_thermal.exe: test_thermal.c ..\..\src\core\thermal.c ..\..\src\core\thermal.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_thermal.c

test_diag_fpu.exe: test_diag_fpu.c ..\..\src\diag\diag_fpu.c ..\..\src\diag\diag.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_diag_fpu.c

test_diag_fpu_fingerprint.exe: test_diag_fpu_fingerprint.c ..\..\src\diag\diag_fpu_fingerprint.c ..\..\src\diag\diag.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_diag_fpu_fingerprint.c

test_diag_fpu_edges.exe: test_diag_fpu_edges.c ..\..\src\diag\diag_fpu_edges.c ..\..\src\diag\diag.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_diag_fpu_edges.c

test_diag_cache.exe: test_diag_cache.c ..\..\src\diag\diag_cache.c ..\..\src\diag\diag.h ..\..\src\core\cache_buffers.c ..\..\src\core\cache_buffers.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_diag_cache.c

test_diag_dma.exe: test_diag_dma.c ..\..\src\diag\diag_dma.c ..\..\src\diag\diag.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_diag_dma.c

test_bench_cache.exe: test_bench_cache.c ..\..\src\bench\bench_cache.c ..\..\src\bench\bench.h ..\..\src\core\cache_buffers.c ..\..\src\core\cache_buffers.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_bench_cache.c

test_bench_cache_char.exe: test_bench_cache_char.c ..\..\src\bench\bench_cache_char.c ..\..\src\bench\bench.h ..\..\src\core\report.c ..\..\src\core\sha1.c
	$(CC) $(CFLAGS) -fe=$@ test_bench_cache_char.c

test_report_csv.exe: test_report_csv.c ..\..\src\core\report.c ..\..\src\core\sha1.c ..\..\src\core\report.h ..\..\src\cerberus.h
	$(CC) $(CFLAGS) -fe=$@ test_report_csv.c

run: $(TESTS) .SYMBOLIC
	@test_timing.exe
	@test_consist.exe
	@test_thermal.exe
	@test_diag_fpu.exe
	@test_diag_fpu_fingerprint.exe
	@test_diag_fpu_edges.exe
	@test_diag_cache.exe
	@test_diag_dma.exe
	@test_bench_cache.exe
	@test_bench_cache_char.exe
	@test_report_csv.exe

clean: .SYMBOLIC
	-del *.exe
	-del *.obj
