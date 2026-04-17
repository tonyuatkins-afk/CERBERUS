#!/usr/bin/env python3
"""Generate src/detect/bios_db.c from hw_db/bios.csv."""

from __future__ import annotations
import csv, sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CSV_PATH  = REPO_ROOT / "hw_db" / "bios.csv"
OUT_PATH  = REPO_ROOT / "src" / "detect" / "bios_db.c"


def c_escape(s):
    if s is None: return '""'
    out = ['"']
    for ch in s:
        if ch == '\\':    out.append('\\\\')
        elif ch == '"':   out.append('\\"')
        elif ch == '\n':  out.append('\\n')
        elif 32 <= ord(ch) < 127: out.append(ch)
        else: out.append(f'\\x{ord(ch):02x}')
    out.append('"')
    return ''.join(out)


def parse_csv():
    with CSV_PATH.open(newline='', encoding='utf-8') as f:
        data_lines = [l for l in f if not l.lstrip().startswith('#') and l.strip()]
    rows = list(csv.DictReader(data_lines))
    for i, r in enumerate(rows):
        r['_line'] = i + 2
    return rows


def validate(rows):
    errors = []
    seen_sigs = set()
    for r in rows:
        sig = (r.get('signature') or '').strip()
        line = r['_line']
        if not sig:
            errors.append(f"line {line}: signature required")
            continue
        if sig in seen_sigs:
            errors.append(f"line {line}: duplicate signature '{sig}'")
        seen_sigs.add(sig)
        if not (r.get('vendor') or '').strip():
            errors.append(f"line {line}: vendor required")
        if not (r.get('family') or '').strip():
            errors.append(f"line {line}: family required")
    return errors


def emit_c(rows, out_path):
    lines = [
        '/*',
        ' * AUTO-GENERATED — DO NOT EDIT.',
        f' * Regenerate with: python hw_db/build_bios_db.py',
        f' * Source: hw_db/bios.csv ({len(rows)} entries)',
        ' */',
        '',
        '#include "bios_db.h"',
        '',
        'const bios_db_entry_t bios_db[] = {',
    ]
    for r in rows:
        sig = c_escape((r.get('signature') or '').strip())
        ven = c_escape((r.get('vendor')    or '').strip())
        fam = c_escape((r.get('family')    or '').strip())
        era = c_escape((r.get('era')       or '').strip())
        nts = c_escape((r.get('notes')     or '').strip())
        lines.append(f'    {{ {sig}, {ven}, {fam}, {era}, {nts} }},')
    lines += [
        '};',
        '',
        f'const unsigned int bios_db_count = {len(rows)};',
        '',
    ]
    out_path.write_text('\n'.join(lines), encoding='utf-8')


def main():
    rows = parse_csv()
    errs = validate(rows)
    if errs:
        for e in errs: print(f"ERROR: {e}", file=sys.stderr)
        return 1
    emit_c(rows, OUT_PATH)
    print(f"wrote {OUT_PATH.relative_to(REPO_ROOT)} with {len(rows)} entries")
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
