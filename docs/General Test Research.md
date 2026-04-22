# Cerberus MS‑DOS Diagnostic Suite

**Executive Summary:** This report outlines the design of **Cerberus**, a comprehensive MS‑DOS hardware diagnostic and benchmarking suite targeting 8088-era through early Pentium PCs.  It covers CPU (integer and floating-point), memory (conventional, XMS, EMS, cache, bandwidth), I/O (disks, keyboard, video, sound, etc.), and system-level tests (timers, CMOS/BIOS, power/thermal, DOS environment).  Each test is described with its **purpose**, prerequisites, method, expected results, implementation notes (real-mode BIOS interrupts, port I/O, inline assembly/C), metrics recorded, safety considerations, and relative difficulty/priority.  We also propose a command-line interface and menu system, sample output formats (human-readable and CSV/JSON), and community features (plugins, scripts, online result sharing).  The design draws on classic DOS-era tools and manuals (e.g. *CacheChk*, *SpeedSys*, *CheckIt*【3†L16-L24】【43†L262-L264】), retro forums (VOGONS) and technical references (Intel datasheets, BIOS services, Wikipedia).  Tables and a flowchart illustrate test dependencies and program flow.

【2†embed_image】 *Figure: Example Cerberus test flow (user selects test categories, runs CPU/Memory/IO tests, aggregates results into report).*

## CPU Tests

### Integer Performance (MIPS/Dhrystone)
- **Purpose:** Measure raw CPU integer throughput (instructions per second, MIPS).  Useful for benchmarking and detecting CPU speed (8088–Pentium).  
- **Prerequisites:** A running DOS environment (real mode).  No special hardware needed.  
- **Method:** Run a well-known integer benchmark (e.g. **Dhrystone 2.1** or custom loop) using BIOS timers (INT 1Ah tick or PIT reads) to count loop iterations. The DOS shareware *DRY_III* (Dhrystone III) runs in protected mode and reports standard Dhrystone results【41†L259-L262】.  We can adapt or reimplement in 16-bit code.  
- **Expected Results:** CPU speed in DMIPS or loops/sec.  Higher values for faster CPUs (e.g. ~8 MIPS for 8086@10MHz, tens of MIPS for 486DX2@66MHz【39†L324-L332】).  Results should roughly double each time clock rate doubles on similar architectures.  
- **Implementation (DOS):** Inline assembly or C code with `int 1Ah` to mark start/end time. For 8086/286, use 1ms timer ticks (18.2Hz) or BIOS INT 12h/15h calls to get memory size as semantically *runtime marker*. Ensure interrupts enabled so timer runs. On 386+, protected-mode DPMI or run under DOS extenders. Use small data set to fit in L1 cache to measure pure compute.  
- **Metrics:** DMIPS (Dhrystones per second), loop iterations per second, cycles per iteration.  Record CPU clock (from user input or BIOS queries) to compute DMIPS.  
- **Safety/Risks:** Very low risk.  Intensive loops are safe. Avoid infinite loops by timeout. Ensure virtual address wrap (if any) is prevented.  
- **Difficulty/Priority:** **High.** Straightforward to implement in assembly/C. Dhrystone is a standard workload, easily found on archives【41†L259-L262】.  

### Instruction Timing and Microbenchmarks
- **Purpose:** Profile individual instructions (e.g. shifts, multiplies, divides, branches) to detect pipeline behavior or unusual latencies (e.g. the slow 8087 divide took ~1000 cycles【30†L179-L183】). Useful for emulation validation or detecting subtle CPU differences.  
- **Prerequisites:** Same as above.  386+ can use RDTSC (on Pentium+) for cycle timing, but on 8086–486 we use timer ticks or calibrated loops.  
- **Method:** Run tight loops of a single instruction (or paired instructions to avoid loop overhead) and time them. For example, count how many ADD or DIV instructions execute in a known time window. Use self-modifying code or counted loops. For branching, measure a long `jmp` vs. conditional mispredict sequences.  
- **Expected Results:** Cycle counts per instruction. For instance, 8087 add ~70–100 cycles, 80387 add ~23–34 cycles【32†L227-L232】; integer ADD on 486 should be ~1 cycle, divide maybe 20+ cycles. Branch misprediction costs (for Pentium+, a few cycles penalty).  
- **Implementation (DOS):** Use BIOS timer or PIT (ports 0x40–0x42) to measure elapsed microseconds. On Pentium, use RDTSC by switching to real mode RMW or virtual-8086 mode if using a DOS extender.  
- **Metrics:** Cycles/instruction (derived), relative latencies.  Possibly report “instructions per microsecond” or “cycles per iteration.”  
- **Safety/Risks:** Running DIV by zero or invalid opcode must be avoided. If testing unsupported instructions, catch exceptions.  
- **Difficulty/Priority:** **Medium.** Requires careful timing; less important than overall benchmarks but useful for deep diagnostics.  

