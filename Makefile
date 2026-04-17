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
ASFLAGS = -f obj

TARGET  = CERBERUS.EXE
MAPFILE = cerberus.map
STACK   = 4096

OBJS = src\main.obj                                                  &
       src\core\timing.obj    src\core\display.obj                   &
       src\core\report.obj    src\core\sha1.obj                      &
       src\core\consist.obj   src\core\thermal.obj                   &
       src\core\crumb.obj                                            &
       src\detect\detect_all.obj                                     &
       src\detect\env.obj                                            &
       src\detect\cpu.obj     src\detect\cpu_a.obj                   &
       src\detect\cpu_db.obj                                         &
       src\detect\fpu.obj     src\detect\fpu_a.obj                   &
       src\detect\fpu_db.obj                                         &
       src\detect\mem.obj                                            &
       src\detect\cache.obj   src\detect\bus.obj                     &
       src\detect\video.obj   src\detect\audio.obj                   &
       src\detect\bios.obj                                           &
       src\diag\diag_all.obj  src\bench\bench_all.obj                &
       src\upload\upload.obj

all: $(TARGET) .SYMBOLIC

$(TARGET): $(OBJS)
	$(LD) system dos name $(TARGET) option map=$(MAPFILE) option stack=$(STACK) file { $(OBJS) }

# Explicit per-file rules (wmake inference rules across subdirs are fragile)
src\main.obj: src\main.c src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\main.c

src\core\timing.obj: src\core\timing.c src\core\timing.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\timing.c

src\core\display.obj: src\core\display.c src\core\display.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\display.c

src\core\report.obj: src\core\report.c src\core\report.h src\core\sha1.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\report.c

src\core\sha1.obj: src\core\sha1.c src\core\sha1.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\sha1.c

src\core\consist.obj: src\core\consist.c src\core\consist.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\consist.c

src\core\thermal.obj: src\core\thermal.c src\core\thermal.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\thermal.c

src\core\crumb.obj: src\core\crumb.c src\core\crumb.h
	$(CC) $(CFLAGS) -fo=$^@ src\core\crumb.c

src\detect\detect_all.obj: src\detect\detect_all.c src\detect\detect.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\detect_all.c

src\detect\env.obj: src\detect\env.c src\detect\env.h src\detect\detect.h src\core\timing.h src\core\report.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\env.c

src\detect\cpu.obj: src\detect\cpu.c src\detect\detect.h src\detect\env.h src\core\report.h src\cerberus.h
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

src\detect\mem.obj: src\detect\mem.c src\detect\detect.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\mem.c

src\detect\cache.obj: src\detect\cache.c src\detect\detect.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\cache.c

src\detect\bus.obj: src\detect\bus.c src\detect\detect.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\bus.c

src\detect\video.obj: src\detect\video.c src\detect\detect.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\video.c

src\detect\audio.obj: src\detect\audio.c src\detect\detect.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\audio.c

src\detect\bios.obj: src\detect\bios.c src\detect\detect.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\detect\bios.c

src\diag\diag_all.obj: src\diag\diag_all.c src\diag\diag.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\diag\diag_all.c

src\bench\bench_all.obj: src\bench\bench_all.c src\bench\bench.h src\cerberus.h
	$(CC) $(CFLAGS) -fo=$^@ src\bench\bench_all.c

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
