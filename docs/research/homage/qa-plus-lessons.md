# QA-Plus v3.12 — commercial service-technician diagnostic

**Tool**: QA-Plus v3.12 (DOS OEM consumer version)
**Author / vendor**: DiagSoft, Inc., Scotts Valley, CA
**Copyright**: 1987, 1988 (HDPREP1.COM stamped 1987, 1988, 1989)
**Binary date**: 1989-07-11
**Runtime**: MS-DOS 2.x / 3.x on PC / XT / AT / PS/2
**Distribution**: Commercial; "OEMPAK" variant bundled with registered OEM systems; explicitly not shareware
**Third-party code**: QAPLUS1.COM embeds "Some code licensed from CHESAPEAKE DATA SYSTEMS, INC."

## Attribution notes

The phase-3 task brief labeled this as "QA-Plus v3.12" and the binary strings confirm the version (`QAPLUS Control Panel V3.12`, `QARam ver. 3.12`, the `3.12` marker inside HDPREP1.COM). The shipped `READ.ME` file, however, only documents changes up through **3.11 (05/01/89)**; the 3.12 binaries dated 07/11/89 appear to be a point release with no additional documented change notes. All four binaries are raw MS-DOS executables (no PKLITE / LZEXE / UPX / EXEPACK signatures), so string extraction was sufficient and no unpacking step was needed.

The product studied is the DOS "QAPlus" consumer / OEM product, not the later Windows-era "QAPlus/NT". Attribution to DiagSoft is correct; a second vendor, Chesapeake Data Systems, Inc., is credited inside QAPLUS1.COM for licensed routines, meaning some algorithms inside QAPLUS1.COM are not DiagSoft's originals. A third name, "HOBBIT ENGINEERING INC.", appears as a static string in QAPLUS1.COM, most plausibly a baked-in dealer-info default rather than a code attribution.

## What QA-Plus does

QA-Plus ships as four cooperating binaries, each playing a different role:

1. **QAPLUS.EXE** (126 KB). The Control Panel. Pull-down menu driver, System Configuration report, Interrupt and DMA assignments, Performance Panel, Disk Performance Panel, mouse and joystick interactive tests, help viewer.
2. **QAPLUS1.COM** (45 KB). "Advanced Diagnostics." The destructive / intrusive test set: RAM, CPU ALU, FPU, DMA controllers, interrupt controller, protected-mode entry, clock / calendar / alarm, keyboard + keylock, video (text + modes 0Dh, 10h, 13h), floppy write / read, fixed disk, COM ports with loopback, LPT printer output.
3. **QARAM.EXE** (31 KB). RAM locator. Builds and edits a user-maintained physical board-and-chip map (`qaplus.ram`) and, on a memory-test failure, maps the reported address to the specific chip on a visual board diagram by blinking it.
4. **HDPREP1.COM** (6 KB). Low-level hard-disk format utility. Destructive; lives alongside the diagnostic.

Support files: `DIAGS.DAT` (dealer name / phone, editable in-tool) and `QACONFIG.SYS` (config skeleton).

Diagnostic test menu has four entry modes: (1) test everything once; (2) select specific tests; (3) select specific tests and log errors to LPT1:; (4) display system information only. A separate "Multiple Testing" option loops either a fixed count or endlessly, with an orthogonal "Stop on Error" toggle. Command-line switches `NoNDP` and `NoEMM` exist specifically to skip probes that hang on buggy clone hardware.

## Architectural separation — the four-binary split

QA-Plus's most distinctive design choice is splitting the tool across **four separate executables**, not four modules in one EXE. The driver is a physical constraint documented in the 3.xx release notes: the RAM test has to relocate DOS and QAPLUS1 itself out of the low 128 KB so the memory under them can be tested. MZ EXEs aren't position-independent, so the destructive diagnostic is a relocatable `.COM` file. The control panel, which doesn't do memory relocation, stays a standard `.EXE`. The RAM-locator UI (QARAM) stays separate because it loads a user-editable chip-map file that doesn't belong in the diagnostic core. The low-level format tool (HDPREP1) is separate so the diagnostic can be run without carrying a data-destroying utility in the same process image.

**CERBERUS parallel**: CERBERUS is one EXE with `detect/`, `diag/`, `bench/`, and `core/` module directories. The logical separation matches (detection vs diagnosis vs benchmark), but the physical separation does not. That is fine for CERBERUS's scope — nothing in `diag_mem.c` relocates DOS — but if a future v0.5+ pass wants a memtest86-style full-conventional-memory sweep, QA-Plus's architecture says: that belongs in a separate relocatable binary, not in the main EXE.