### Floating-Point / x87 Coprocessor Tests
- **Purpose:** Verify the presence, functionality, and performance of the x87 FPU (8087/80287/80387/etc) and compliance with IEEE 754.  Tests include arithmetic accuracy, exception handling, and throughput (FLOPS).  
- **Prerequisites:** If no FPU is present, use software trap (INT 1Ch or undefined opcode). Must also consider BIOS settings for coprocessor.  
- **Method/Algorithm:** Execute known FP operations and compare against expected results. Examples: `FDIV, FSQRT, FSIN` on known inputs; check results against double-precision references (in code or small LUT). Measure cycle count of FADD/FMUL/FDIV loops. The Intel 8087 (1980) accelerated FP operations 20–500%【30†L169-L175】 and performed ~50,000 FLOPS【30†L173-L175】, with simple ops taking ~100+ cycles【30†L179-L183】. Cycle counts drop dramatically on 80387 (387).  We use the *x87* instruction set and check the tag/status word for exceptions (underflow, overflow, etc).  
- **Expected Results:** Correct numeric results within expected precision. FPU stack operations should produce mathematically consistent results. E.g. 8087 may yield ~6× speedup over software FP for basic ops【32†L254-L257】. An absent FPU should trigger an “INT 7F” or illegal opcode.  
- **Implementation (DOS):** Inline assembly with FPU instructions. For example, perform a series of `FADD, FMUL, FDIV` in a loop and time via PIT. Handle x87 status word after each operation to detect errors. Optionally use BIOS INT 15h/AH=0xC7 to check error codes, or INT 1Ch vectored FPU exception.  
- **Metrics:** MFLOPS (million FLOPS), cycles per FP instruction. Also verify flags (zero, overflow, underflow).  
- **Safety/Risks:** Floating operations can trigger traps (division by zero). Mask exceptions or handle INTs gracefully. CPU must be in real mode with coprocessor not disabled by BIOS.  
- **Difficulty/Priority:** **High.** Implementation requires inline x87 assembly. Essential for 386+ machines. We cite that 8087/80287 instructions took tens to hundreds of cycles, whereas 80387 (and Pentium’s integrated FPU) greatly reduced those times【32†L227-L232】.  

### Pipeline and Branch Prediction Proxies
- **Purpose:** Approximate the effects of pipelining and branch prediction (Pentium and later). While not directly observable in real mode, we can infer branch penalty by measuring worst-case jumps.  
- **Prerequisites:** Focus only on Pentium (P5) and above, as 486 has no dynamic branch predictor (but does have a 5-stage pipeline【22†L205-L213】).  
- **Method:** Create a loop with unpredictable branches (e.g. based on timer, or alternating jumps) and measure average execution time. Compare to same code with branches replaced by sequential code to infer mispredict cost. For superscalar FPU swaps (`FXCH`) on Pentium, consult x87 data for 0-cycle exchange optimization【32†L185-L193】.  
- **Expected Results:** Pentium mispredict cost ~15 cycles; 486 pipelines flush cost ~5 cycles. (486 was the first “tightly-pipelined” x86【22†L205-L213】.) Specific values depend on CPU stepping.  
- **Implementation:** Timer-based measurement of conditional loops. Possibly use software branch taken/not taken patterns. (Accurate branch-predictor testing is very advanced; a simple proxy is to time a known branch pattern.)  
- **Metrics:** Cycles lost per mispredicted branch.  
- **Safety/Risks:** Very safe (just loops). Not critical.  
- **Difficulty/Priority:** **Low/Medium.** Interesting but not essential; skip if time is short.  

### Cache Tests (L1/L2/Associativity)
- **Purpose:** Detect presence, size, and associativity of CPU caches, and measure cache latency and bandwidth.  In the DOS era, caches were small (e.g. 486 had 8 KB unified L1【22†L187-L193】; Pentium split 16 KB into 8 KB data + 8 KB code; early Pentium II had optional on-chip L2).  
- **Prerequisites:** No special OS support. Ensure no cache disable (some chipsets allowed turning off L1).  
- **Method:** Perform timed memory accesses with varying working set sizes and strides. For example, use the technique of reading a large array with different strides to find abrupt changes in access time when the data no longer fits in cache. Classic DOS tool *CacheChk* (1986) performed memory-access timing to “see if you have a cache, how many, and to check the access speed”【3†L16-L24】. We can implement pointer-chasing tests that “walk” a linked list spread by page/line sizes. Compare tight loop (L1 hits) vs. random accesses (cache misses) to deduce latency difference.  
- **Expected Results:** Identify L1 and L2 sizes by noting when access time jumps. For instance, a 486DX/33 (8 KB L1) might show a flat access time up to ~8 KB, then a jump to slower L2/main RAM timing. Pentiums typically have a larger L2 (e.g. 256 KB) which may or may not be on-chip. The output could report “L1=8KB, L2 ~256KB, line size ~32 bytes, 4-way associative” if detectable.  
- **Implementation:** In C or assembly, disable interrupts for precise timing, allocate a large array (maybe 2–4 MB), and measure loads with different strides (1,2,4,8,... KB). Use the PIT or a high-resolution timer. On 386+ ensure A20 line is enabled so all memory is addressable.  
- **Metrics:** Access latency (cycles) vs. data size, effective bandwidth (MB/s) for sequential read.  
- **Safety/Risks:** Must run in real mode to avoid protected-mode prefetch.  Also be careful with pages crossing the 64 KB boundary (DOS segment limit). Use `INT 12` or BIOS E801 to determine memory and avoid invalid memory.  
- **Difficulty/Priority:** **High.** Complex to implement correctly, but very useful for cache diagnostics. Existing DOS tools like *CacheChk* and *SpeedSys* use such techniques【3†L16-L24】.  

### Translation Lookaside Buffer (TLB) and MMU
- **Purpose:** In real mode (DOS) paging is typically off, so the TLB (used by paging in protected mode) is inactive. However, on 386+, if an application can briefly enable unreal mode or page tables, one could probe TLB behavior.  
- **Prerequisites:** 386 or above for any paging. Requires switching to protected or unreal mode (beyond simple DOS scope).  
- **Method:** Theoretically, measure memory access latency with varying page alignments to infer TLB size (similar to cache test but stepping by 4 KB or 16 KB). In practice, DOS diagnostics generally skip this.  
- **Expected Results:** (N/A for real mode; in protected mode TLBs are typically 4–8 entries for 80386)  
- **Implementation Notes:** Likely **not feasible** in a purely real-mode DOS utility. We note this limitation rather than inventing a test.  
- **Safety/Risks:** Changing paging bits in real-mode can crash system.  
- **Difficulty/Priority:** **Low/Skipped.** Mentioned for completeness; no real implementation under DOS.  

