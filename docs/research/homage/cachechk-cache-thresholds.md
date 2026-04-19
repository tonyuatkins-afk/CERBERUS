# CACHECHK — cache-boundary detection and threshold calibration

**Tool:** CACHECHK v4 (2/7/96) (`Homage/CACHECHK/CACHECHK.EXE`)
**Author:** Ray Van Tassle, Algonquin IL, "postcard-ware"
**Runtime:** 16-bit DOS MZ + PKLITE v1.15 packed; unpacked to
`_research/CACHECHK-UNPACKED.EXE` via UNP 4.11
**CERBERUS diff target:** `diag_cache` thresholds (issue #5
calibration), `bench_cache` sizing, cache topology reporting

## What CACHECHK does, observed

CACHECHK **sweeps memory-access throughput across many block
sizes** (1 KB, 2 KB, 4 KB, 8 KB, 16 KB, 32 KB, 64 KB, 128 KB,
256 KB, 512 KB, 1 MB, 2 MB, 4 MB) and infers cache structure by
counting **plateaus** in the resulting throughput curve. From the
author's own doc file, re-emitted here only as numeric measurements
from the doc's sample table:

| CPU + cache           | L1 ns/B | L2 ns/B | RAM ns/B | L1/RAM | L2/RAM |
|-----------------------|---------|---------|----------|--------|--------|
| 386/25SL + 64 K L2    | —       | 80.2    | 108      | —      | 1.35×  |
| 386/25 + 64 K L2      | —       | 59.2    | 90       | —      | 1.52×  |
| 486/33 + 8 K + 128 K  | 30.7    | 43.6    | 70       | 2.28×  | 1.60×  |
| 486/66 + 8 K          | 16.1    | —       | 50       | 3.11×  | —      |
| 486/100 AMD + 8 K     | 11.1    | —       | 46       | 4.15×  | —      |
| 486/100 Intel + 16 K + 256 K | 10.0 | 18.8 | 26        | 2.60×  | 1.38×  |
| P-75 + 8 K + 256 K    | 10.2    | 16.4    | 24       | 2.35×  | 1.46×  |

**Classification-string signal** from the unpacked binary confirms
the plateau-counting algorithm. Three mutually exclusive status
strings exist:
- "seems to have one cache!?" — two plateaus detected
- "seems to have both L1 and L2 cache." — three plateaus
- "seems to have %d caches! (This can't be right.)" — anything else

The implicit per-plateau threshold is roughly "~15-20% throughput
jump between adjacent block sizes signals a new plateau." The DOC
file describes the detection as "by inspection, we see there are
two breakpoints in the memory access speed. The first at 16kb,
the second at 256kb" — a visual / heuristic check, not a single
numeric threshold.

## What CERBERUS currently does

`diag_cache` measures throughput at exactly **two** buffer sizes
(2 KB and 32 KB) and computes a **per-line** ratio
(large_per_line_us / small_per_line_us). Verdict table at
`src/diag/diag_cache.c:131`:

- `ratio_x100 ≥ 130` (≥ 1.30×) → PASS, "cache_working"
- `ratio_x100 ∈ [120, 130)` → WARN, "partial"
- `ratio_x100 ∈ [100, 120)` → FAIL, "no_cache_effect"
- `ratio_x100 < 100` → WARN, "anomaly" (physically impossible)

`bench_cache` adds four throughput numbers (small_read,
small_write, large_read, large_write) for relative comparison;
it does not do cache-level classification.

## Gap analysis

**Different questions.** CACHECHK answers "how many cache levels
are there and what are their sizes?" CERBERUS `diag_cache`
answers "is the L1 cache present and working?" Both answers are
correct for their scope. CACHECHK is a topology tool; CERBERUS
`diag_cache` is a health tool.

**Threshold alignment.** CERBERUS's 1.30× per-line PASS threshold
is **conservative** relative to CACHECHK's sample table:

- CACHECHK's weakest real-hardware cache signal is the 386/25SL
  at 1.35× (L2 vs RAM). CERBERUS would correctly PASS this at
  1.30×.
- The 386/25 desktop at 1.52× clears CERBERUS's PASS threshold by
  a wide margin.
- All 486 and P-75 entries show ≥ 2× for their dominant cache
  level — far above CERBERUS's threshold.

The only case CERBERUS might false-FAIL is a system where only
an L2 exists (no L1) AND the L2/RAM ratio is below 1.30×.
CACHECHK's sample table shows one such configuration (386/25SL
at 1.35×) where CERBERUS would still PASS. No gap here.

**Methodology note — plateau detection vs. two-point.**
CACHECHK's 12-point sweep gives it resolution to detect both L1
and L2 boundaries in one pass. CERBERUS's two-point test cannot
distinguish "machine has only L1" from "machine has L1 + L2 that
both fit in our 2 KB buffer"; in both cases it reports
`cache_working`. This matches CERBERUS's scope (health not
topology). If CERBERUS ever wants topology, the CACHECHK
plateau-sweep pattern is the natural model.

**On the first-real-iron calibration data-point** (BEK-V409 i486
DX-2, per_line ratio 1.71× at
`diag_cache.c:99-101`): 1.71× sits between CACHECHK's 486/66
(3.11×) and 486/33 (2.28×) entries, **lower than both**. The
BEK-V409 result is consistent with TSR-contended BIOS config
reducing the speedup below clean-configuration numbers — as
already noted in `diag_cache.c`.

## Recommended action

**None.** CERBERUS `diag_cache` is correctly calibrated; the
1.30× threshold is well inside the envelope CACHECHK's
real-hardware table establishes. The reformulation in commit
`25306b4` (issue #5 fix: per-line instead of per-traversal) was
the right call and the chosen threshold holds up against
CACHECHK's broader sample.

**Deferred idea for v0.5 or later:** a sweep-mode diagnostic that
measures 6-8 block sizes and counts plateaus, bringing CERBERUS
closer to CACHECHK's topology answer for machines that want
"what cache levels are present, and how big?" Would replace
nothing; adds a `/DEEP:CACHE` verbose mode.

## Attribution

CACHECHK v4 by Ray Van Tassle, 1995-1996. Postcard-ware per
author's note. All algorithmic descriptions above are synthesized
from the author's own CACHECHK.DOC file + static-string analysis
of the UNP-unpacked binary. No code reproduced in this document.
