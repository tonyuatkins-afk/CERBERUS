#!/usr/bin/env python3
"""DGROUP budget audit for CERBERUS.EXE.

Parses cerberus.map (OpenWatcom wlink output) and reports near-data
usage segment-by-segment plus total. Gates M2 per 0.8.0 plan §4
ship criterion 4: DGROUP near-data must be <= 62 KB at tag, with a
2 KB hard-reserve for future patch-level additions. Under the DOS
64 KB hard ceiling at all times.

Usage:
    python tools/dgroup_check.py                 # default map path
    python tools/dgroup_check.py <path/to/map>   # explicit map path
    python tools/dgroup_check.py --strict        # nonzero exit on budget fail

Map file grammar expected:

    DGROUP                          2089:0000            0000ea00

followed by per-segment rows:

    Segment         Class   Group    Address     Size
    _NULL           BEGDATA DGROUP   2089:0000   00000020
    CONST           DATA    DGROUP   208b:0002   00005e00
    ...

All DGROUP-grouped rows are summed; the header figure is reported
for cross-check.
"""

import argparse
import pathlib
import re
import sys


DOS_HARD_CEILING = 65536
SOFT_TARGET = 62 * 1024
HARD_RESERVE = 2 * 1024


SEG_RE = re.compile(
    r"^(?P<seg>\S+)\s+(?P<cls>\S+)\s+DGROUP\s+"
    r"[0-9a-f]+:[0-9a-f]+\s+(?P<size>[0-9a-f]+)\s*$",
    re.IGNORECASE,
)
GROUP_RE = re.compile(
    r"^DGROUP\s+[0-9a-f]+:[0-9a-f]+\s+(?P<size>[0-9a-f]+)\s*$",
    re.IGNORECASE,
)


def parse_map(path):
    group_total = None
    segments = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if group_total is None:
                m = GROUP_RE.match(line)
                if m:
                    group_total = int(m.group("size"), 16)
                    continue
            m = SEG_RE.match(line)
            if m:
                segments.append(
                    (m.group("seg"), m.group("cls"), int(m.group("size"), 16))
                )
    return group_total, segments


def fmt_kb(b):
    return "{:>5} B ({:>5.1f} KB)".format(b, b / 1024.0)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument(
        "map_path",
        nargs="?",
        default="cerberus.map",
        help="path to the linker map file (default: cerberus.map)",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="nonzero exit if DGROUP exceeds the M2 soft target",
    )
    args = parser.parse_args()

    path = pathlib.Path(args.map_path)
    if not path.exists():
        print("ERROR: map file not found: {}".format(path), file=sys.stderr)
        print("Build with `wmake` first.", file=sys.stderr)
        return 2

    group_total, segments = parse_map(path)
    if group_total is None:
        print(
            "ERROR: no DGROUP header found in {}".format(path), file=sys.stderr
        )
        return 2
    if not segments:
        print(
            "ERROR: no DGROUP segments found in {}".format(path),
            file=sys.stderr,
        )
        return 2

    segments.sort(key=lambda row: -row[2])
    summed = sum(size for _, _, size in segments)

    print("=" * 72)
    print("CERBERUS DGROUP AUDIT -- {}".format(path))
    print("=" * 72)
    print()
    print("{:<22} {:<10} {}".format("Segment", "Class", "Size"))
    print("-" * 72)
    for name, cls, size in segments:
        print("{:<22} {:<10} {}".format(name, cls, fmt_kb(size)))
    print("-" * 72)
    print("{:<33} {}".format("SUMMED (segment rows):", fmt_kb(summed)))
    print(
        "{:<33} {}".format(
            "REPORTED (DGROUP header):", fmt_kb(group_total)
        )
    )
    if summed != group_total:
        print(
            "  NOTE: sum differs from header by {} bytes (alignment padding)".format(
                group_total - summed
            )
        )
    print()
    print("Budgets (0.8.0 plan Section 4):")
    print(
        "  DOS hard ceiling:        {} bytes  ({:>5.1f} KB)".format(
            DOS_HARD_CEILING, DOS_HARD_CEILING / 1024.0
        )
    )
    print(
        "  0.8.0 soft target:       {} bytes  ({:>5.1f} KB)".format(
            SOFT_TARGET, SOFT_TARGET / 1024.0
        )
    )
    print(
        "  Hard reserve for patches: {} bytes  ({:>5.1f} KB)".format(
            HARD_RESERVE, HARD_RESERVE / 1024.0
        )
    )
    print()

    headroom_hard = DOS_HARD_CEILING - group_total
    headroom_soft = SOFT_TARGET - group_total
    print(
        "  Headroom vs hard ceiling:  {} bytes".format(headroom_hard)
    )
    print(
        "  Headroom vs soft target:   {} bytes".format(headroom_soft)
    )
    print()

    status = "OK"
    rc = 0
    if group_total >= DOS_HARD_CEILING:
        status = "HARD FAIL"
        rc = 3
    elif group_total > SOFT_TARGET:
        status = "OVER SOFT TARGET"
        rc = 1 if args.strict else 0
    elif headroom_soft < HARD_RESERVE:
        status = "AT RISK"
        rc = 0
    print("STATUS: {}".format(status))
    if status == "OVER SOFT TARGET":
        print(
            "  Past the 62 KB soft target. M2 feature gate per 0.8.0 plan"
        )
        print("  Section 4 requires trimming before adding M2 features.")
    elif status == "AT RISK":
        print(
            "  Under target but within the 2 KB hard-reserve window."
        )
        print("  Treat as yellow: proceed with caution, trim when you can.")
    elif status == "HARD FAIL":
        print(
            "  EXCEEDS DOS 64 KB HARD CEILING. The binary may fail to load."
        )
    print()
    return rc


if __name__ == "__main__":
    sys.exit(main())