### Memory Bandwidth & Latency (RAM Copy Speed)
- **Purpose:** Measure raw memory throughput and latency (beyond caches), which can reveal DRAM speed differences (FPM vs. EDO RAM, etc.) and chipset bandwidth.  
- **Prerequisites:** 386+ to use `REP MOVSB` with 32-bit registers. On 8086/286, use word or byte moves.  
- **Method:** Copy a large block of memory (e.g. 1 MB) repeatedly and time the operation. Alternatively, use `MOVSB`/`MOVSW` with chaining. Ensure caches are bypassed by copying larger than cache size or using memory with `MOV` and I/O barrier (if possible).  
- **Expected Results:** Bandwidth in MB/s. Typical 486 with FPM RAM might get ~50–100 MB/s copy; Pentium with EDO may reach ~150–200 MB/s.  
- **Implementation:** Assembly loop with `REP MOVSD`. Use `INT 1Ah` or BIOS calls to start/end. Avoid caching by invalidating caches before test if possible (some CPUs allow cache flush instructions, but tricky in real mode).  
- **Metrics:** Bytes per second (throughput) and baseline latency (ns per access, inferred from inverse throughput).  
- **Safety/Risks:** Memory self-test: ensure not to overwrite system-critical data. Use a safe buffer region.  
- **Difficulty/Priority:** **Medium.** Useful for performance tuning but less critical than basic functionality tests.  

## Memory and BIOS Tests

### Conventional/Extended/Expanded Memory Detection
- **Purpose:** Detect amount of conventional (<640 KB), upper memory (640–1024 KB), and extended memory (>1 MB) available, including XMS/EMS/UMB.  Ensure all configured RAM is accessible.  
- **Prerequisites:** BIOS that supports relevant calls, memory manager (HIMEM, EMM386, etc.) loaded for XMS/EMS.  
- **Method:** Use BIOS interrupts:  
  - **INT 12h** returns KB of base memory (usually 640 KB + BIOS/Video ROM gap)【34†L82-L90】.  
  - **INT 15h/AH=88h** returns contiguous kilobytes above 1 MB (extended memory)【36†L272-L280】. Due to BIOS bugs, it may limit at 64 MB or 15 MB, so use INT 15h/AX=E801h to cover >16 MB【36†L224-L232】.  
  - **EMS (LIM) detection:** If an EMS driver is present, INT 67h (EMS page frame) or calling EMS API can test for expanded memory. This is complicated (requires EMS init).  
  - **UMB (Upper Memory Blocks):** Check UMBs via DOS memory manager (XMS/EMS drivers) or by testing address 0xD0000–0xFFFFF for writeable RAM. Often use EMS in “page frame” at 0xE0000.  
- **Expected Results:** Values in KB/MB of each region. For example, “Conventional: 640 KB, XMS above 1MB: 16384 KB, EMS available: 0 KB” etc.  
- **Implementation:** In C, call BIOS INTs via inline ASM. Use OSDev guidelines: e.g. before INT 15h/AH=88h, clear CF (some BIOSes don’t)【36†L279-L287】. Check carry flag for unsupported. If INT 15h fails, fall back to probing (quick scan of memory with safe read). Tools like CheckIt 3.0 display EMS/XMS information【43†L262-L264】.  
- **Metrics:** Amount of memory (KB/MB) reported in each category.  
- **Safety/Risks:** INT 15h should not damage anything. Probing memory might crash if holes exist (careful to skip known non-RAM regions like BIOS ROMs). Do not overwrite data; use CPU in protected mode? Probably not; skip manual probing.  
- **Difficulty/Priority:** **High.** Critical to know memory resources. Use BIOS calls first, rely on XMS/EMS driver APIs if available (e.g. HIMEM INT 2Fh/AX=4310h to get XMS handle【45†L7-L10】).  

### RAM Integrity Tests (MemTest)
- **Purpose:** Verify that RAM (conventional, extended, expanded) has no bit-errors. Traditionally done by writing/reading patterns (0xAA55, walking ones, etc.).  
- **Prerequisites:** 286+ for more than 64KB, 386+ for extended memory tests. 8086 limited.  
- **Method:** Implement a simple memory test: write patterns to each address, read back and verify. A full `memtest86` is impractical in DOS real mode, but a simplified “walk and fill” test can be done on chunks of memory. For video RAM, switch to graphics mode and write checkerboard patterns.  
- **Expected Results:** No mismatches. If errors occur, report failing addresses.  
- **Implementation:** Either incorporate an existing DOS memory test (CheckIt had RAM tests【43†L262-L264】) or write custom code. For <1MB, use segments; for >1MB, use unreal mode segments (if addressing >64KB).  
- **Metrics:** Errors count (should be zero).  
- **Safety/Risks:** **High-risk** because writing patterns will destroy data (OS, BIOS, video). This test should be run at boot before loading sensitive data, or only on unused free memory. Operating system is MS-DOS itself, which may crash. Hence such tests must be optional or run from a bootable floppy in cold boot.  
- **Difficulty/Priority:** **Medium.** Very useful for diagnosing faulty RAM, but must warn user about memory overwrite. Likely treat as an advanced *“Proceed with caution”* mode.  

