# CheckIt — PC-XT baseline derivation

**Tool:** CheckIt 3.0, Jan 1991 (`Homage/Checkit/CHECKIT.EXE`)
**Author:** TouchStone Software Corporation, Copyright 1988-1990
**Runtime:** 16-bit DOS MZ + MS-C overlay linker (.OVL), Vermont
Creative Software "Windows for Data/C" TUI
**CERBERUS diff target:** `bench.cpu.dhrystones`,
`bench.fpu.whetstones`, `bench.video.*` rating / comparison
semantics

## What CheckIt does, observed

CheckIt compares each measured benchmark against a **hardcoded
reference machine** and prints `Rating: N.NN times <baseline>`.
The default baseline label is literally `IBM PC-XT`. Users may
replace the baseline via "S-Save for Compare" — the current
machine's results are serialized into `CHECKIT.CNF` under
`bm_dstones`, `bm_wstones`, `bm_bvideo`, `bm_dvideo`,
`bm_transfer`, `bm_aveseek`, `bm_trkseek`, each with a trailing
user-supplied 10-character label. The `IBM PC-XT` entry is not
present in CHECKIT.CNF; it lives inside the EXE.

From the benchmark help text (`HELP:bench_main` in CHECKIT.HLP):

> The upper window graphically shows this measurement against a
> 4.77 MHz original PC/XT machine.

CheckIt's PC-XT anchor is therefore a **4.77 MHz IBM PC/XT**, and
the stored reference is a hardcoded Dhrystones/second (and
matching constants for Whetstone + video CPS) baked into the
binary.

## Static-analysis limits

The benchmark rendering functions that consume the PC-XT
constants live in CheckIt's overlays (`CHECKIT.OVL`, loaded at
runtime by the MS-C overlay manager). Direct 16-bit-immediate
xrefs to "Rating: %5.2f times %s" and "IBM PC-XT" resolve only
from *root-module* code; references from overlaid code use
overlay-manager indirect addressing and are not visible to a
static xref scan. A static sweep of double-precision
floating-point literals in plausible PC-XT ranges surfaces
hundreds of candidates without a clean way to disambiguate
benchmark baselines from display constants, unrelated numeric
literals, and misaligned garbage.

**Takeaway:** the exact PC-XT Dhrystones-per-second constant is
not cleanly recoverable from static analysis alone. The correct
empirical path is to run CheckIt on known hardware, read the
displayed rating, and back-compute the constant.

## What CERBERUS currently does

`bench_dhrystone` emits **absolute** Dhrystones/second — no
rating, no baseline. The bench comment at `bench_dhrystone.c:30`
notes:

> CheckIt reference on the BEK-V409 bench box: 33,609 Dhrystones/
> sec at DSP-measured 66.74 MHz. Our port targets ±5% of that
> number.

CERBERUS is calibrated against **CheckIt's absolute number on
matched hardware**, not against a derived PC-XT ratio. The
baseline anchor is CheckIt's output on the same machine, not
CheckIt's internal PC-XT constant.

## Gap analysis

**None requiring action.** CERBERUS reports an absolute
measurement; CheckIt reports a measurement plus a derived ratio
against a hardcoded reference. A CheckIt-style rating is a
display convention that CERBERUS could add, but nothing in
issue #4 requires it. The ±5% matching claim at the absolute
level is what makes numbers comparable between the two tools;
the PC-XT ratio is derived and does not need to be replicated.

A hypothetical future "CheckIt-compatible rating" feature in
CERBERUS would need:
1. The hardcoded PC-XT Dhrystone constant extracted empirically
2. Matching Whetstone + video CPS constants
3. A `rating = measured / pc_xt_ref` formatter

Not a v0.4 or v0.5 deliverable; raised only to document the
shape of the dependency.

## Recommended action

**None.** CERBERUS's absolute-number approach is sound and
directly comparable to CheckIt on matched hardware, which is
what issue #4 calibration requires. The PC-XT ratio is a
display-layer convention, not a measurement methodology.

## Attribution

CheckIt 3.0 (c) 1988-1990 TouchStone Software Corporation. All
observations from self-documenting help text (`CHECKIT.HLP`),
sample configuration file (`CHECKIT.CNF`), and static analysis
of the MZ binary. No decompiled code reproduced.