## Consistency cross-checks

Direct answer to the sub-question: **QA-Plus does not have a consistency engine in CERBERUS's sense**. Reported-vs-measured cross-checks exist but are embedded in individual tests rather than abstracted into a rule table:

- Port detection writes back to the BIOS data area: "Ports are searched for and BIOS table is updated to max of 3 LPT's / 4 COM's". If BIOS disagreed with probe, probe wins.
- COM / LPT ports that respond to a probe but emit no IRQ during the test get routed to a `noIRQ` list rather than silently passing. The policy is "if you can't name the interrupt, it's not a clean pass."
- DOS time is displayed alongside Clock time, DOS date alongside Clock date, for user-visual comparison. No programmatic delta check.
- The joystick test validates analog voltage is "10x pot @ center" range before counting channels; presence without plausible voltage is not reported as present.

None of these are novel relative to CERBERUS's 11-rule `consist.c`. The lesson runs the other direction: CERBERUS's dedicated consistency-engine architecture, with named rules, verdicts, and single-responsibility checks, is **cleaner than QA-Plus's embedded-in-each-test approach**. No new rule to port.

## Diagnostic methodology — genuine gaps

QAPLUS1.COM runs nine named RAM patterns: `Pseudo-Random`, `Walking Bit left`, `Walking Bit right`, `Inv Walking Bit left`, `Inv Walking Bit right`, `Checkerboard`, `Inv Checkerboard`, `Bit Stuck High`, `Bit Stuck Low`. CERBERUS `diag_mem.c` runs three patterns (walking-1s, walking-0s, address-in-address) on a 4 KB static buffer. The missing pattern categories are:

- **Checkerboard / Inv-Checkerboard** — catches adjacent-cell coupling faults (bit-line shorts, row/column decoder leaks) that per-bit walking patterns miss.
- **Pseudo-random fill** — catches data-pattern-sensitive defects (hold time, retention under varying switching activity).
- **Refresh-failure detection as a distinct error class** — QAPLUS1 classifies `Refresh Failure` alongside `Parity Error`, `Data Error`, etc. CERBERUS's verdict is binary (pattern match or not) without a refresh-specific category.

Filed as v0.5+ candidate: add checkerboard and inv-checkerboard to `diag_mem`, and consider splitting the verdict into `data_fault` vs `refresh_fault` once a refresh probe is feasible.

Other diagnostics QA-Plus runs that CERBERUS does not: **interrupt controller functional test (8259A)**, **clock / calendar / alarm test** (RTC INT 1Ah probe beyond what `timing.c` uses), **keyboard coverage + typematic test**, **serial port internal + external loopback**, **printer LPT output with user confirmation**. The first two are plausible v0.5+ additions for CERBERUS; the last three are peripheral diagnostics outside CERBERUS's scope (system hardware, not attached devices).

## Failure handling — the "Possible Causes" catalog

QA-Plus pairs every failing test with a physical-component guide. The error summary enumerates which cards or components could be at fault (`C.P.U. Card`, `Multi-Function Card`, `Video Monitor`, `Video Cable`, `Keylock Assembly`, `Fixed Disk/Cable`, 20 categories total), and each category is followed by a paragraph explaining how to physically identify that component on an IBM-compatible AT chassis: "The C.P.U. card is identified by the RESET pushbutton on its rear panel bracket." The output ends with an explicit anti-false-confidence disclaimer: "The preceding error diagnoses are 'POSSIBLE CAUSES' only ... in most cases only ONE of the POSSIBLE CAUSES is actually at fault."

Forensic-value emit (the CACHECHK Phase-3 pattern) is present. RAM errors print `Addr=XXXX:XXXX Bad bits: XXXXX` with the raw bitmask and segment, and the user is taught to aggregate: "You can probably disregard the error messages below if the segment part of the addresses are the same, and the 'bad bits' are all dots." QARAM then consumes the same raw address and translates it into a specific chip on the board diagram. This is the deepest forensic chain of any tool in the Homage corpus: raw bitmask → address aggregate → physical chip, with the tool explicitly teaching the reader how to interpret each layer.

**CERBERUS gap**: CERBERUS emits verdicts and crumbs but no "possible causes" mapping and no address-to-chip physical translator. The physical translator is genuinely hard — it requires a user-maintained board map that can't be derived from the machine. For 486-era SIMM-based hardware the mapping is simpler (bank-to-SIMM-slot rather than chip-level), but it still needs per-machine configuration. Filed as v0.6+ concept, not v0.5.