### CMOS and BIOS Checks
- **Purpose:** Verify BIOS/CMOS functionality (RTC, battery). Check that CMOS settings (date/time, disk parameters) are stable. Detect if battery is failing (e.g. time resets).  
- **Prerequisites:** Accessible CMOS RAM (I/O ports 0x70/0x71 for index/data) and RTC (IRQ 8 at INT 1Ah).  
- **Method:**  
  - Read CMOS bytes (e.g. BIOS version, clock rate jumper, etc.) and report values. For example, CMOS addresses F/CHE (0x0E) and F/CHF (0x0F) often hold the RTC checksum. A bad battery yields default values (55h/AAh)【36†L274-L283】.  
  - Check RTC by reading date/time (INT 1Ah) and waiting to see if seconds increment (or after warm boot if battery is dead, time goes to Jan 1 1980 often).  
  - Use ROM BIOS area: check for IBM-compatible signature (0x55AA at the end of BIOS ROM).  
- **Expected Results:** Valid CMOS signature, increasing RTC, correct BIOS sign.  
- **Implementation:** In C/ASM, use `out 70h, register; in 71h, data`. Compare known-good vs current. For POST codes, DOS mode cannot see them (they go to speaker); skip.  
- **Metrics:** Battery OK/Fail flag, RTC update rate (Hz), BIOS ID string (if found).  
- **Safety/Risks:** Reading CMOS is safe. Do not write to CMOS except for a controlled test.  
- **Difficulty/Priority:** **Medium.** CMOS battery often fails in old machines, so worthwhile.  

### System Timer and RTC
- **Purpose:** Verify that system timers (8253 PIT, RTC) are functioning.  
- **Prerequisites:** Standard PC hardware.  
- **Method:**  
  - **PIT (IRQ 0):** Use BIOS INT 1Ah or direct PIT port reads to verify the tick rate (18.2 Hz) and timer 0 mode. Possibly measure CPU loops against PIT to calibrate CPU speed (already done in benchmarks).  
  - **RTC (IRQ 8):** Check periodic interrupt (should be disabled by DOS) or read time from CMOS as above.  
- **Expected Results:** PIT channel 0 counts down at 1193182 Hz/65536 ~18.2 Hz. RTC tick once per second.  
- **Implementation:** For PIT, read port 0x40 latch count. For RTC, use INT 1Ah/7 to read CMOS time registers.  
- **Metrics:** Timer tick period (ms), accuracy of RTC over a minute.  
- **Safety/Risks:** Minimal. The timer is shared with DOS. Just ensure interrupts re-enabled if disabled.  
- **Difficulty/Priority:** **Low.** Mostly informational.  

## I/O and Peripheral Tests

### Floppy Disk (INT 13h)
- **Purpose:** Test floppy drive (A:/, B:/) read/write and formatting. Detect bad sectors or missing drive.  
- **Prerequisites:** A floppy controller and disk inserted. DOS blank disk or spare floppy for test.  
- **Method:** Use BIOS INT 13h functions (e.g. AH=00h reset, 02h read sector, 03h write sector) to perform small reads/writes. For example, write a test pattern to track, read back and verify.  
- **Expected Results:** All reads match writes; drive status OK. If errors, report which drive and track/sector failed. Use error codes (AH on INT 13h error).  
- **Implementation:** 16-bit BIOS calls, careful with heads/cylinders (DH=head, DL=drive number). For 360K/1.44M, geometry is standard. For non-standard drives, rely on BIOS.  
- **Metrics:** Speed (ms per sector), bad-sector count.  
- **Safety/Risks:** May corrupt disk data. Only use on blank or user-copied sectors. Warn user.  
- **Difficulty/Priority:** **High (optional).** Essential for floppy-based systems. Feasible via BIOS.

### IDE/ATA Hard Disk (INT 13h Extensions)
- **Purpose:** Detect hard disks and test basic read/write operations via BIOS.  
- **Prerequisites:** ATA drive connected with compatible BIOS (especially for early BIOS with a 528MB/504MB barrier).  
- **Method:** Use INT 13h/AH=48h or 41h (extended disk parameters) to get drive size and geometry. Then perform a read/write on a safe sector (e.g. first sector of partition). Alternatively, use BIOS CHS calls (old style) or drive parameters.  
- **Expected Results:** Drive geometry and capacity reported; read/write success. Error codes on failure (e.g. disk not found).  
- **Implementation:** Use INT 13h with DMA disabled (BIOS handles it).  For LBA > 8GB, DOS int 13h cannot access beyond 8GB due to limitations (unless int 15h E820 map present, but that’s for OS use).  
- **Metrics:** Average read/write time per sector.  
- **Safety/Risks:** Overwriting data can be catastrophic. Only test on empty/dummy partition or ask user for a safe sector.  
- **Difficulty/Priority:** **High (if HDD present).** Drives nearly always use BIOS INT 13h.

### SCSI Disk (INT 14h or Vendor BIOS)
- **Purpose:** If a SCSI controller BIOS is present (rare on early PCs), test the SCSI disks similarly via BIOS calls.  
- **Prerequisites:** SCSI host adapter with BIOS (e.g. Adaptec).  
- **Method:** Use the vendor’s BIOS INT (often INT 14h with AH=00 reset, AH=02 read). This is highly vendor-specific.  
- **Expected Results:** As for IDE.  
- **Difficulty/Priority:** **Low.** Optional – skip or provide general instructions, no coding required without specific vendor docs.  

### Parallel Port (Printer) (INT 17h, Port 378h)
- **Purpose:** Test parallel port hardware and printer interface.  
- **Prerequisites:** Optional printer connected, or loopback.  
- **Method:** Use BIOS INT 17h (print character) to send data, then INT 17h/AH=03 to get status. Alternatively, write directly to port 378h and check port status bits at 27Bh. For loopback test, short pin 14 and 16 to test cable.  
- **Expected Results:** PORT status indicates “Printer ready, no error.” A test character should complete without timeout.  
- **Implementation:** Sequence: INT 17h AH=00 (initialize), AH=02 (print 'A'), AH=03 (status into AL).  
- **Metrics:** Handshake timing, error bits (Paper out, etc.).  
- **Safety/Risks:** Minimal – printing a character on connected printer.  
- **Difficulty/Priority:** **Medium.** Useful if printers are used; easy via BIOS.  

