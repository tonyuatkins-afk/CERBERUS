#!/usr/bin/env python3
"""Generate src/detect/fpu_db.c from hw_db/fpus.csv.

Schema:
    tag       — unique string key produced by detect_fpu
    friendly  — human-readable name (required)
    vendor    — manufacturer/class (optional)
    notes     — brief caveat (optional)

Follows the same pattern as hw_db/build_cpu_db.py.
"""

from __future__ import annotations
import csv
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
CSV_PATH  = REPO_ROOT / "hw_db" / "fpus.csv"
OUT_PATH  = REPO_ROOT / "src" / "detect" / "fpu_db.c"


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
    seen = set()
    for row in rows:
        tag = (row.get('tag') or '').strip()
        line = row['_line']
        if not tag:
            errors.append(f"line {line}: tag is required")
            continue
        if tag in seen:
            errors.append(f"line {line}: duplicate tag '{tag}'")
        seen.add(tag)
        if not (row.get('friendly') or '').strip():
            errors.append(f"line {line}: friendly name is required for tag '{tag}'")
    return errors


def emit_entry(row) -> str:
    tag      = c_escape((row.get('tag')      or '').strip())
    friendly = c_escape((row.get('friendly') or '').strip())
    vendor   = c_escape((row.get('vendor')   or '').strip())
    notes    = c_escape((row.get('notes')    or '').strip())
    return f'    {{ {tag}, {friendly}, {vendor}, {notes} }},'


def emit_c(rows, out_path: Path):
    lines = [
        '/*',
        ' * AUTO-GENERATED — DO NOT EDIT.',
        ' *',
        f' * Regenerate with: python hw_db/build_fpu_db.py',
        f' * Source: hw_db/fpus.csv ({len(rows)} entries)',
        ' */',
        '',
        '#include "fpu_db.h"',
        '#include <string.h>',
        '',
        'const fpu_db_entry_t fpu_db[] = {',
    ]
    for row in rows:
        lines.append(emit_entry(row))
    lines += [
        '};',
        '',
        f'const unsigned int fpu_db_count = {len(rows)};',
        '',
        'const fpu_db_entry_t *fpu_db_lookup(const char *tag)',
        '{',
        '    unsigned int i;',
        '    if (!tag) return (const fpu_db_entry_t *)0;',
        '    for (i = 0; i < fpu_db_count; i++) {',
        '        if (strcmp(fpu_db[i].tag, tag) == 0) return &fpu_db[i];',
        '    }',
        '    return (const fpu_db_entry_t *)0;',
        '}',
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
