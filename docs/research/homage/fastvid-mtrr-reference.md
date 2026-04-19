# FASTVID — MTRR reference, not a VLB tool

**Tool:** FASTVID 1.10 (`Homage/FASTVID/FASTVID.EXE`)
**Author:** John Hinkley, 1996 (`72466.1403@compuserve.com`)
**Companion:** FINDLFB.EXE (VESA linear-framebuffer probe)
**Runtime:** 32-bit DOS via DOS4GW / Watcom C/C++32
**License:** "This program is freely distributable" (author's embedded note)

## What it is — negative-space correction

The initial Phase 2 brief identified FASTVID as Bill Yerazunis's
VLB video acceleration TSR, relevant to CERBERUS issue #6 (slow
VLB bandwidth). Static-string extraction contradicts both
attributes. FASTVID 1.10 is John Hinkley's one-shot MTRR /
write-posting configurator for **Pentium Pro motherboards** using
the Intel 82450 chipset — not a TSR, not VLB-related, not
Yerazunis's. The Yerazunis tool is a different program not
currently in the Homage corpus.

## The concept FASTVID does illustrate

Write-combining, configured via Pentium Pro Memory Type Range
Registers (MTRRs), speeds up DOS video workloads by letting the
CPU buffer sequential writes to the VGA banked window
(0xA0000-0xBFFFF) and the SVGA linear framebuffer (VESA-reported
base) instead of committing them one by one. FASTVID's text notes
that enabling LFB write-combining "speeds up some DOS and Win95"
workloads; enabling banked VGA write-combining "will speed up
most DOS applications." The mechanism is CPU-side (MTRR +
write-posting MSRs), not chipset-side — which is why coverage is
82450-only and why the tool explicitly refuses to run on the
82440 (Natoma) chipset.

## CERBERUS relevance

- **Issue #6 (VLB bandwidth):** none. VLB is 486-era local-bus;
  MTRRs don't exist on 486. FASTVID cannot diagnose or resolve
  anything on VLB hardware.
- **bench_video, Pentium Pro+ hypothetical:** if CERBERUS ever
  extends its capture corpus to P6+ machines, MTRR
  write-combining state becomes a legitimate bench_video
  variance source between otherwise-identical hardware. Not a
  v0.4 concern; note filed for forward reference.
- **Methodology:** nothing to copy. FASTVID is a performance
  *mutator*; CERBERUS is a performance *measurer*. Orthogonal.

## Action

None. Issue #6 still open; the correct reference tool is not in
the current Homage corpus and requires a separate sourcing pass.