### Serial Ports (COM1–COM4) (INT 14h, Ports 3F8h/2F8h, etc.)
- **Purpose:** Test UARTs (16550 and earlier), RS‑232 links.  
- **Prerequisites:** Possibly a loopback connector or external device.  
- **Method:** Use BIOS INT 14h (AH=00 init, AH=01 send, AH=02 receive, AH=03 status) to loopback characters: send a byte and read it back via a null-modem loopback.  
- **Expected Results:** The character sent equals the character received; no framing/parity errors.  
- **Implementation:** Configure baud (e.g. 9600, 8N1), send 0x55, check receive. For 16550 FIFO, flush first (AH=04).  
- **Metrics:** Baud rate (if measured via loop timing), error count.  
- **Safety/Risks:** Minimal – just RX/TX.  
- **Difficulty/Priority:** **Medium.** Straightforward with BIOS.  

### Keyboard (INT 16h and Port 60h/64h)
- **Purpose:** Test that AT/PS2 keyboards are working (scan codes, modifiers).  
- **Prerequisites:** Keyboards must be AT or PS/2 type.  
- **Method:** Use BIOS INT 16h to check for keystrokes; verify that certain keys (e.g. Enter, A-Z) produce expected scancodes. For PS/2 detection, you can poll port 0x64 status and 0x60 data to see if a key makes code. Compare to INT16 output for consistency.  
- **Expected Results:** Keypresses registered correctly; no stuck keys. Possibly detect keyboard type via port 60h responses (PS/2 keyboards send 0xAA on self-test).  
- **Implementation:** Poll INT 16h/AH=01 (check keystroke ready) and AH=00 (get keystroke). For extended scan codes, check ACPI or direct port.  
- **Metrics:** Latency (ignore), simply pass/fail for each tested key.  
- **Safety/Risks:** Pressing keys can affect the system if not disabled. Only test before boot menus or in safe mode.  
- **Difficulty/Priority:** **Low/Medium.** Use if keyboard reliability is critical (e.g. kiosks).  

### Video (INT 10h graphics/text modes)
- **Purpose:** Verify video adapter operation (CGA/EGA/VGA/SVGA). Test text and graphics modes, memory, and basic drawing.  
- **Prerequisites:** Standard VGA-compatible card or older.  
- **Method:** Use BIOS INT 10h to set various modes (text mode 80×25, graphics mode 320×200, etc). Then write to screen or video memory to test output. For example, fill screen with patterns. *INT 10h* AH=00h sets mode【52†L126-L134】. Then write characters or pixels (via INT 10h or direct memory at A000:0000). Check visually for errors (blocks missing, color errors).  
- **Expected Results:** Display modes change correctly; memory writes appear (no garbled patterns). For monochrome CGA/EGA, check high/low memory segments (B000:0000). Report supported resolutions (from BIOS VBE for SVGA).  
- **Implementation:** Sequence: INT 10h/AH=00 to set mode, AH=06 (scroll/window) to clear, etc. For SVGA, may use VESA BIOS if available (VBE INT 10h/AH=4Fh). No easy way to detect all modes without VESA.  
- **Metrics:** Detected video modes, resolution.  
- **Safety/Risks:** Changing video mode can confuse text output. Use brief delays for user to observe.  
- **Difficulty/Priority:** **High.** Essential to ensure display functions.  

### Sound Devices
- **PC Speaker:**  
  - **Purpose:** Test built-in beeper.  
  - **Method:** Toggle PIT channel 2 (port 0x61) to emit a tone (e.g. standard beep 750 Hz). You can also use BIOS INT 1Ah/3 or DOS `int 21h/2` to beep. User must audibly confirm.  
  - **Expected:** A beep tone.  
  - **Implementation:** E.g. `out 61h, al` commands (where AL enables toggling).  
  - **Safety:** Low.  
  - **Priority:** Low (just confirms speaker not blown).  

- **AdLib (Yamaha OPL2) Sound Card:**  
  - **Purpose:** Detect and test OPL2 FM synth (AdLib/SoundBlaster compatibility).  
  - **Method:** Write to AdLib ports (388h, 389h, etc) known initialization sequence (reset oscillator), then play a short tone by writing to registers (e.g. set FG channel frequency, key on) and ask user to listen. The presence of AdLib can be assumed if ports respond (no simple readback port; may check timer overflow flag at 388h bit 0 after reset).  
  - **Expected:** Audible FM tone (if speaker/line-out connected).  
  - **Safety:** Moderate (high volumes could damage speakers; use low level).  
  - **Priority:** Medium (for legacy music compatibility).  

- **Sound Blaster (DSP-based cards):**  
  - **Purpose:** Test digital audio and MIDI on SB/CMS cards.  
  - **Method:** Write to DSP reset port (2XXh, e.g. 220h base): toggle reset, then read version via port 22Eh,32Eh. If no response, SB not present. Play a square wave: send DSP command 0x1D (tone) or simpler, just test MPU-401 MIDI port (if present) with a known status command.  
  - **Expected:** DSP returns version, a tone is heard, MIDI returns ack.  
  - **Safety:** Use low volume.  
  - **Priority:** Medium (common enough to merit detection).  

*(Note: For brevity, detailed register sequences are omitted; focus on concept. Many PC sound tests rely on trial-and-error or specialized software, as MS-DOS had no standard API beyond low-level port IO.)*  

