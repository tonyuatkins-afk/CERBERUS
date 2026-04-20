# REPSTOSD.EXE - DOS real-mode reference binary for CERBERUS issue #6
#
# Standalone, not linked into CERBERUS.EXE. Small-model because the
# program is a few hundred bytes of code and has no data beyond a few
# longs. Targets 386 minimum because REP STOSD is a 386+ instruction;
# anything lower would need REP STOSW and a different comparison table.

CC  = wcc
LD  = wlink

# -0     8088/8086 baseline (REP STOSW is a 16-bit instruction; works
#        on any x86 from 8088 onward, so the tool runs on any CERBERUS
#        target machine that has a VGA card for mode 13h)
# -fpi   inline 8087 emulation (not used here but harmless)
# -ms    small memory model (one segment each of code and data)
# -ox    max optimization
# -w3    warning level 3
# -zq    quiet
# -bt=dos target DOS
CFLAGS = -0 -fpi -ms -ox -w3 -zq -bt=dos

TARGET = REPSTOSD.EXE
STACK  = 2048

all: $(TARGET) .SYMBOLIC

$(TARGET): REPSTOSD.OBJ
	$(LD) system dos name $(TARGET) option stack=$(STACK) file { REPSTOSD.OBJ }

REPSTOSD.OBJ: REPSTOSD.C
	$(CC) $(CFLAGS) -fo=$^@ REPSTOSD.C

clean: .SYMBOLIC
	-del REPSTOSD.OBJ
	-del $(TARGET)
