#!/usr/bin/env python3
"""Generate src/detect/cpu_db.c from hw_db/cpus.csv.

The CSV is the source of truth. Contributors edit it, run this script,
commit both the CSV and the regenerated C. The Makefile has a `regen`
target that invokes this.

Validation:
  - `match_kind` must be 'cpuid' or 'legacy'
  - cpuid rows must have vendor + family + model + stepping_min <= stepping_max
  - legacy rows must have a non-empty legacy_class
  - No duplicate CPUID (vendor, family, model, stepping_min, stepping_max)
  - No duplicate legacy_class
"""

from __future__ import annotations
import csv
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
CSV_PATH  = REPO_ROOT / "hw_db" / "cpus.csv"
OUT_PATH  = REPO_ROOT / "src" / "detect" / "cpu_db.c"


def c_escape(s: str) -> str:
    """Escape a string for inclusion in a C string literal."""
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
        # Skip comment lines
        data_lines = [l for l in f if not l.lstrip().startswith('#') and l.strip()]
    reader = csv.DictReader(data_lines)
    for i, row in enumerate(reader):
        row['_line'] = i + 2  # +1 for header, +1 for 1-based
        rows.append(row)
    return rows


def validate(rows):
    errors = []
    seen_cpuid = set()
    seen_legacy = set()

    for row in rows:
        mk = (row.get('match_kind') or '').strip()
        line = row['_line']
        if mk not in ('cpuid', 'legacy'):
            errors.append(f"line {line}: match_kind must be 'cpuid' or 'legacy', got '{mk}'")
            continue

        if mk == 'cpuid':
            vendor = (row.get('vendor') or '').strip()
            if not vendor:
                errors.append(f"line {line}: cpuid row must have a vendor")
            try:
                family = int(row['family'])
                model  = int(row['model'])
                smin   = int(row['stepping_min'])
                smax   = int(row['stepping_max'])
            except (ValueError, KeyError) as exc:
                errors.append(f"line {line}: invalid numeric field: {exc}")
                continue
            if not (0 <= family <= 255):
                errors.append(f"line {line}: family out of range")
            if not (0 <= model <= 255):
                errors.append(f"line {line}: model out of range")
            if not (0 <= smin <= 15 and 0 <= smax <= 15):
                errors.append(f"line {line}: stepping out of 0-15 range")
            if smin > smax:
                errors.append(f"line {line}: stepping_min > stepping_max")
            key = (vendor, family, model, smin, smax)
            if key in seen_cpuid:
                errors.append(f"line {line}: duplicate cpuid tuple {key}")
            seen_cpuid.add(key)

        else:  # legacy
            lc = (row.get('legacy_class') or '').strip()
            if not lc:
                errors.append(f"line {line}: legacy row must have a legacy_class")
            if lc in seen_legacy:
                errors.append(f"line {line}: duplicate legacy_class '{lc}'")
            seen_legacy.add(lc)

        if not (row.get('friendly') or '').strip():
            errors.append(f"line {line}: friendly name is required")

    return errors


def emit_entry(row) -> str:
    mk = row['match_kind'].strip()
    friendly = c_escape((row.get('friendly') or '').strip())
    notes    = c_escape((row.get('notes')    or '').strip())

    if mk == 'cpuid':
        vendor = c_escape((row.get('vendor') or '').strip())
        family = int(row['family'])
        model  = int(row['model'])
        smin   = int(row['stepping_min'])
        smax   = int(row['stepping_max'])
        return (
            f'    {{ CPU_DB_MATCH_CPUID, {vendor}, {family}, {model}, '
            f'{smin}, {smax}, "", {friendly}, {notes} }},'
        )
    else:
        lc = c_escape((row.get('legacy_class') or '').strip())
        return (
            f'    {{ CPU_DB_MATCH_LEGACY, "", 0, 0, 0, 0, {lc}, '
            f'{friendly}, {notes} }},'
        )


def emit_c(rows, out_path: Path):
    lines = [
        '/*',
        ' * AUTO-GENERATED — DO NOT EDIT.',
        ' *',
        f' * Regenerate with: python hw_db/build_cpu_db.py',
        f' * Source: hw_db/cpus.csv ({len(rows)} entries)',
        ' */',
        '',
        '#include "cpu_db.h"',
        '#include <string.h>',
        '',
        'const cpu_db_entry_t cpu_db[] = {',
    ]
    for row in rows:
        lines.append(emit_entry(row))
    lines += [
        '};',
        '',
        f'const unsigned int cpu_db_count = {len(rows)};',
        '',
        'const cpu_db_entry_t *cpu_db_lookup_cpuid(const char *vendor,',
        '                                          unsigned char family,',
        '                                          unsigned char model,',
        '                                          unsigned char stepping)',
        '{',
        '    unsigned int i;',
        '    for (i = 0; i < cpu_db_count; i++) {',
        '        const cpu_db_entry_t *e = &cpu_db[i];',
        '        if (e->match_kind != CPU_DB_MATCH_CPUID)   continue;',
        '        if (strcmp(e->vendor, vendor) != 0)        continue;',
        '        if (e->family != family)                   continue;',
        '        if (e->model  != model)                    continue;',
        '        if (stepping < e->stepping_min)            continue;',
        '        if (stepping > e->stepping_max)            continue;',
        '        return e;',
        '    }',
        '    return (const cpu_db_entry_t *)0;',
        '}',
        '',
        'const cpu_db_entry_t *cpu_db_lookup_legacy(const char *legacy_class)',
        '{',
        '    unsigned int i;',
        '    if (!legacy_class) return (const cpu_db_entry_t *)0;',
        '    for (i = 0; i < cpu_db_count; i++) {',
        '        const cpu_db_entry_t *e = &cpu_db[i];',
        '        if (e->match_kind != CPU_DB_MATCH_LEGACY)  continue;',
        '        if (strcmp(e->legacy_class, legacy_class) == 0) return e;',
        '    }',
        '    return (const cpu_db_entry_t *)0;',
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
