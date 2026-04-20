# LM60, Landmark System Speed Test 6.0

**Tool:** Landmark System Speed Test 6.0 (LM60)
**Author:** Landmark Research International Corporation
**Source binary:** `Homage/LM60/LM60.EXE` (16-bit DOS MZ,
43,518 bytes, tightly packed text segments; no recognized
packer signature)
**CERBERUS diff target:** rating-vs-absolute rendering; v0.5+
methodology reference

## What we were looking for

T16 asked whether LM60, the headline "Landmark Speed"
benchmark that defined DOS-era CPU rating for more than a
decade, provides methodology that CERBERUS should study or
adopt. I had LM60 on the Tier 2 acquisition list as a
"distinguished DOS utility" to obtain; it turns out to have
already been in the Homage corpus, so the acquisition step was
a no-op.

## What we found

**Attribution:** explicit copyright string near the top of the
binary: `(C) 1993 Landmark Research International Corporation`.
LM60 is the sixth major version of Landmark's System Speed Test
line; v1.0 dates back to roughly 1986 and defined the
"Landmark Speed" rating that was ubiquitous in DOS-era PC
reviews and benchmark tables.

**Accessible strings in the binary are minimal.** Beyond the
copyright line, only fragments of format strings and command-
line help tokens were recoverable via passive string scans:

- `%e3.2f MHz` (or similar sprintf format; MHz value
  formatter)
- `SPEED /B.L@-` (partial command-line usage hint)

The binary is tightly-packed but not via any recognized packer
signature (no PKLITE, LZEXE, EXEPACK, DIET, or UPX markers).
Deeper methodology details would require disassembly, which
is out of scope for Phase 3.

**The Landmark Speed concept, externally documented:**

Landmark Speed is a CPU-IPC rating expressed in "equivalent
MHz of an IBM PC/XT" (4.77 MHz 8088). A 25 MHz 386DX might be
rated at "Landmark Speed: 63 MHz" meaning its integer
throughput equals that of a hypothetical 63 MHz 8088. The
number is NOT the CPU's actual clock; it is a throughput
rating.

This uses the same IBM PC-XT anchor as CheckIt (Phase 2 T1
found CheckIt's `bm_dstones` reference machine is the same
4.77 MHz 8088). The two benchmarks independently chose the
same normalization target, which is natural for the era:
the PC/XT was the universal reference every DOS user's system
was "faster than by some factor."

LM60 also reports a **Video Speed** number in "VGA times
faster than PC/XT" form. If LM60 runs on the BEK-V409 and
reports e.g. "Video Speed: 80x PC/XT", that is a text-mode
measurement (LM60's Video Speed pre-dates graphics-mode
benchmarks) and would need to be converted through the same
chars-per-second bridge documented in the T5 CheckIt video
lesson to compare against CERBERUS.

## Consequence for CERBERUS

**The same PC-XT anchor pattern is already acknowledged** in
Phase 2's `checkit-pcxt-baseline.md` lesson. LM60 is another
data point: the PC-XT reference was an era convention, not a
tool-specific choice. CERBERUS's absolute-number approach
sidesteps the normalization decision entirely.

**CERBERUS could optionally add a "Landmark-style rating"
display.** Same shape as the deferred "CheckIt-compatible
rating" discussion in `checkit-pcxt-baseline.md`. Would need
an empirical Landmark Speed reference point on a known box,
then `rating = measured_iters / lm60_xt_iters` for display.
Low-value; the absolute KB/s and absolute Dhrystone numbers
CERBERUS already emits are more useful as raw data for future
comparison.

**No Landmark Speed target for CERBERUS.** Same reasoning as
the CheckIt Dhrystone non-equivalence (Phase 2 T2): LM60's
number measures its own synthetic, and directly aiming for
agreement would be methodologically meaningless.

## Recommended action

**No changes to CERBERUS from this research.** LM60 is a
contemporaneous peer to CheckIt, using the same PC-XT
convention. Mentioned in the Homage record for completeness
and attribution. If CERBERUS ever wants a comparative-rating
display, LM60 is one of the three (CheckIt, Landmark, SPEEDSYS)
reference sources whose anchors would need to be seeded.

## Attribution

Landmark System Speed Test 6.0 (c) 1993 Landmark Research
International Corporation. Commercial software, distributed
widely through the DOS-era PC community. Findings are from
byte-level scans of the binary without unpacking. No code
reproduced; results are from the visible-string fragments
cross-referenced against the externally-documented Landmark
Speed methodology (widely cited in DOS-era PC hardware
reviews; no single authoritative secondary source is named
here because the rating convention was ambient in the
industry from 1986 to roughly 1996).