### Joystick/Gameport (Port 201h)
- **Purpose:** Test analog joystick/gameport (2 axes, 2 buttons).  
- **Prerequisites:** Joystick plugged into gameport.  
- **Method:** Measure the time it takes for the joystick capacitive circuit to charge (read counter bits). DOS extenders used INT 15h/AH=0Bh on some gameport cards; otherwise directly poll port 201h 4-bits (each bit goes low after timing out). In practice, write 0xFF to port 201h to charge capacitors, then read and time loops until bits go low. Compare against a fixed timeout to infer axis position. Buttons are bits 4–5.  
- **Expected:** Axis values vary with knob position, buttons read 0/1 when pressed.  
- **Implementation:** Inline assembler to poll port 0x201.  
- **Metrics:** Analog readings (0–65535), button states.  
- **Safety:** Low.  
- **Difficulty/Priority:** **Low/Optional.** Nice for completeness.  

### Mouse (INT 33h)
- **Purpose:** Verify MS/DOS mouse driver and pointing device.  
- **Prerequisites:** Mouse driver loaded (INT 33h service).  
- **Method:** Call INT 33h/AH=00 (reset) to detect. Then use AH=03 (get position) to see X/Y values. Prompt user to move the mouse and see if coordinates change.  
- **Expected:** INT 33h returns nonzero if mouse is present; movement updates values.  
- **Metrics:** (Pass/Fail) Presence of mouse.  
- **Safety:** Low.  
- **Difficulty/Priority:** **Low.**  

### Network Card (NE2000/ISA Ethernet)
- **Purpose:** Detect presence of an ISA Ethernet card (e.g. NE1000/2000) and basic functionality.  
- **Prerequisites:** NE2K-compatible card at standard I/O base (0x300/0x320) or PCnet.  
- **Method:** Probe known I/O ports (0x300-0x31F) for the NE2K signature (NICs often respond at offset 0x0E = 0x57, 0xEC or 0xFE). Alternatively, attempt to bind a packet driver (INT 14h) and get a MAC. As a minimal test, report “Card detected” if signature found. Full driver interface is beyond Cerberus scope.  
- **Expected:** Either found or not. Report I/O base and IRQ if discovered (hard without driver).  
- **Safety:** Be careful not to conflict with other devices at the same port. Probe only if user consents.  
- **Difficulty/Priority:** **Low.** Rare; optional.  

### Printer (Test Page)
- **Purpose:** Test ability to send data to LPT1/LPT2 printers.  
- **Prerequisites:** A printer or parallel cable (or loopback with NO OP).  
- **Method:** Use DOS `COPY CON LPT1` or write a test string via INT 17h as above.  
- **Expected:** Printer prints or status indicates busy.  
- **Safety:** High – may waste paper/ink. Ask user.  
- **Difficulty/Priority:** **Low/Optional.**  

### Power, Thermal, Voltage Sensors (where available)
- **Purpose:** Read motherboard health sensors (temperatures, fan speeds, voltages) if any BIOS/SMBus support exists. On most vintage PCs this is not exposed to DOS.  
- **Method:** Some modern-ish boards (Pentium era) had Super I/O chips (e.g. Winbond W836xx) accessible via port 2Eh/2Fh with complex protocols, or ACPI tables (none in DOS). Generally **not feasible** under plain DOS without specific drivers.  
- **Implementation:** We skip this in Cerberus for true DOS compatibility, but we note it as a possible enhancement if running under, say, Windows 9x or with a specialized DOS driver.  
- **Difficulty/Priority:** **Very Low.** Only mention as future extension.  

## BIOS Interrupts and POST Behavior
- **Purpose:** Check presence and behavior of critical BIOS interrupts (like INT 19h boot vector, INT 15h power-off functions, etc.) and POST codes.  
- **Method:** 
  - Verify the BIOS interrupt vector table has expected entries (e.g. INT 19h should point to the ROM BIOS bootstrap). Not easily tested under DOS.  
  - For INT 15h features: e.g. check if INT 15h/AH=86h (Wait) works (it just delays), if INT 15h/AX=534D (“SMI” for ACPI) is supported on later boards.  
  - Detect “beep codes” by issuing a bad BIOS call? Probably not safe.  
- **Expected Results:** All supported INTs return valid codes; unsupported ones set CF.  
- **Implementation:** If any particularly useful INTs exist (like INT 15h E820 memory map for >4GB, not needed here).  
- **Safety:** Generally no action for most INTs; but be careful (some INT 15h calls can reboot).  
- **Difficulty/Priority:** **Low.** Mention that BIOS interrupts mainly serve hardware detection (used above).  

## DOS Environment Checks

### DOS Version & OEM
- **Purpose:** Report DOS version, OEM and revision (to understand environment capabilities).  
- **Method:** INT 21h/AH=30h returns major/minor and OEM ID (in BL/BH)【47†L33-L41】.  
- **Expected Results:** Values like “MS-DOS 6.22” or “FreeDOS 1.0” (FreeDOS often has OEM=0xFF).  
- **Metrics:** Major version (AL), Minor (AH), OEM ID (BH).  
- **Implementation:** Simple call and output to report.  
- **Cite:** “INT 21h/AH=30h returns the DOS version number and OEM ID【47†L33-L41】.”  
- **Priority:** **High.** Basic info.

### Interrupt Vector Table and DOS Vectors
- **Purpose:** List interrupt vectors and see if key DOS interrupts (e.g. INT 21h, INT 13h) point to DOS or BIOS.  
- **Method:** Read memory at 0000:0000 (interrupt vector table). E.g. confirm INT 20h (DOS terminate), INT 21h (DOS API) point to DOS base.  
- **Expected:** Known signatures (e.g. 4D 5A “MZ” at an EXE, but not needed).  
- **Implementation:** Possibly skip due to complexity of parsing pointers.  

