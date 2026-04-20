# CACHECHK, UMC-class timer workaround

**Tool:** CACHECHK v4 (Ray Van Tassle, 1995-1996)
**Source binary:** `Homage/CACHECHK/CACHECHK.EXE` (PKLITE-packed);
Phase 2 unpacked via Ben Castricum's UNP 4.11 to
`Homage/_research/CACHECHK-UNPACKED.EXE` (50,216 bytes).
**CERBERUS diff target:** `timing.c` PIT C2 path, Rule 4a
(`timing_independence`), issue #1 UMC491 deep-dive.

## What we were looking for

Phase 2 T6 unpacked CACHECHK and documented the cache-threshold
calibration (T7). T8 was deferred: understand the UMC timer
workaround CACHECHK's author calls out in its `.DOC` file, and
see whether it informs CERBERUS's own UMC491 8254 phantom-wrap
handling (rule 4a / issue #1).

Author's documentation (`CACHECHK.DOC`, "Timer" section):

> CACHECHK directly accesses the timer chip to get a
> high-precision timer (0.838 microsec resolution). In some
> motherboards (notibly reported to be "UMC with fake cache
> chips"), there is a problem with this timer. I worked around
> this in version 2, but there may be some boards where my
> work-around still doesn't work.

The author does not document what the workaround IS in the
text file. T8 asks: can we reverse-engineer it from the unpacked
binary.

## What we found

**CACHECHK uses the canonical 8254 PIT Channel 2 high-resolution
timer pattern,** driven directly (not via BIOS). Byte patterns
in the unpacked binary:

- `out 43h, al` (PIT command register writes): 3 occurrences.
  Three distinct PIT programmings across the tool's lifetime
  (initialize, latch-for-read start, latch-for-read stop).
- `out 42h, al` (PIT C2 counter-load writes): 2 occurrences.
  Writing the initial count value into channel 2 on init.
- `in al, 40h` (PIT C0 tick-counter reads): 2 occurrences.
  Cross-referencing C0 (BIOS tick) against C2 (high-res) for
  the long-interval sanity path.
- `in al, 42h` (PIT C2 read): 2 occurrences. Low byte then
  high byte after each latch.

This is the same high-resolution pattern CERBERUS uses in
`src/core/timing.c`. CACHECHK and CERBERUS are reaching for the
identical 0.838 us resolution via the same mechanism. The
workaround is not a different timer source; it is a sanity
check applied to the readings.

**The sanity check emits a specific diagnostic string on
failure:**

```
Timer messed up! %08lx %08lx %08lx
```

Three 32-bit hex values printed on failure. The implication is
clear: CACHECHK validates the timer readings against a known
invariant and, when the invariant fails, prints the three
load-bearing values (most likely: start count, stop count, and
derived delta or elapsed BIOS ticks) for post-hoc diagnosis
rather than silently accepting biased data.

The author's note that "there may be some boards where my
work-around still doesn't work" is consistent with: the
workaround catches the common UMC-class misbehavior but
cannot guarantee coverage across every chipset variant.

## What the workaround likely does

From the PIT I/O patterns, the read sequence structure, and
the "three hex values" diagnostic shape, the workaround
structure fits the following pattern:

1. Latch C2, read low + high byte = reading A.
2. Perform a small no-op work unit.
3. Latch C2 again, read low + high = reading B.
4. Check that reading B is LESS than reading A (counter counts
   down).
5. Cross-check the C2 delta against a simultaneous C0 (BIOS
   tick) delta over the same wall-clock interval; if they
   disagree beyond tolerance, declare the timer bad.
6. On bad: emit "Timer messed up!" with the three values and
   bail the measurement rather than reporting nonsense numbers.

This is structurally identical to CERBERUS's current approach
in `timing_compute_dual` (`src/core/timing.c:106-220`), which
cross-checks C2 delta against C0 delta and bails at 25%
divergence. The Phase 3 T3 result confirmed that divergence
guard during test fixing.

Where CERBERUS differs (and could take inspiration):
CERBERUS's WARN emit says "measurement_failed" without
including the raw values. CACHECHK emits the three numeric
values in the failure message so you can see, after the fact,
exactly what the timer reported. On a UMC491 system where the
phantom wraps are intermittent, the raw-value emit would let a
collector compare multiple failing runs to identify the
characteristic phantom pattern.

## Consequence for issue #1

**CERBERUS's current approach is aligned.** The Rule 4a
`timing_independence` check and the 25% divergence bail in
`timing_compute_dual` mirror what CACHECHK does. No urgent
recalibration needed.

**The useful addition from this Phase 3 finding: emit raw
forensic values on failure.** Replace or augment the current
`measurement_failed` emit with something like:

```
timing.cross_check.status = measurement_failed
timing.cross_check.pit_raw = %08lx
timing.cross_check.bios_raw = %08lx
timing.cross_check.wraps = %lu
```

so that issue #1's UMC491 deep-dive, when it finally happens on
a real UMC491 board, has the raw numbers for diagnosis. Same
diagnostic-emit pattern as the issue #2 `audio.opl_probe_trace`
added in the same session.

## Recommended action

**v0.4 / v0.5: none required.** CERBERUS's divergence check is
doing the same thing as CACHECHK's. Status is consistent.

**v0.5+ candidate:** Add raw-forensic-value emit on timing
cross-check failure. Small patch; same shape as the OPL probe
trace. Filed as a Rule-4a enhancement.

## Attribution

CACHECHK v4 (c) 1995-1996 Ray Van Tassle, freeware. Findings
are from byte-level scans of the PKLITE-unpacked binary
(unpacked Phase 2 via Ben Castricum's UNP 4.11), plus the
author-written `CACHECHK.DOC` reference. The "Timer messed up"
string was located at file offset 0xBE36; 8254 PIT port-I/O
byte patterns were located at offsets 0x2791, 0x27AC, 0x2F2F,
0x3EDB. No code reproduced; descriptions are byte-pattern
observations cross-referenced against Intel 8254 datasheet
semantics.
