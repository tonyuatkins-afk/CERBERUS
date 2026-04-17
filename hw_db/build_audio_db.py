#!/usr/bin/env python3
"""Generate src/detect/audio_db.c from hw_db/audio.csv."""

from __future__ import annotations
import csv, sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
CSV_PATH  = REPO_ROOT / "hw_db" / "audio.csv"
OUT_PATH  = REPO_ROOT / "src" / "detect" / "audio_db.c"


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
    seen = set()
    for r in rows:
        key = (r.get('match_key') or '').strip()
        line = r['_line']
        if not key:
            errors.append(f"line {line}: match_key required")
            continue
        if key in seen:
            errors.append(f"line {line}: duplicate match_key '{key}'")
        seen.add(key)
        if not (r.get('friendly') or '').strip():
            errors.append(f"line {line}: friendly required")
    return errors


def emit_c(rows, out_path):
    lines = [
        '/*',
        ' * AUTO-GENERATED — DO NOT EDIT.',
        f' * Regenerate with: python hw_db/build_audio_db.py',
        f' * Source: hw_db/audio.csv ({len(rows)} entries)',
        ' */',
        '',
        '#include "audio_db.h"',
        '#include <string.h>',
        '',
        'const audio_db_entry_t audio_db[] = {',
    ]
    for r in rows:
        mk  = c_escape((r.get('match_key') or '').strip())
        frn = c_escape((r.get('friendly')  or '').strip())
        ven = c_escape((r.get('vendor')    or '').strip())
        nts = c_escape((r.get('notes')     or '').strip())
        lines.append(f'    {{ {mk}, {frn}, {ven}, {nts} }},')
    lines += [
        '};',
        '',
        f'const unsigned int audio_db_count = {len(rows)};',
        '',
        'const audio_db_entry_t *audio_db_lookup(const char *match_key)',
        '{',
        '    unsigned int i;',
        '    if (!match_key) return (const audio_db_entry_t *)0;',
        '    for (i = 0; i < audio_db_count; i++) {',
        '        if (strcmp(audio_db[i].match_key, match_key) == 0) return &audio_db[i];',
        '    }',
        '    return (const audio_db_entry_t *)0;',
        '}',
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
