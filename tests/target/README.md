# Target-side tests

DOS-side validation of `CERBERUS.EXE`. Run inside DOSBox-X (or on real iron) from the CERBERUS repo root.

## Phase 0 smoke test

`PHASE0.BAT` exercises the scaffold gate checklist interactively:

1. `CERBERUS /?` prints help, exits 0
2. Default run writes `CERBERUS.INI`, exits 0
3. `/U` hits the disabled-upload stub and exits 2 (Phase 5 decoupling contract)
4. Unknown option exits 1

Expected exit codes match the constants in `src/cerberus.h`:

| Exit code | Meaning |
|---|---|
| 0 | Normal success |
| 1 | Usage / parse error |
| 2 | Upload failed / disabled |
| 3 | Hardware hang (breadcrumb recovery path) |

## How to run

Inside DOSBox-X, from this repo root:

```
CD tests\target
PHASE0
```

Or via automation (Task 1.0+ will add `dosrun.py` bridge).
