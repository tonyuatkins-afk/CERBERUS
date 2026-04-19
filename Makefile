# CERBERUS - Retro PC System Intelligence Platform
# Build with OpenWatcom 2.0: wmake
#
# Prerequisites:
#   - OpenWatcom 2.0 installed (WATCOM env var set)
#   - NASM on PATH (for .asm modules — none in Phase 0, added in Task 0.2)
#   - Python 3 (for hw_db CSV -> C regeneration — lands in Phase 1)

CC  = wcc
LD  = wlink
ASM = "C:\Program Files\NASM\nasm.exe"

# OpenWatcom flags:
#   -0          8088/8086 instructions only (maximum compatibility)
#   -fpi        inline 8087 emulation (no coprocessor required)
#   -mm         medium memory model (>64K code, <=64K data near)
#   -ox         maximum optimization
#   -w3         warning level 3 (bump to -wx after code is known clean)
#   -zq         quiet
#   -bt=dos     target DOS
CFLAGS  = -0 -fpi -mm -ox -w3 -zq -bt=dos -i=src
# Historical-benchmark modules compile at -od (no optimization) so Watcom's
# -ox DCE cannot eliminate the Dhrystone/Whetstone workloads. The v8 real-
# iron capture on 2026-04-18 showed that volatile + checksum observers alone
# were insufficient — DCE happened inside the non-volatile-qualified
# procedure bodies (Proc_1/2/3/7) that receive non-volatile pointers to
# Rec_Type members, and propagating volatile through those signatures would
# require reference-breaking qualification changes. -od is methodologically
# correct anyway: Weicker's 1984 Dhrystone paper and Curnow's Whetstone both
# presume unoptimized reference compilation.
#
# -oi (intrinsic math) is added because -od's "no optimization" stance does
# NOT mean "route transcendentals through the software math library." Weicker
# warned against DCE, not against inline FPU instructions. The v9 real-iron
# capture showed Whetstone running 130× too slow at pure -od — every
# sin/cos/atan/sqrt/exp/log call dispatched through Watcom's unoptimized
# libm wrappers rather than compiling to inline x87 instructions. Adding -oi
# restores FPU-native math without touching DCE behavior; volatile + checksum
# observer still carry the anti-DCE load.
#
# Only applied to bench_dhrystone.obj and bench_whetstone.obj; the other
# bench modules (cpu / memory / fpu) have internal volatile / pragma-aux
# guards and stay at -ox.
CFLAGS_NOOPT = -0 -fpi -mm -ot -oi -w3 -zq -bt=dos -i=src
ASFLAGS = -f obj

TARGET  = CERBERUS.EXE
MAPFILE = cerberus.map
STACK   = 4096

OBJS = src\main.obj                                                  &
       src\core\timing.obj    src\core\display.obj                   &
       src\core\report.obj    src\core\sha1.obj                      &
       src\core\consist.obj   src\core\thermal.obj                   &
       src\core\crumb.obj     src\core\ui.obj                        &
       src\detect\detect_all.obj                                     &
       src\detect\env.obj     src\detect\unknown.obj                 &
       src\detect\cpu.obj     src\detect\cpu_a.obj                   &
       src\detect\cpu_db.obj                                         &
       src\detect\fpu.obj     src\detect\fpu_a.obj                   &
       src\detect\fpu_db.obj                                         &
       src\detect\mem.obj     src\detect\mem_a.obj                   &
       src\detect\cache.obj   src\detect\bus.obj                     &
       src\detect\video.obj   src\detect\video_db.obj                &
       src\detect\audio.obj   src\detect\audio_db.obj                &
       src\detect\bios.obj    src\detect\bios_db.obj                 &
       src\diag\diag_all.obj  src\diag\diag_cpu.obj                  &
       src\diag\diag_mem.obj  src\diag\diag_fpu.obj                  &
       src\diag\diag_video.obj                                       &
       src\bench\bench_all.obj src\bench\bench_cpu.obj               &
       src\bench\bench_memory.obj src\bench\bench_fpu.obj            &
       src\bench\bench_dhrystone.obj src\bench\bench_whetstone.obj   &
       src\upload\upload.obj

all: $(TARGET) .SYMBOLIC

$(TARGET): $(OBJS)
	$(LD) system dos name $(TARGET) option map=$(MAPFILE) option stack=$(STACK) file { $(OBJS) }

# Explicit per-file rules (wmake inference rules across subdirs are fragile)
src\main.obj: src\main.c src\cerberus.h src\detect\unknown.h src\core\ui.h src\core\consist.h src\core\thermal.h src\core\timing.h
	$(CC) $(CFLAGS) -fo=$^@ src\main.c

src\core\timing.obj: src\core\timing.c src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\timing.c

src\core\display.obj: src\core\display.c src\core\display.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\display.c

src\core\report.obj: src\core\report.c src\core\report.h src\core\sha1.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\report.c

src\core\sha1.obj: src\core\sha1.c src\core\sha1.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\sha1.c

src\core\consist.obj: src\core\consist.c src\core\consist.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\consist.c

src\core\thermal.obj: src\core\thermal.c src\core\thermal.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\thermal.c

src\core\crumb.obj: src\core\crumb.c src\core\crumb.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\crumb.c

