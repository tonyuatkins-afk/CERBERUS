#!/usr/bin/env python3
"""Generate src/detect/video_db.c from hw_db/video.csv.

Same pattern as build_cpu_db.py / build_fpu_db.py. Entries are matched
by substring scan of the video BIOS ROM — top-to-bottom, first match
wins, so order matters and should go specific-before-general.
"""

from __future__ import annotations
import csv
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
CSV_PATH  = REPO_ROOT / "hw_db" / "video.csv"
OUT_PATH  = REPO_ROOT / "src" / "detect" / "video_db.c"


def c_escape(s: str) -> str:
    if s is None:
        return '""'
    out = ['"']
    for ch in s:
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif 32 <= ord(ch) < 127:
            out.append(ch)
        else:
            out.append(f'\\x{ord(ch):02x}')
    out.append('"')
    return ''.join(out)


def parse_csv():
    rows = []
    with CSV_PATH.open(newline='', encoding='utf-8') as f:
        data_lines = [l for l in f if not l.lstrip().startswith('#') and l.strip()]
    reader = csv.DictReader(data_lines)
    for i, row in enumerate(reader):
        row['_line'] = i + 2
        rows.append(row)
    return rows


def validate(rows):
    errors = []
    valid_families = {'mda', 'cga', 'hercules', 'ega', 'vga', 'svga'}
    seen_sigs = set()
    for row in rows:
        line = row['_line']
        sig = (row.get('bios_signature') or '').strip()
        if not sig:
            errors.append(f"line {line}: bios_signature is required")
        elif sig in seen_sigs:
            errors.append(f"line {line}: duplicate bios_signature '{sig}'")
        seen_sigs.add(sig)
        fam = (row.get('family') or '').strip()
        if fam not in valid_families:
            errors.append(f"line {line}: family '{fam}' not in {sorted(valid_families)}")
        if not (row.get('chipset') or '').strip():
            errors.append(f"line {line}: chipset is required")
    return errors


def emit_entry(row) -> str:
    sig     = c_escape((row.get('bios_signature') or '').strip())
    vendor  = c_escape((row.get('vendor')         or '').strip())
    chipset = c_escape((row.get('chipset')        or '').strip())
    family  = c_escape((row.get('family')         or '').strip())
    notes   = c_escape((row.get('notes')          or '').strip())
    return f'    {{ {sig}, {vendor}, {chipset}, {family}, {notes} }},'


def emit_c(rows, out_path: Path):
    lines = [
        '/*',
        ' * AUTO-GENERATED — DO NOT EDIT.',
        ' *',
        f' * Regenerate with: python hw_db/build_video_db.py',
        f' * Source: hw_db/video.csv ({len(rows)} entries)',
        ' */',
        '',
        '#include "video_db.h"',
        '#include <string.h>',
        '',
        'const video_db_entry_t video_db[] = {',
    ]
    for row in rows:
        lines.append(emit_entry(row))
    lines += [
        '};',
        '',
        f'const unsigned int video_db_count = {len(rows)};',
        '',
    ]
    out_path.write_text('\n'.join(lines), encoding='utf-8')


def main() -> int:
    rows = parse_csv()
    errs = validate(rows)
    if errs:
        for e in errs:
            print(f"ERROR: {e}", file=sys.stderr)
        return 1
    emit_c(rows, OUT_PATH)
    print(f"wrote {OUT_PATH.relative_to(REPO_ROOT)} with {len(rows)} entries")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