### Environment Variables / Loaded Drivers
- **Purpose:** Check for environment space, TSRs, memory managers.  
- **Method:** INT 21h/AH=62h to find environment segment, INT 21h/4Fh to check DosX (XMS) handle, INT 2Fh functions for XMS/EMS presence (EMS uses 67h, XMS uses 87h).  
- **Expected:** List of loaded device drivers (DOS lists) and presence of HIMEM, EMM386, etc.  
- **Difficulty/Priority:** **Medium.** Useful for expert users.  

### Protected Mode / Emulation Detection
- **Purpose:** Detect if running under an emulator (DOSBox, QEMU, VirtualBox) or 32-bit DOS extenders.  
- **Method:** Hard; some tricks exist (e.g. DOSBox has trap INT 2Fh_2A with “DOSBox” in string, VirtualBox may leave an I/O port magic). Can skip or mention as future.  
- **Priority:** **Low.**  

## User Interface and Features

### Command-Line Interface
- Cerberus should allow both **interactive menu** and **CLI options**. Sample CLI usage:  
  ```
  Cerberus.exe [-a | --all] [-c | --cpu] [-m | --memory] [-i | --io] [-v | --verbose] [-o <file>] [--json] [--csv]
  ```
  - `-a/--all`: run all tests, 
  - `-c/--cpu`, `-m/--memory`, `-i/--io`: run specific categories, 
  - `-v`: verbose logging, 
  - `-o`: output file (log), 
  - `--csv` or `--json`: machine-readable output.  
  Without arguments, launch an interactive text menu (arrows or number selection). This mirrors DOS utilities like PC-Doctor for DOS (which had both menu and batch modes)【44†L1-L3】.  

### Menu Layout
- **Main Menu:** Options 1: CPU Tests, 2: Memory Tests, 3: I/O Tests, 4: System Info, 5: Run All, Q: Quit.  
- **Submenus:** For example, under CPU Tests: 1=Integer, 2=FPU, 3=Cache, 4=Back.  
- Use simple DOS text UI (80×25). Ensure keystroke reading (INT 16h) so user can navigate without Enter.  

### Output and Reporting
- **Human-readable:** Text paragraphs and labeled values. E.g.:  
  ```
  CPU Benchmark (Dhrystone): 1234 DMIPS (approx)
  FPU: Detected (80387), FADD time = 28 cycles
  L1 Cache: ~8 KB, hit time ~3 cycles; L2: none detected
  Extended Memory: 16384 KB (16 MB)
  Floppy A: 1.44 MB OK (R/W test passed)
  CMOS Battery: OK (RTC held time)
  DOS Version: MS-DOS 6.22 (OEM=IBM)
  ```
- **Tabular/CSV:** Provide a `--csv` option to dump results as CSV, e.g.:
  ```csv
  Test,Result,Units
  Dhrystone,1234,DMIPS
  FPU_Add_Cycles,28,cycles
  L1_Cache_Size,8192,bytes
  XMS_Size,16384,KB
  CMOS_Batt,OK,flag
  ```
- **JSON:** Provide `--json`:
  ```json
  {
    "CPU": {"Dhrystone_DMIPS":1234, "Clock_MHz":66},
    "FPU": {"Present":true, "AddCycles":28},
    "Cache": {"L1_Size":8192, "L1_Assoc":4, "L2_Size":0},
    "Memory": {"ExtendedKB":16384, "EMS_KB":0, "UMB_Bytes":0},
    "Disk": {"FloppyA_OK":true, "HDD_OK":true},
    "System": {"DOS_Version":"6.22", "CMOS_Battery":true}
  }
  ```
- Include an option for an INI-like or structured text output if desired.  

### Self-Test and Stress Modes
- **Repeating tests:** `--repeat <n>` to run tests multiple times (e.g. for stability or stress).  
- **Long-run mode:** e.g. `--burnin 60` to loop certain tests for 60 minutes, logging errors. (Useful for burn-in memory/CPU stability).  

### Logging
- Write output to a log file if `-o` is given (text, CSV or JSON). Also print to screen unless `--quiet`. Include a timestamp.  

### Versioning and Hardware ID
- **Cerberus version:** Embed in binary, display in header.  
- **Hardware ID:** Generate a simple checksum or signature (e.g. hash) of key IDs: CPU type, FPU type, BIOS ID string, maybe first 1K of SMBIOS (if accessible via INT 15h/C7) to help identify machine. Useful for comparing reports across systems.  

### Localization
- While English output is default, allow basic translation by external message files (like DOS .MSG or simple key=value INI file) so community can add languages (e.g. French, German for retro enthusiasts).  

### Example Output Snippet (text)
```
Cerberus v1.0 Diagnostic Report
===============================
CPU: 80486DX4 @ 100 MHz
- Dhrystone 2.1: 5120 DMIPS (~55.5 DMIPS/MHz)
- Integer loop: 140 MIPS (max)
FPU: Intel 80487SX Present
- FADD: 32 cycles, FMUL: 28 cycles (80387-level speeds)
L1 Cache: 16 KB unified, 4-way (detected via latency jump)
Memory: 4 MB XMS (int15h/ax=88h → 4096 KB)
Floppy A: 1.44MB (Found, R/W OK)
HDD C: 500MB (BIOS int13h reports 976 cyl * 16 heads * 63 sectors)
CMOS Battery: GOOD (RTC at 2026-04-21 15:31, not reset)
DOS Version: MS-DOS 6.22 (IBM OEM)
```

### Sample Output (CSV)
```csv
Test,Result,Units
CPU_Dhrystone_DMIPS,5120,DMIPS
CPU_MHz,100,MHz
FPU_Present,Yes,flag
FPU_AddCycles,32,cycles
Cache_L1KB,16,KB
Memory_ExtKB,4096,KB
FloppyA_OK,Yes,flag
HDD_Capacity,500,MB
CMOS_Batt,OK,flag
```