## Commercial-grade UI patterns

Worth adopting where compatible with CERBERUS scope:

- Pull-down menu top bar with distinct drop-downs for Report, Test, Performance.
- Distinct `DISPLAY SYSTEM INFORMATION` mode (detection only, no tests) vs full-test mode vs select-specific-tests mode.
- Continuous / Multiple-Testing mode with orthogonal Stop-on-Error toggle for catching intermittent faults.
- Error log redirected to LPT1: for unattended overnight runs.
- F1 context help everywhere, separate help file per major subsystem.
- Command-line escape switches (`NoNDP`, `NoEMM`) to bypass probes that hang on buggy clone hardware — a direct precedent for CERBERUS's `/SKIP:` and `/ONLY:` flags.

Worth rejecting:

- OEM dealer name and phone baked into a user-editable `DIAGS.DAT` file, plus an explicit "CHANGE DEALER NAME AND/OR PHONE NUMBER" menu item. Couples the tool to a commercial service channel and is irrelevant for an open-source diagnostic.
- Color scheme switches as separate command-line flags (`B&W`, `Blue`, `Red`, `Green`) rather than a single config key.

## Anti-patterns — what CERBERUS should avoid

1. **Bundling a destructive low-level format tool with a diagnostic.** HDPREP1.COM ships in the same directory as the diagnostic binaries. The 3.11 release notes admit a silent bug where HDPREP overrode an entry of interleave=1 with interleave=2 (a silent data-reorganization corruption); the destructive tool had a long-lived defect that quietly violated user intent. CERBERUS should keep destructive utilities, if any, in a separate distribution channel.
2. **Embedding licensed third-party code without traceability.** QAPLUS1.COM credits Chesapeake Data Systems but offers no way to know which routines are CDS's. Any algorithmic lesson from this binary carries the risk that the algorithm is CDS's IP, not DiagSoft's. This is one reason no code from QA-Plus is transliterated into CERBERUS; lessons stay at the prose-description level.
3. **CE-cylinder fixed-disk write test by default.** QA-Plus does prompt the user to bypass, but the existence of a write test on supposedly-unused tracks at the end of a live disk is a long-tail data-loss risk. CERBERUS already rejects any destructive storage test; this tool confirms that rejection.
4. **Memory test that forces `<Ctrl+Alt+Del>` reboot on completion.** QAPLUS1's RAM test prints `TSRS, and other vectors could have been destroyed during memory testing ... You must now re-boot your machine.` Any test that mandates a reboot is incompatible with CERBERUS's "run from DOS, return cleanly to prompt" contract. If CERBERUS ever adds a full-RAM sweep, it belongs in a separate bootable binary (back to the four-binary argument).

## Recommended action for CERBERUS

1. **Ship now (lesson-only)**: document QA-Plus's four-binary split as precedent for any future full-RAM-sweep effort. No code change.
2. **v0.5+ candidate**: add `Checkerboard` and `Inv Checkerboard` patterns to `diag_mem.c`. Low effort, real diagnostic coverage gain for adjacent-cell coupling faults.
3. **v0.5+ candidate**: add a `diag_pic` module for 8259A interrupt-controller probe, paralleling the existing `diag_dma`.
4. **v0.5+ candidate**: surface a "possible causes" mapping alongside FAIL verdicts — a short per-verdict list of which physical components could be at fault. Starts as a static table keyed on verdict name.
5. **v0.6+ concept**: address-to-chip / address-to-SIMM-bank translator keyed on a user-supplied machine map file (the QARAM pattern). Deferred because it needs the board-map data format designed first and because CERBERUS lacks the TUI chrome for a visual board editor.
6. **Reject**: dealer-info system, OEM branding hooks, destructive tools bundled with diagnostic, third-party licensed code paths, forced-reboot test modes.

## Attribution

DiagSoft, Inc. (Scotts Valley, CA) produced QA-Plus versions 1.x through 3.x for DOS between approximately 1986 and 1990. Chesapeake Data Systems, Inc. licensed unspecified routines into QAPLUS1.COM. Copyright DiagSoft 1987, 1988, 1989. No binary is redistributed in the CERBERUS tree; the source copy lives in `C:\Development\Homage\qap_v312\` on the author's development machine. This doc contains no decompiled function bodies, no disassembly listings, and no verbatim transliterations of QA-Plus algorithms.