src\core\ui.obj: src\core\ui.c src\core\ui.h src\core\display.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\ui.c

src\detect\detect_all.obj: src\detect\detect_all.c src\detect\detect.h src\core\crumb.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\detect_all.c

src\detect\env.obj: src\detect\env.c src\detect\env.h src\detect\detect.h src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\env.c

src\detect\unknown.obj: src\detect\unknown.c src\detect\unknown.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\unknown.c

src\detect\cpu.obj: src\detect\cpu.c src\detect\detect.h src\detect\cpu.h src\detect\env.h src\detect\unknown.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\cpu.c

src\detect\cpu_a.obj: src\detect\cpu_a.asm
	$(ASM) $(ASFLAGS) -o src\detect\cpu_a.obj src\detect\cpu_a.asm

src\detect\cpu_db.obj: src\detect\cpu_db.c src\detect\cpu_db.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\cpu_db.c

# Regenerate the C source from the CSV. Run manually when cpus.csv changes.
regen-cpu-db: .SYMBOLIC
	python hw_db\build_cpu_db.py

src\detect\fpu.obj: src\detect\fpu.c src\detect\detect.h src\detect\cpu.h src\detect\env.h src\detect\fpu_db.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\fpu.c

src\detect\fpu_a.obj: src\detect\fpu_a.asm
	$(ASM) $(ASFLAGS) -o src\detect\fpu_a.obj src\detect\fpu_a.asm

src\detect\fpu_db.obj: src\detect\fpu_db.c src\detect\fpu_db.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\fpu_db.c

regen-fpu-db: .SYMBOLIC
	python hw_db\build_fpu_db.py

src\detect\mem.obj: src\detect\mem.c src\detect\detect.h src\detect\cpu.h src\detect\env.h src\core\report.h src\core\crumb.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\mem.c

src\detect\mem_a.obj: src\detect\mem_a.asm
	$(ASM) $(ASFLAGS) -o src\detect\mem_a.obj src\detect\mem_a.asm

src\detect\cache.obj: src\detect\cache.c src\detect\detect.h src\detect\cpu.h src\detect\env.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\cache.c

src\detect\bus.obj: src\detect\bus.c src\detect\detect.h src\detect\cpu.h src\detect\env.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\bus.c

src\detect\video.obj: src\detect\video.c src\detect\detect.h src\detect\env.h src\detect\video_db.h src\core\display.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\video.c

src\detect\video_db.obj: src\detect\video_db.c src\detect\video_db.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\video_db.c

regen-video-db: .SYMBOLIC
	python hw_db\build_video_db.py

src\detect\audio.obj: src\detect\audio.c src\detect\detect.h src\detect\env.h src\detect\audio_db.h src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\audio.c

src\detect\audio_db.obj: src\detect\audio_db.c src\detect\audio_db.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\audio_db.c

regen-audio-db: .SYMBOLIC
	python hw_db\build_audio_db.py

src\detect\bios.obj: src\detect\bios.c src\detect\detect.h src\detect\env.h src\detect\bios_db.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\bios.c

src\detect\bios_db.obj: src\detect\bios_db.c src\detect\bios_db.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\bios_db.c

regen-bios-db: .SYMBOLIC
	python hw_db\build_bios_db.py

src\diag\diag_all.obj: src\diag\diag_all.c src\diag\diag.h src\core\crumb.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\diag\diag_all.c

src\diag\diag_cpu.obj: src\diag\diag_cpu.c src\diag\diag.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\diag\diag_cpu.c

src\diag\diag_mem.obj: src\diag\diag_mem.c src\diag\diag.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\diag\diag_mem.c

src\diag\diag_fpu.obj: src\diag\diag_fpu.c src\diag\diag.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\diag\diag_fpu.c

src\diag\diag_video.obj: src\diag\diag_video.c src\diag\diag.h src\core\display.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\diag\diag_video.c

src\bench\bench_all.obj: src\bench\bench_all.c src\bench\bench.h src\core\crumb.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\bench\bench_all.c

src\bench\bench_cpu.obj: src\bench\bench_cpu.c src\bench\bench.h src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\bench\bench_cpu.c

src\bench\bench_memory.obj: src\bench\bench_memory.c src\bench\bench.h src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\bench\bench_memory.c

src\bench\bench_fpu.obj: src\bench\bench_fpu.c src\bench\bench.h src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\bench\bench_fpu.c

src\bench\bench_dhrystone.obj: src\bench\bench_dhrystone.c src\bench\bench.h src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS_NOOPT) -fo=$^@ src\bench\bench_dhrystone.c

src\bench\bench_whetstone.obj: src\bench\bench_whetstone.c src\bench\bench.h src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS_NOOPT) -fo=$^@ src\bench\bench_whetstone.c

src\upload\upload.obj: src\upload\upload.c src\upload\upload.h
	$(CC) $(CFLAGS) -fo=$^@ src\upload\upload.c

clean: .SYMBOLIC
	-del src\*.obj
	-del src\core\*.obj
	-del src\detect\*.obj
	-del src\diag\*.obj
	-del src\bench\*.obj
	-del src\upload\*.obj
	-del $(TARGET)
	-del $(MAPFILE)