### Sample Output (JSON)
```json
{
  "CPU": {"Model":"i486DX4","MHz":100,"Dhrystone_DMIPS":5120},
  "FPU": {"Present":true,"Type":"80487SX","FADD_Cycles":32},
  "Cache": {"L1_KB":16,"Associativity":4},
  "Memory": {"Extended_KB":4096,"EMS_KB":0},
  "Storage": {"FloppyA_OK":true,"HDD_Capacity_MB":500},
  "CMOS": {"Battery_OK":true,"RTC":"2026-04-21T15:31:00Z"},
  "DOS": {"Version":"6.22","OEM":"IBM"}
}
```

## Reporting and Community Features

- **Plugins/Test Scripts:** Design Cerberus to load additional tests as plugin executables or scripts (e.g. simple batch or .COM tests callable via configuration). Define a plugin API (set of registers or output format) so community can add custom tests (e.g. a memory hole map or specialized card test).  
- **Validation Database:** Encourage users to upload logs to a website. Cerberus could compute a hardware “fingerprint” (CPU type, BIOS date, etc.) and allow comparing results. A checksum or signature included in JSON can be used to identify identical hardware. A future online DB could aggregate Cerberus results by CPU, video card, etc.  
- **Web Upload:** Option `--upload http://site/upload` could POST results to a central repository (with user consent). This allows sharing stats or searching for similar configurations. (Requires a DOS TCP/IP stack or a small embedded http client – advanced feature.)  
- **Localization and Configuration:** Support reading a config file (INI) for default options, and message files for translations. Users can tweak test thresholds or add custom hardware IDs in config.

## Tables and Diagrams

**Table 1.** *CPU Test Comparison.* Common benchmark tests and what they measure:

| Test             | CPUS           | Metric                 | Tool/Ref                         |
| ---------------- | -------------- | ---------------------- | -------------------------------  |
| Dhrystone (int)  | 8086+          | DMIPS (integer MIPS)   | DOS *DRY_III* (Dhrystone)【41†L259-L262】 |
| Custom integer loop | 8086+       | Ops/sec, loop cycles   | Cerberus microbenchmark         |
| x87 FPU (FADD)   | 80287+ (387+)  | Cycles per FADD        | Measured via inline code【32†L227-L232】 |
| Cache access     | 386+           | Latency (ns), size     | *CacheChk* concept【3†L16-L24】 |
| Memory copy      | 386+           | MB/s (bandwidth)       | Cerberus block copy             |

```mermaid
flowchart LR
    A([Start]) --> B{Select Test Category}
    B --> C[CPU Tests]
    B --> D[Memory Tests]
    B --> E[I/O & Peripherals]
    B --> F[System Info]
    C --> C1[Dhrystone Benchmark]
    C --> C2[FPU Test]
    C --> C3[Cache/Microbench]
    D --> D1[RAM Size (INT 15h)]
    D --> D2[EMS/XMS detect]
    D --> D3[MemTest Patterns]
    E --> E1[Floppy Disk (INT13)]
    E --> E2[HDD (INT13/48h)]
    E --> E3[Keyboard/Mouse]
    E --> E4[Video (INT10h)]
    E --> E5[Sound test]
    E --> E6[Serial/Parallel]
    F --> F1[CMOS/RTC]
    F --> F2[BIOS interrupts]
    F --> F3[DOS version]
    C1 & C2 & C3 & D1 & D2 & D3 & E1 & ... --> G[Log Results]
    G --> H{Output Format}
    H --> I[Text Report]
    H --> J[CSV/JSON]
    H --> K[Save to File / Display]
```
*Figure: Program flowchart. The user selects tests (CPU, Memory, I/O, System), the corresponding routines run, results are logged, and output is formatted for the user (text, CSV, JSON).*

## Prioritization

Primary, “must-have” tests are marked **High** priority above: CPU integer/FPU benchmarks, memory size (INT 15h), basic I/O (floppy/hard disk), video, and DOS version. Medium-priority include cache tests, timers, keyboard/mouse, sound. Lower-priority/optional tests (branch prediction, SCSI, joystick, advanced sensors) are included for completeness but can be skipped initially.  

All tests reference standard PC specs and vintage tools where possible. For example, *CacheChk* (1986) and *SpeedSys* are classic DOS benchmarks【3†L16-L24】【15†L302-L304】, and modern communities (VOGONS) often cite SpeedSys for memory testing and CheckIt for 8086/286 systems【15†L302-L304】. BIOS and CPU info is drawn from Intel manuals and Wikipedia (e.g. the 486 has 8 KB L1 cache, 5-stage pipeline【22†L187-L193】【22†L205-L213】). Hardware register details (e.g. INT 21h/30h for DOS version【47†L33-L41】, INT 10h for video modes【52†L126-L134】) are documented in public references. We highlight these sources inline.  

**Sources:** Classic DOS reference sites (DOSDays, Ralf Brown’s IntList), Wikipedia, Intel/AMD datasheets, and retro forums. For example, DOSDays notes *CacheChk* is “a DOS tool that performs memory access timing tests…for 386 and 486 machines”【3†L16-L24】. VOGONS threads recommend *CheckIt* and *SpeedSys* for memory tests under DOS【15†L302-L304】. Intel and x87 Wikipedia entries provide CPU and FPU timings【30†L169-L175】【32†L227-L232】. Extended Memory Specification is explained in Wikipedia【26†L153-L161】, and BIOS interrupt usage (INT 15h 88h/E801h for memory) is documented on OSDev【36†L272-L280】. All cited references are linked. 

