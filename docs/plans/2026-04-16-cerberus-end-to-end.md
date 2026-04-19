# CERBERUS End-to-End Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use crucible:build to implement this plan task-by-task.
>
> **Scope note:** This plan covers v0.1 → v1.0. Phase 0 (scaffold) and Phase 1 (v0.2 DETECT) have bite-sized TDD task lists — actionable immediately. Phases 2–5 have architecture + module contracts + quality-gate criteria. Re-run the planning skill at the start of each of those phases to produce bite-sized breakdowns grounded in then-current code. Phase 6 (release) is bite-sized.
>
> **Reality check:** Spec §3 states v0.1 has timing / CPU detect / display / INI reporter / BIOS info implemented. The repo at the time of this plan contains only CERBERUS.md, README.md, LICENSE, .gitignore. No code exists. Phase 0 builds the skeleton that §3 describes.

**Goal:** Ship CERBERUS v1.0 — a 64KB DOS real-mode EXE that detects, diagnoses, and benchmarks IBM PC hardware from 8088/MDA through 486/VGA, produces INI reports with a consistency engine cross-checking all three analysis heads, and optionally uploads results via the NetISA TLS 1.3 card.

**Architecture:** Single-binary orchestrator with per-head subsystem modules sharing a core library (timing, display, report, consistency, thermal). Each hardware subsystem (cpu, fpu, mem, cache, bus, video, audio, bios) lives in a single module that exposes up to three verbs — `detect()`, `diagnose()`, `benchmark()` — called by per-head orchestrators. Results flow into a central result table keyed by subsystem, then the consistency engine cross-checks, then the renderer paints. Progressive-enhancement text-mode UI works on MDA and scales up through VGA without a separate code path.

**Tech stack:** Open Watcom C/C++ 2.0, NASM 2.x, `wmake`. DOS real-mode, **medium memory model** (decision in Pre-flight PF-2). Host-side unit tests via Watcom's Windows/Linux build for portable modules (report, consist, signature). Target-side integration tests via `dosrun.py` automation in DOSBox-X with 8088/286/386/486 presets. Real-hardware validation before each tagged release.

---

## Pre-flight: Decisions That Must Close Before Phase 0

These are cheap to decide and expensive to defer. Close them in one sitting.

### PF-1 — Name collision check (**do this NOW, before Phase 0 — 15 min priority**)

The GitHub repo is already live with the CERBERUS name (README.md committed). The "before any public announcement" framing was misleading — the repo is already discoverable via GitHub search. Close this BEFORE Phase 0 starts.

- Search Google, VOGONS, GitHub for existing DOS tools named "Cerberus"
- Search the broader tech-tool namespace (Cerberus FTP Server, Cerberus banking trojan, Cerberus security tools)
- **Decision:** proceed with CERBERUS, or rename. Candidates if renaming: HYDRA (three-headed, already used — skip), TRIPLEX, CANIS, ARGUS (hundred-eyed guardian — still retro-myth, uncrowded namespace)
- **If rename is chosen:** repo has <5 non-author stars/forks today. Force-push a rename commit and update the GitHub repo name now — cost is still low. Every subsequent day the rebrand cost compounds (barelybooting.com page, VOGONS post, YouTube video).

### PF-2 — Memory model (5 min decision, propagates)

Spec §9 says "large model." Target is <64KB EXE. Large model is overkill.

Options:
- **Small** — 64KB code + 64KB data, near pointers only, fastest code gen
- **Compact** — 64KB code + >64KB data with far data pointers
- **Medium** — >64KB code + 64KB data with far code pointers ← **recommended**
- **Large** — >64KB code + >64KB data with far pointers both ways

DETECT/DIAGNOSE/BENCHMARK assembly routines will overflow 64KB code. Data buffers for the core (PIT samples, signature hash, INI serialization, result table) comfortably fit in 64KB. Medium is the right call for the base build.

**Far-buffer convention (load-bearing):** Phase 3's memory-latency and cache benchmarks explicitly require buffers ≥ 4× L2 size — on a late 486 that is 256KB+, and cache-bandwidth stride tests already reach 64KB (the medium-model data ceiling). Wholesale switching to the large model is not the answer; it would regress code size and perf across the entire tool. Instead:
- Base build stays medium model
- Benchmark modules declare oversize buffers as `static char __far <name>[N]`
- Pointer arithmetic inside benchmark inner loops uses `__far` pointers explicitly
- A Phase 3 prerequisite task (to be added at Phase 3 re-plan) defines a shared far-buffer allocation helper in `core/` so `__far` usage is isolated from non-benchmark code
- DGROUP is checked via `wlink option map` after every task per `feedback_dos_conventions.md` rule #2

**Action:** update CERBERUS.md §9 to state medium model + far-buffer convention. Add `wmake` switch `-mm` (medium model) to Makefile. Track DGROUP via map file at every task boundary.

### PF-3 — "Consistency Engine" vs "Truth Engine" (2 min)

§6 uses both. Pick one.

**Recommendation:** **Consistency Engine** in code, docs, and INI. "Truth Engine" lives only in marketing copy (YouTube, itch.io blurb). Never in source.

**Action:** edit CERBERUS.md §6 to drop the parenthetical.

### PF-4 — Schema, signature, and run identity versioning (3 min)

The INI carries multiple independently-versioned identities. Conflating them breaks the NetISA dedup goal.

**Three version fields, all in `[cerberus]`:**
- `version=1.0` — tool version (bumps per release)
- `schema_version=1.0` — INI layout version (bumps when field names or section structure change across releases). Clients reading submitted INIs dispatch on this.
- `signature_schema=1` — **rules governing which keys and values contribute to the canonical signature** (see Task 0.4 step 5). Bumps ONLY when the signature's canonical subset or normalization rules change. Frozen at v1.0.

**Two signatures:**
- `signature=<hex8>` — **hardware identity** — SHA-1 of the canonical subset (seven detect keys at HIGH confidence). Same hardware across tool versions produces the same `signature`. This is the dedup key.
- `run_signature=<hex8>` — **record identity** — SHA-1 of the full INI contents. Different runs on the same hardware produce different `run_signature` values. This disambiguates two submissions from the same machine (counterfeit-catching scenario: same `signature`, divergent `run_signature` + divergent benchmark numbers → the interesting case for the Consistency Engine and the NetISA DB).

**What this gains:** a machine that identifies as 486DX but benchmarks at 286-class speed is discoverable in the DB as "signature X has N submissions but their bench numbers span a 10× range" — exactly the counterfeit/remarked-CPU story CERBERUS is built to surface. If the signature included benchmark values, those two submissions would have different signatures and never be compared.

**Action:** define §6 example INI with all three version fields plus both signatures. Document in `docs/methodology.md` which fields belong to each identity.

### PF-5 — Upload privacy scope (5 min)

§10 doesn't specify what CERBERUS will *never* collect.

**Action:** add to §10 an explicit non-collection list:
- SMBIOS serial numbers
- CPU microcode revision IDs beyond family/model/stepping
- Hard drive serials or labels
- Any user-entered strings or CMOS boot password residue
- Directory listings, filenames, or file contents
- MAC addresses (NetISA card abstracts this away — CERBERUS never sees it)

The INI is strictly hardware capability + measured behavior + a hash signature of a canonical subset.

### PF-6 — Re-plan schema-review protocol (2 min)

The "re-run planning at phase start" pattern (noted in the plan header) is pragmatic but needs a guard: if a later phase wants to change `result_t` semantics, signature canonical rules, or INI section layout, that change retroactively affects earlier phases and any community-submitted samples.

**Action:** at the start of each re-plan (Phases 2, 3, 4, 5), the first step is a "schema review" task: inspect `src/cerberus.h`, `core/report.c` signature rules, and the INI section format. If a change is required, either (a) keep old code compatible via addition-not-modification, or (b) bump `schema_version` AND `signature_schema`, update `docs/methodology.md`, and explicitly note which prior sample INIs become non-comparable. Never silently change serialization.

### PF-7 — CLI switch naming consistency (2 min)

Current spec has `/D` meaning "detection only" and `/B` meaning "skip diagnostics" — asymmetric "only" vs "skip" semantics. Fix before shipping any public help output.

**Action:** rename to consistent verbs. Final form: `/ONLY:DET`, `/ONLY:DIAG`, `/ONLY:BENCH` and `/SKIP:DET`, `/SKIP:DIAG`, `/SKIP:BENCH`. `/SKIP:<name>` namespace is shared with the crash-recovery breadcrumb — skipping a specific test name after a hang uses the same switch. Default calibrated N bumped from 5 to **7** to give Mann-Kendall thermal test enough signal. Document in §7 of CERBERUS.md. Parse implementation is in Phase 0 Task 0.1 step 3 (no TODO).

---

## Phase 0: Scaffold — Build What §3 Claims Exists

**Goal:** A buildable `CERBERUS.EXE` that:
1. Parses `/?` and prints help
2. Runs all three head orchestrators as stubs (printing "head N stub" messages)
3. Writes a valid INI skeleton to `CERBERUS.INI`
4. Exits cleanly with a status code

**Exit criteria:** `wmake` produces a DOS EXE that runs in DOSBox-X 8088 preset and on a 486 preset without crashing. INI file written matches a golden snapshot.

**Estimated duration:** 2–4 sittings.

### Task 0.1 — Create source tree

**Files:**
- Create: `Makefile`, `src/cerberus.h`, `src/main.c`
- Create: `src/core/{timing,display,report,consist,thermal}.{h,c}` (stubs)
- Create: `src/detect/detect.h`, `src/detect/detect_all.c`, plus per-subsystem stubs
- Create: `src/diag/{diag.h,diag_all.c}` (stubs)
- Create: `src/bench/{bench.h,bench_all.c}` (stubs)
- Create: `src/upload/{upload.h,upload.c}` (stubs)
- Create: `docs/methodology.md`, `docs/consistency-rules.md`, `docs/contributing.md` (stubs with one-line TODO each)

**Step 1: Create the directory skeleton**

```bash
mkdir -p src/core src/detect src/diag src/bench src/upload docs \
         hw_db/submissions tests/host tests/target \
         .github/ISSUE_TEMPLATE
```

The `hw_db/` directory holds the human-editable CSV databases (CPU, FPU, video, audio, BIOS) that build scripts in Phase 1 regenerate into C source. `hw_db/submissions/` documents the community-contribution workflow. `.github/ISSUE_TEMPLATE/` holds the hardware-submission issue template added in Task 1.11.

**Step 2: Write `src/cerberus.h`** — master types and constants

```c
#ifndef CERBERUS_H
#define CERBERUS_H

#define CERBERUS_VERSION         "0.1.0"
#define CERBERUS_SCHEMA_VERSION  "1.0"
#define CERBERUS_SIGNATURE_SCHEMA "1"

typedef enum { MODE_QUICK = 0, MODE_CALIBRATED = 1 } run_mode_t;

typedef enum {
    CONF_LOW = 0, CONF_MEDIUM = 1, CONF_HIGH = 2
} confidence_t;

typedef enum {
    VERDICT_UNKNOWN = 0, VERDICT_PASS = 1, VERDICT_WARN = 2, VERDICT_FAIL = 3
} verdict_t;

/* Typed result value — discriminated union avoids round-tripping numbers
   through strings for the consistency engine's numeric comparisons.
   Serialization to INI uses the pre-formatted `display` field. */
typedef enum {
    V_STR  = 0,   /* categorical/text; use v.s */
    V_U32  = 1,   /* unsigned 32-bit integer; use v.u */
    V_Q16  = 2    /* fixed-point Q16.16 (no FPU dependency); use v.fixed */
} value_type_t;

typedef struct {
    const char  *key;                  /* e.g., "cpu.detected" */
    value_type_t type;
    union {
        const char   *s;
        unsigned long u;
        long          fixed;           /* Q16.16 — upper 16 integer, lower 16 fraction */
    } v;
    const char  *display;              /* pre-formatted for INI output */
    confidence_t confidence;
    verdict_t    verdict;
} result_t;

#define MAX_RESULTS 256                /* calibrated mode can exceed 128 entries */

typedef struct {
    result_t     results[MAX_RESULTS];
    unsigned int count;                /* NOT unsigned char — MAX_RESULTS > 255 */
} result_table_t;

/* Q16.16 helpers */
#define Q16_FROM_INT(i)   ((long)(i) << 16)
#define Q16_TO_INT(q)     ((int)((q) >> 16))
#define Q16_PERCENT_OK(a, b, tol_pct) \
    ( ((a) - (b) >= 0 ? (a) - (b) : (b) - (a)) \
      <= (((b) / 100L) * (long)(tol_pct)) )

/* CLI options */
typedef struct {
    run_mode_t mode;
    unsigned char runs;          /* for calibrated mode — default 7 (Phase 4 thermal wants >=7) */
    unsigned char do_detect;     /* 1 = run head I */
    unsigned char do_diagnose;   /* 1 = run head II */
    unsigned char do_benchmark;  /* 1 = run head III */
    unsigned char do_upload;     /* 1 = /U */
    unsigned char no_cyrix;      /* 1 = /NOCYRIX — skip unsafe port 22h probe */
    unsigned char no_intro;      /* 1 = /NOINTRO — skip VGA splash */
    char out_path[64];
} opts_t;

#endif
```

**Step 3: Write `src/main.c`** — CLI, orchestration, exit

The `/U` parse is implemented IN PHASE 0 — not TODO — because the Phase 5 decoupling contract depends on `/U` producing a non-zero exit code when the upload stub reports `UPLOAD_DISABLED`. Exit-code semantics are stable across the NETISA build-flag toggle.

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cerberus.h"
#include "core/display.h"
#include "core/report.h"
#include "core/crumb.h"
#include "detect/detect.h"
#include "diag/diag.h"
#include "bench/bench.h"
#include "upload/upload.h"

/* Exit codes */
#define EXIT_OK           0
#define EXIT_USAGE        1
#define EXIT_UPLOAD_FAIL  2
#define EXIT_HW_HANG      3

static void print_help(void) {
    puts("CERBERUS " CERBERUS_VERSION " - Retro PC System Intelligence");
    puts("");
    puts("Usage: CERBERUS [/Q] [/C[:n]] [/ONLY:<h>] [/SKIP:<h>] [/O:file]");
    puts("                [/U] [/NOCYRIX] [/NOINTRO] [/?]");
    puts("  /Q              Quick mode (default)");
    puts("  /C[:n]          Calibrated mode, n runs (default 7)");
    puts("  /ONLY:DET|DIAG|BENCH   Run only that head");
    puts("  /SKIP:DET|DIAG|BENCH   Skip that head");
    puts("  /O:file         Output INI (default CERBERUS.INI)");
    puts("  /U              Upload via NetISA (non-zero exit if disabled)");
    puts("  /NOCYRIX        Skip Cyrix DIR probe (port 22h safety)");
    puts("  /NOINTRO        Skip VGA splash");
    puts("  /?              This help");
}

static int str_starts_with(const char *s, const char *p) {
    while (*p) if (*s++ != *p++) return 0;
    return 1;
}

static int parse_args(int argc, char *argv[], opts_t *o) {
    int i;
    /* defaults */
    o->mode = MODE_QUICK; o->runs = 1;
    o->do_detect = o->do_diagnose = o->do_benchmark = 1;
    o->do_upload = 0; o->no_cyrix = 0; o->no_intro = 0;
    strcpy(o->out_path, "CERBERUS.INI");

    for (i = 1; i < argc; i++) {
        char *a = argv[i];
        if (!strcmp(a, "/?"))            { print_help(); return 0; }
        else if (!strcmp(a, "/Q"))       { o->mode = MODE_QUICK; o->runs = 1; }
        else if (str_starts_with(a, "/C")) {
            o->mode = MODE_CALIBRATED;
            o->runs = (a[2] == ':' && a[3]) ? (unsigned char)atoi(a + 3) : 7;
            if (o->runs < 3) o->runs = 3;            /* thermal needs ≥ 7; enforce floor */
        }
        else if (str_starts_with(a, "/ONLY:")) {
            o->do_detect = o->do_diagnose = o->do_benchmark = 0;
            if      (!strcmp(a + 6, "DET"))   o->do_detect = 1;
            else if (!strcmp(a + 6, "DIAG"))  o->do_diagnose = 1;
            else if (!strcmp(a + 6, "BENCH")) o->do_benchmark = 1;
            else { fprintf(stderr, "bad /ONLY target: %s\n", a + 6); return -1; }
        }
        else if (str_starts_with(a, "/SKIP:")) {
            if      (!strcmp(a + 6, "DET"))   o->do_detect = 0;
            else if (!strcmp(a + 6, "DIAG"))  o->do_diagnose = 0;
            else if (!strcmp(a + 6, "BENCH")) o->do_benchmark = 0;
            /* other /SKIP:<name> values are consumed by the crumb skiplist */
        }
        else if (str_starts_with(a, "/O:")) {
            strncpy(o->out_path, a + 3, sizeof(o->out_path) - 1);
            o->out_path[sizeof(o->out_path) - 1] = 0;
        }
        else if (!strcmp(a, "/U"))       { o->do_upload = 1; }
        else if (!strcmp(a, "/NOCYRIX")) { o->no_cyrix = 1; }
        else if (!strcmp(a, "/NOINTRO")) { o->no_intro = 1; }
        else { fprintf(stderr, "unknown option: %s\n", a); return -1; }
    }
    return 1;
}

int main(int argc, char *argv[]) {
    opts_t opts;
    result_table_t table;
    int parse_rc, exit_code = EXIT_OK;
    int upload_rc;

    memset(&table, 0, sizeof(table));

    parse_rc = parse_args(argc, argv, &opts);
    if (parse_rc == 0)  return EXIT_OK;          /* /? path */
    if (parse_rc == -1) return EXIT_USAGE;       /* parse error */

    display_init();
    crumb_check_previous();                      /* prints notice if CERBERUS.LAST from prior hang */
    display_banner();

    if (opts.do_detect)    detect_all(&table);
    if (opts.do_diagnose)  diag_all(&table);
    if (opts.do_benchmark) bench_all(&table);

    /* consistency + thermal hooks wired in Phase 4 (v0.5) */

    report_write_ini(&table, &opts, opts.out_path);

    if (opts.do_upload) {
        upload_rc = upload_ini(opts.out_path);
        if (upload_rc != UPLOAD_OK) exit_code = EXIT_UPLOAD_FAIL;
    }

    display_shutdown();
    return exit_code;
}
```

**Phase 0 test for decoupling contract:** run `CERBERUS /U` on the default (non-NETISA) build, assert stdout contains "disabled" and exit code equals 2. This locks the Phase 5 contract before NetISA code exists.

**Step 4: Write header stubs** for every module listed in §11. Every `.h` declares the module's entry point; every `.c` implements a no-op that prints `"[<module>] stub"` and returns. Keep each under 20 lines. These exist so the build succeeds and the call graph is real.

**Step 5: Write Makefile**

This Makefile must actually build a tree with subdirectories and both C and NASM sources. Watcom `wmake`'s default `.c.obj` inference rule does NOT traverse subdirectories — explicit per-subdirectory rules are required. NASM needs its own rule. Link must set stack size (default 2KB is insufficient — see `feedback_dos_conventions.md` rule #3) and generate a map file for DGROUP tracking.

Watcom wmake's path-qualified inference-rule syntax is finicky and varies between wmake versions. **Use explicit per-file rules** — more lines, but guaranteed to build on any Watcom wmake. Do not use `{path}.ext{.ext}:` or `path/.ext{.ext}:` forms until tested against the target wmake version.

```makefile
# CERBERUS - Watcom wmake (explicit per-file rules; no inference-rule gymnastics)
CC       = wcc
AS       = nasm
LD       = wlink
CFLAGS   = -mm -bt=dos -os -zq -i=src -w3
# -mm    medium memory model (>64K code, <=64K data near)
# -bt    build target DOS
# -os    optimize for size
# -zq    quiet
# -w3    warning level 3 (NOT -wx; gate uses -w3 consistently)
# Note: do NOT pass -s (remove stack checks) without a verified stack budget.
ASFLAGS  = -f obj
TARGET   = CERBERUS.EXE
MAPFILE  = cerberus.map
STACK    = 4096
HDR      = src/cerberus.h

OBJS = src/main.obj                                                  &
       src/core/timing.obj   src/core/timing_a.obj                   &
       src/core/display.obj  src/core/display_a.obj                  &
       src/core/report.obj   src/core/consist.obj                    &
       src/core/thermal.obj  src/core/crumb.obj                      &
       src/detect/detect_all.obj                                     &
       src/detect/cpu.obj    src/detect/cpu_a.obj                    &
       src/detect/fpu.obj    src/detect/mem.obj                      &
       src/detect/cache.obj  src/detect/bus.obj                      &
       src/detect/video.obj  src/detect/audio.obj                    &
       src/detect/bios.obj                                           &
       src/diag/diag_all.obj src/bench/bench_all.obj                 &
       src/upload/upload.obj

all : $(TARGET)

# Link: explicit stack, map file, DOS system
$(TARGET) : $(OBJS)
    %write  cerberus.lnk system dos
    %append cerberus.lnk name $(TARGET)
    %append cerberus.lnk option map=$(MAPFILE)
    %append cerberus.lnk option stack=$(STACK)
    %append cerberus.lnk option quiet
    %append cerberus.lnk file { $(OBJS) }
    $(LD) @cerberus.lnk

# Explicit C rules (one per source file — verbose but portable)
src/main.obj                  : src/main.c                        $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/core/timing.obj           : src/core/timing.c                 $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/core/display.obj          : src/core/display.c                $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/core/report.obj           : src/core/report.c                 $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/core/consist.obj          : src/core/consist.c                $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/core/thermal.obj          : src/core/thermal.c                $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/core/crumb.obj            : src/core/crumb.c                  $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/detect_all.obj     : src/detect/detect_all.c           $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/cpu.obj            : src/detect/cpu.c                  $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/fpu.obj            : src/detect/fpu.c                  $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/mem.obj            : src/detect/mem.c                  $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/cache.obj          : src/detect/cache.c                $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/bus.obj            : src/detect/bus.c                  $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/video.obj          : src/detect/video.c                $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/audio.obj          : src/detect/audio.c                $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/detect/bios.obj           : src/detect/bios.c                 $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/diag/diag_all.obj         : src/diag/diag_all.c               $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/bench/bench_all.obj       : src/bench/bench_all.c             $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@
src/upload/upload.obj         : src/upload/upload.c               $(HDR)
    $(CC) $(CFLAGS) -fo=$^@ $[@

# Explicit NASM rules
src/core/timing_a.obj         : src/core/timing_a.asm
    $(AS) $(ASFLAGS) -o $^@ $[@
src/core/display_a.obj        : src/core/display_a.asm
    $(AS) $(ASFLAGS) -o $^@ $[@
src/detect/cpu_a.obj          : src/detect/cpu_a.asm
    $(AS) $(ASFLAGS) -o $^@ $[@

clean : .SYMBOLIC
    @for %d in (core detect diag bench upload) do @del /Q src\%d\*.obj 2>NUL
    @del /Q src\*.obj 2>NUL
    @del /Q $(TARGET) $(MAPFILE) cerberus.lnk 2>NUL
```

**Build verification before Phase 0 completion:** run `wmake` on the tree with all stubs present and verify:
1. Every `.obj` file listed in `OBJS` is created
2. `cerberus.lnk` is generated with `system dos`, `option map=`, `option stack=4096`
3. `CERBERUS.EXE` exists and runs `/?` without crash
4. `cerberus.map` is committed alongside the binary; DGROUP size recorded in the commit message

If any step fails, the Makefile is wrong — fix it in Phase 0, do not proceed to Task 0.2.

**Step 6: Build**

```bash
wmake
```

Expected: `CERBERUS.EXE` created, size ~5–15KB.

**Step 7: Smoke test in DOSBox-X**

Use `dosrun.py` (per memory `reference_devenv_relay.md`):
```
python devenv/dosrun.py --preset 486 -- CERBERUS /?
python devenv/dosrun.py --preset 8088 -- CERBERUS /?
```
Expected: help text prints, no crash, exit code 0.

**Step 8: Commit**

```bash
git add src Makefile docs/methodology.md docs/consistency-rules.md docs/contributing.md
git commit -m "Scaffold: directory tree, stubs, Makefile, working EXE"
```

### Task 0.2 — Implement core/timing.c properly

§6 describes PIT Channel 2 gate-based measurement. This is not stubbable — it's load-bearing for every later benchmark. Implement it now and host-test the math.

**Files:** `src/core/timing.h`, `src/core/timing.c`, `src/core/timing_a.asm`, `tests/host/test_timing.c`

**Steps:**
1. Write `timing.h` — API declared with explicit integer widths:
   ```c
   typedef unsigned long us_t;        /* microseconds; 32-bit to hold >65ms */
   typedef unsigned int  ticks_t;     /* raw 16-bit PIT counter reading */
   void    timing_init(void);
   void    timing_start(void);
   us_t    timing_stop(void);         /* returns microseconds directly */
   us_t    timing_ticks_to_us(unsigned long ticks);
   void    timing_wait_us(us_t microseconds);   /* busy-wait; used by probes */
   int     timing_emulator_hint(void);          /* 1 if timing looks virtualized */
   ```
   Note the **unsigned long** parameter and return. This is load-bearing per `feedback_dos_16bit_int.md`. `timing_wait_us` is needed by Task 1.7 (OPL2 probe 80µs delay) and Task 1.1 (CPU detection timing-sensitive probes); implementing it in Phase 0 avoids a cross-phase API retrofit.
2. Write host unit test for `timing_ticks_to_us` — pure math, no hardware. PIT runs at 1193182 Hz, so 1 tick ≈ 838.095 ns. Use 32-bit arithmetic throughout:
   ```c
   us_t timing_ticks_to_us(unsigned long ticks) {
       /* Approx of (ticks * 1000000UL) / 1193182UL, 32-bit safe.
          Systematic ~0.01% undercount is acceptable for ±10% bench match goal. */
       return (us_t)((ticks * 838UL + 500UL) / 1000UL);
   }
   ```
   Do NOT write `ticks * 838` as plain int — that overflows 16-bit at ticks=78. Test corner cases: 0 (→0 µs), 1 (→1 µs), 100 (→84 µs), 1000 (→838 µs), 65535 (→54,919 µs), 1000000 (→838,000 µs). Every intermediate MUST be `unsigned long`.
3. Run host test — expected FAIL (function not yet implemented)
4. Implement `timing_ticks_to_us` in C with `unsigned long` intermediates as shown above
5. Run host test — expected PASS for all six corner cases
6. Implement `timing_start/stop` in NASM:
   - `timing_start`: gate Channel 2 OFF (port 61h bit 0 = 0, keeping speaker data bit 1 clear), program counter Mode 2 (`10110100b` to port 43h), load 0xFFFF into counter (port 42h low, then high), gate ON (port 61h bit 0 = 1).
   - `timing_stop`: latch Channel 2 (port 43h = `10000000b`), read low byte then high byte from port 42h, gate OFF.
   - **Rollover math (CRITICAL — PIT Channel 2 counts DOWN in Mode 2):** Let `start_count = 0xFFFF` (loaded value), `stop_count` = latched read. Normal case: `stop_count < start_count` (counter decreased). **Wraparound case: `stop_count >= start_count`, meaning the counter passed 0 and reloaded mid-measurement.** Compute delta as:
     ```c
     unsigned long delta;
     if (stop_count <= start_count)
         delta = (unsigned long)(start_count - stop_count);          /* normal: decreased */
     else
         delta = 65536UL + (unsigned long)start_count - (unsigned long)stop_count;  /* wrapped */
     ```
     The wrap case triggers when the measured interval exceeds ~55ms (65535 ticks × 838ns). Unit-test BOTH branches in the host harness by passing `(start=0xFFFF, stop=0x8000)` → expect 0x7FFF, and `(start=0x1000, stop=0xE000)` → expect 0x3000 (wrap). If you write this backwards, every measurement longer than 55ms is garbage and the bug is invisible in DOSBox (fast enough to never wrap in typical tests) — surfaces only on slow real hardware.
7. Implement `timing_wait_us(us)`: compute target ticks from us, read current PIT Channel 0 counter at 0x40 (BIOS system tick channel, already running; DO NOT reprogram it), busy-wait until enough ticks have elapsed. Reading Channel 0 requires latching (port 43h = `00000000b` for Channel 0 latch), then reading port 40h low + high. This does NOT touch Channel 2 — safe to call while Channel 2 is gated for measurement.
8. **XT-clone PIT Channel 2 sanity probe** at `timing_init()`: on some XT clones Channel 2's gate is wired oddly to the speaker. Verify that (a) writing gate-disable → re-enable produces a running counter, (b) two reads within a known delay produce a delta within 10× of expectation. If either check fails, set a flag so all measurement-derived results downstream carry confidence LOW and the `timing_emulator_hint()` returns 1. Document in methodology.md.
9. Write DOS-side smoke test: time a fixed 10,000-iteration NOP loop. Assert µs result is within expected range for the DOSBox-X-reported CPU class.
10. Run in DOSBox-X 486 preset — verify result is plausible
11. Run in DOSBox-X 8088 preset — verify result is plausible (slower)
12. Commit

### Task 0.3 — Implement core/display.c adapter detection

§6 text-mode abstraction. Detect MDA/CGA/Hercules/EGA/VGA at startup; expose `display_putc`, `display_box`, `display_goto`, `display_attr`. CGA gets retrace-synced writes.

**Files:** `src/core/display.h`, `src/core/display.c`, `src/core/display_a.asm`

**Steps:**
1. Write `display.h` — adapter enum, putc/box/attr API
2. Implement adapter detection via INT 10h AH=12h (EGA/VGA) and BIOS data area 0040:0049h (current mode) and 0040:0010h (equipment flag bits 4–5 for initial adapter). Fallback: probe for MDA segment B000h, CGA segment B800h.
3. Implement `display_putc` using BIOS INT 10h AH=0Eh for the generic path (works everywhere)
4. Implement CGA fast path: direct VRAM write with retrace wait (poll port 3DAh bit 0 until 0, then until 1)
5. Implement box-drawing using CP437 line chars (═║╔╗╚╝╠╣╦╩╬)
6. DOSBox-X smoke test: draw a box in each preset, verify no corruption
7. Commit

### Task 0.4 — Implement core/report.c INI writer + signature

**Files:** `src/core/report.h`, `src/core/report.c`, `tests/host/test_report.c`

**Steps:**
1. Write `report.h` — API: `report_add(table, key, type, v, display, conf, verdict)`, `report_write_ini(table, opts, path)`, `report_hardware_signature(table) → hex8` (canonical-subset SHA-1), `report_run_signature_from_file(path) → hex8` (full-INI SHA-1 computed after initial write, then appended — see step 5b below)
2. Host unit test: given a fixed result_table_t, INI output byte-matches a golden string
3. Run test — FAIL
4. Implement: iterate results, group by section (derive from key prefix e.g. "cpu.detected" → `[cpu]`, `detected=`), emit sorted by section then key
5. Implement TWO signatures with separate roles per PF-4:

   **`signature` (hardware identity — dedup key):** SHA-1 over a **frozen canonical subset** so the value is stable across tool versions on the same hardware.
   - Keys included, in this fixed order: `cpu.detected`, `cpu.class`, `fpu.detected`, `memory.conventional_kb`, `memory.extended_kb`, `video.adapter`, `bus.class`
   - Only values with `confidence == CONF_HIGH` contribute. MEDIUM and LOW serialize as `key=unknown` so later detection improvements don't retroactively change historical signatures.
   - Normalization (frozen at v1.0, documented in `docs/methodology.md`): lowercase, no spaces, fixed vocabulary — video: `vga|ega|cga|mda|hercules|unknown`; bus: `isa8|isa16|vlb|pci|unknown`; FPU: `none|287|387|integrated|unknown`; CPU class: `8086|8088|v20|v30|286|386sx|386dx|486sx|486dx|486dxnosepl|pentium|cyrix|amd5x86|unknown`
   - Serialize as `key=value\n` in the listed order; SHA-1; first 8 hex chars
   - Emit `signature_schema=1` in `[cerberus]`. Bumps only when canonical rules change, never on tool feature additions.

   **`run_signature` (record identity):** SHA-1 over the **full INI file contents** (every `key=value` line from every section, excluding `run_signature=` itself). First 8 hex chars. Same hardware, different run → different `run_signature`. Two submissions from the same counterfeit-CPU machine hit the same `signature` with different `run_signature` values — the Consistency Engine surfaces the inter-run divergence as its signature feature.

   **Serialization order for `run_signature`:** INI is written in a fixed section order (cerberus, environment, cpu, fpu, memory, cache, bus, video, audio, bios, diagnose, bench, consistency) with keys sorted within each section. A placeholder `run_signature=0000000000000000` line is written first, then after all other output is complete, the file is re-read, the placeholder's line replaced with the actual SHA-1 of all non-run_signature lines. This two-pass write keeps the signature self-consistent on re-read.

   Use a local SHA-1 implementation (~200 lines of C) — no library dependency. **Caveat:** SHA-1 processes big-endian 32-bit words internally; on 16-bit little-endian 8086 the implementation needs explicit byte-swap helpers. Unit-test against RFC 3174 test vectors.

6. Host test: (a) same inputs → same signature every time; (b) changing a LOW-confidence field does NOT change `signature`; (c) changing a HIGH-confidence field DOES change `signature`; (d) two different benchmark outputs on the same hardware produce same `signature` but different `run_signature`; (e) RFC 3174 test vectors for "abc" and the 1000000-byte test produce the correct SHA-1.
7. Run test — PASS
8. Commit

### Task 0.5 — Crash-recovery breadcrumb

**Why:** Flaky vintage hardware will hang mid-test. A breadcrumb file written before each test starts lets users post-mortem a lockup and skip the offender on retry.

**Files:** `src/core/crumb.h`, `src/core/crumb.c`

**Steps:**
1. Write `crumb.h` — API: `crumb_enter(test_name)`, `crumb_exit()`, `crumb_check_previous()`, `crumb_skiplist_has(name)`.
2. Implement with explicit read-only-media handling. The 8088/floppy floor-target case is exactly when breadcrumbs matter most — do not assume writable CWD.

    ```c
    static char crumb_path[80];
    static int  crumb_disabled = 0;

    static int try_crumb_path(const char *dir) {
        int fd;
        if (dir && *dir) {
            sprintf(crumb_path, "%s\\CERBERUS.LAST", dir);
        } else {
            strcpy(crumb_path, "CERBERUS.LAST");
        }
        /* O_CREAT|O_WRONLY|O_TRUNC via Watcom's open() from <io.h>+<fcntl.h> */
        fd = open(crumb_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) return 0;
        close(fd);
        unlink(crumb_path);  /* clean up probe */
        return 1;
    }

    void crumb_init(void) {
        const char *tmp;
        /* 1st choice: CWD */
        if (try_crumb_path(NULL)) return;
        /* 2nd choice: %TEMP% if set */
        tmp = getenv("TEMP");
        if (tmp && try_crumb_path(tmp)) return;
        /* 3rd choice: %TMP% */
        tmp = getenv("TMP");
        if (tmp && try_crumb_path(tmp)) return;
        /* give up */
        crumb_disabled = 1;
        fputs("crash-recovery disabled (read-only media, no TEMP)\n", stderr);
    }

    void crumb_enter(const char *test_name) {
        int fd;
        if (crumb_disabled) return;
        fd = open(crumb_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) { crumb_disabled = 1; return; }  /* medium went read-only mid-run */
        write(fd, test_name, strlen(test_name));
        write(fd, "\n", 1);
        /* Force directory entry + FAT to disk: DOS file-system buffers are not
           flushed until close() or commit. Use INT 21h AH=68h (DOS 3.3+) for
           explicit commit; fall back to close+reopen on DOS 2.x. */
        _dos_commit(fd);          /* Watcom: INT 21h AH=68h on 3.3+, no-op otherwise */
        close(fd);
    }

    void crumb_exit(void) {
        if (crumb_disabled) return;
        unlink(crumb_path);
    }

    void crumb_check_previous(void) {
        FILE *f;
        char buf[64];
        if (crumb_disabled) return;
        f = fopen(crumb_path, "rt");
        if (!f) return;
        if (fgets(buf, sizeof(buf), f)) {
            printf("Previous run hung during: %s\n", buf);
            printf("Pass /SKIP:%s to bypass that test.\n", buf);
        }
        fclose(f);
        unlink(crumb_path);
    }
    ```
3. `crumb_init()` is called once from `main.c` before any detect/diag/bench call. `crumb_check_previous()` inspects and clears the file; `crumb_enter/exit` wrap each test invocation.
4. `/SKIP:<name>` CLI flag populates a skiplist the orchestrators consult via `crumb_skiplist_has()`. Uses the same namespace as the crash-recovery message.
5. DOSBox-X test: simulate hang by INT 19h during a stub test, verify breadcrumb was written. Additional test: mount a read-only directory, verify `crumb_init` falls through to TEMP, and if TEMP is also unset, CERBERUS still runs (with breadcrumb disabled and a one-line notice).
6. Commit

### Task 0.6 — Quality gate 0: scaffold review

Run the user's quality-gate pattern (adversarial review, per TAKEOVER precedent). Target: 1 round only at this stage — scaffold shouldn't have enough surface for 5 rounds of bugs.

**Gate checklist:**
- [x] EXE builds with `wmake` from clean checkout
- [x] Every listed `.obj` appears under `src/` after build
- [x] `cerberus.lnk` generated with `system dos`, `option map=`, `option stack=4096`
- [x] EXE runs in DOSBox-X 8088 preset without crash
- [x] EXE runs in DOSBox-X 486 preset without crash
- [x] `/?` prints help and exits 0
- [x] Default invocation writes `CERBERUS.INI` with valid section headers
- [x] `CERBERUS /U` (with stub upload module) prints "disabled" message and exits with code 2 (Phase 5 decoupling contract pre-lock)
- [x] `CERBERUS` from a read-only media simulation (mount RO in DOSBox-X) still runs, writes INI, prints "crash-recovery disabled" once
- [x] Host unit tests pass: `timing_ticks_to_us` corner cases, rollover math both branches, SHA-1 RFC 3174 test vectors, INI golden-file (4 pre-existing `test_timing` failures post-PIT-rework tracked as [#1](https://github.com/tonyuatkins-afk/CERBERUS/issues/1); all other suites clean)
- [x] Binary size under 64KB (expect ~15KB at this stage) — gate was for Phase 0 scaffold state (15KB) and passed at the time; current tip-of-tree is 80KB, see v1.0 size-target risk in the Risk Register
- [x] Build completes with CFLAGS `-w3`, zero warnings
- [x] `cerberus.map` committed alongside binary; DGROUP size recorded in commit message

**Commit tag:** `v0.1.1-scaffold` (not a GitHub release; internal checkpoint tag).

---

## Phase 1: v0.2 — DETECT Complete

**Goal:** All hardware detection subsystems in the §5 Head I table report accurate results on period-correct hardware.

**Entry criteria:** Phase 0 gate passed. Timing and display work.

**Exit criteria:** Running CERBERUS on a known-good 486DX/VGA system produces correct `[cpu] [fpu] [memory] [cache] [bus] [video] [audio] [bios]` sections. Same on an 8088/MDA system (with appropriate subsystems reporting "none" rather than false positives). 5-round quality gate passed.

**Estimated duration:** 6–10 sittings. This is the largest phase.

### Module contracts (apply to every detect module)

Every `detect/<subsys>.c` exposes:
```c
void detect_<subsys>(result_table_t *t);
```

Which calls `report_add(t, "<subsys>.<key>", value, confidence, VERDICT_UNKNOWN)` zero or more times. Detection never sets a verdict — that's DIAGNOSE's job in Phase 2.

### Task 1.0 — Emulator / environment detection (detect/env.c)

**Why this runs first:** Phase 1's gate criterion (Task 1.10) mandates that results from DOSBox-X and from real hardware must match at HIGH confidence or the gate fails. That only works if CERBERUS can reliably tell it is in an emulator and downgrade timing/cache/bench confidence accordingly. Without emulator detection, the consistency engine in Phase 4 will tune its rule thresholds to emulator artifacts (synthetic cache timing per Task 1.4) and false-positive on real hardware.

**Files:** `src/detect/env.c`

**Methodology — layered fingerprinting:**

1. **DOSBox / DOSBox-X:** Read the `BLASTER` env (DOSBox sets `A220 I7 D1 H5 T6` by default in many confs). Then inspect INT vectors at `00F1h–00FFh` — DOSBox sets INT F0h and others to callback handlers with a recognizable segment (`F000h` in classic DOSBox). DOSBox-X can be configured to hide these; also check for the string `"DOSBox"` in the copyright area of the BIOS data scan (0xF000 segment) and the `CDOSBOX` device name via INT 2Fh AX=4A40h (DOSBox-X integration signal).
2. **86Box / PCem:** No standard self-identification. Detect by INT 13h disk geometry inconsistencies or timing signatures. For v0.2, report as "emulator: unknown" if DOSBox signatures absent and CPU timing looks virtualized per `timing_emulator_hint()`.
3. **QEMU:** CPUID leaf `0x40000000` returns `"TCGTCGTCGTCG"` (TCG JIT) or `"KVMKVMKVMKVM"` (KVM). Read only if CPUID available (guard per Task 1.1 step 5).
4. **NTVDM / OS/2 DOS box:** INT 2Fh AX=1600h returns AL=1/2/3/4 under Windows 3.x+/NT; AX=4010h returns OS/2 version.
5. **Real hardware:** all of the above absent AND `timing_emulator_hint()` returns 0.

**Steps:**
1. Write `env.h`/`env.c` exposing `void detect_env(result_table_t *t);` and `int env_is_emulated(void);`
2. Implement the probe ladder above, populating `[environment]` INI section with: `emulator=<id>`, `emulator_version=<x>` (best-effort), `virtualized=<yes|no>`, `confidence_penalty=<none|medium|low>`.
3. The `confidence_penalty` field is consumed by every downstream `report_add` call: if emulation is detected, subsequent calls cap their reported confidence at the penalty level (HIGH → MEDIUM, MEDIUM stays MEDIUM). This is the mechanism by which emulator-produced INIs become comparable across the consistency engine.
4. Host test: verify the confidence-capping logic given a synthetic emulator flag
5. Target test: run on DOSBox-X default config → expect `emulator=dosbox-x`; run on DOSBox-X "hide emulator" config → expect `emulator=unknown, virtualized=yes` via timing hint; run on real hardware (8088 or 486) → expect `emulator=none`.
6. Commit

**Called first** in `detect_all()` so every subsequent detect module sees the emulator flag and acts accordingly. Update CERBERUS.md §5 to add Environment as a subsystem row.

### Task 1.1 — CPU detection (detect/cpu.c + cpu_a.asm)

**Methodology:** Layered probing from oldest to newest. Reference: Intel 486 Programmer's Reference Manual §10.2 (CPU identification), plus the Chris Giese CPU-detection code (widely cited in the DOS scene). Each probe must only execute on CPUs where its instructions are valid — run older probes first and short-circuit when the class is determined.

**Correct canonical probe sequence (cross-checked against Intel EFLAGS specification):**

1. **8086/8088 vs 286+:** `PUSHF` + `POP AX`, try to clear EFLAGS bits 12–15 via `AND AX, 0FFFh` / `PUSH AX` / `POPF` / `PUSHF` / `POP AX`. On 8086/8088 bits 12–15 are hardwired 1 and will return set; on 286+ they can be cleared.
2. **V20/V30 vs 8088/8086:** On 8086/8088 class, probe for NEC-specific instructions. Use the `SET1` / `CLR1` bit instructions (NEC-only) — encode via NASM `db` bytes, wrap in an INT 6 (invalid opcode) handler. If no fault, it's V20/V30.
3. **286 vs 386+:** On 286+ class, probe for 386 via operand-size-prefix acceptance. The `0x66` operand-size prefix was introduced on the 80386; **on a 286, any instruction prefixed with `0x66` (including `PUSHFD` encoded as `66 9C`) raises INT 6 (invalid opcode exception).** The 286 does NOT decode 0x66 as a benign prefix — it fails hard. Probe protocol:
   - Install a temporary INT 6 handler that sets a "faulted" flag in its data segment and IRETs past the `66 9C` sequence (requires advancing the saved CS:IP by 2 bytes; handlers of this kind are standard — see Chris Giese CPU-detect code)
   - Execute `db 66h, 9Ch` (PUSHFD). On 386+, this pushes a 32-bit EFLAGS image onto the stack; on 286, control transfers to the INT 6 handler and the "faulted" flag is set
   - Restore the original INT 6 handler
   - If faulted flag is set → CPU is 286 (or earlier if step 1 didn't filter). If not set → CPU is 386+, and the 32-bit EFLAGS are available for further classification.
   **Do NOT use AC flag here — AC is 486-only (see step 4).** Step 4 is ONLY reachable when step 3 confirms 386+.
4. **386 vs 486:** On 386+, attempt to toggle AC flag (EFLAGS bit 18) via `PUSHFD` / `OR dword / POPFD` / `PUSHFD`. 386 forces AC to 0; 486 retains the toggle. This correctly distinguishes 386 from 486. Note: **early i486DX parts lack CPUID** but still support AC toggle — so AC is the reliable 386/486 boundary.
5. **CPUID availability (late-486 and Pentium+):** Independently of step 4, attempt to toggle ID flag (EFLAGS bit 21). This succeeds on CPUID-capable parts (Pentium, late-486 like Intel SL-enhanced i486DX4, most Cyrix M1/M2, AMD K5+). Early i486DX-25/33 parts will fail this probe but succeed step 4 — correctly classified as "486 without CPUID."
6. **If CPUID available:** execute CPUID leaf 0 (vendor string into EBX/EDX/ECX), then leaf 1 (family/model/stepping/feature flags). Compare vendor against known strings: `GenuineIntel`, `AuthenticAMD`, `CyrixInstead`, `CentaurHauls`, `TransmetaCPU`, `NexGenDriven`, etc.
7. **Special-case probes (run only where class is plausible):**
   - **NEC V20/V30:** already handled in step 2
   - **RapidCAD:** 486-class timing signature with 387-class FPU behavior — probe by executing an FPU affine/projective infinity test and a 486-only integer operation; RapidCAD presents as 486 CPU + 387 FPU
   - **Cyrix DIR0/DIR1:** see step 8 below — gated probe only
   - **AMD 5x86:** identified via CPUID if present; if CPUID unavailable, fall back to CPU-class 486 + AMD-specific timing signatures
8. **Cyrix DIR probe (GATED — do not run unconditionally):** Port 22h is NOT inert on every motherboard. On some chipsets (TI PCP6, certain VIA/SiS boards) port 22h/23h is the chipset-configuration index/data pair and a spurious write can latch an index into cache-control logic. Only run the Cyrix probe if **all** of the following are true:
   - CPU class from step 4 is 386 or 486 era (not 8086/286/Pentium+)
   - CPUID is either unavailable OR returns vendor `CyrixInstead`
   - The user has not passed `/NOCYRIX` on the command line
   The probe itself: write `FEh` to port 22h (Cyrix DIR0 index — reads as device ID on Cyrix, harmless on most other chipsets), then read port 23h. On Cyrix, this returns a device-specific ID byte (e.g., `30h` for Cx486SLC, `3xh`/`4xh` for various 5x86/M1). On non-Cyrix, the byte is typically `FFh` or motherboard-specific and will not match any known Cyrix ID. Document the port-22h side-effect risk in `docs/methodology.md`.

**Steps:**
1. Write host test fixtures: hand-crafted EFLAGS-manipulation traces with known outcomes for each CPU class (8086/286/386/486-no-CPUID/486-CPUID/Pentium). Host test verifies the C classifier logic, not the ASM itself.
2. Write DOS target test harness: run on known-CPU DOSBox-X presets and match expected output
3. Implement ASM probes in `cpu_a.asm` (NASM) with NASM `db` bytes for instructions not supported by the assembler for the lowest target class. Wrap all new-instruction probes in an INT 6 (invalid-opcode) fault handler so a wrong-class probe fails safely instead of trapping.
4. Implement C wrapper in `cpu.c` that drives the probe sequence and publishes results with appropriate confidence: CPU class from steps 1/3/4/5 is HIGH confidence; CPUID vendor/family/model is HIGH when CPUID succeeds; special-case (RapidCAD, Cyrix-without-CPUID) is MEDIUM.
5. Add `/NOCYRIX` CLI flag for users with chipsets sensitive to port 22h
6. Test on DOSBox-X 8088, V20, 286, 386, 486 (with and without CPUID emulated), Pentium presets — expect each to report correctly with correct confidence
7. Test on real hardware — at minimum one 486 and one 386. Flag any real-hardware misclassification as a gate blocker.
8. Commit

### Task 1.1a — CPU identification database (detect/cpu_db.c)

**Goal:** turn raw probe results (CPU class + vendor string + family/model/stepping) into a rich friendly name, marketing name, known errata list, and `class_ipc` range for Phase 4 rule 4b. This is the primary lever that makes CERBERUS identification "feature-rich" — the gap between "486DX-class" and "Intel i486DX2-66 SX807/SX808, sSpec SX807, B1 stepping, FDIV-bug-free, class_ipc 0.4-0.5 MIPS/MHz."

**Files:** `src/detect/cpu_db.h`, `src/detect/cpu_db.c`, `hw_db/cpus.csv` (source of truth, human-editable)

**Schema — each entry:**
```c
typedef struct {
    /* CPUID-based match (all zero = pre-CPUID entry, match on legacy probe instead) */
    const char    *vendor;        /* CPUID vendor string, e.g. "GenuineIntel", NULL for pre-CPUID */
    unsigned char  family_min, family_max;
    unsigned char  model_min,  model_max;
    unsigned char  stepping_min, stepping_max;    /* 0xFF for "any" */

    /* Legacy match (when CPUID unavailable) */
    const char    *legacy_class;  /* "8086", "8088", "v20", "286", "386dx", "486dx-nocpuid", etc. */
    const char    *legacy_vendor_probe;  /* e.g. "cyrix-dir0=30h" result string, NULL for any */

    /* Output */
    const char    *friendly;      /* "Intel i486DX2-66" */
    const char    *marketing;     /* "IntelDX2 Processor 66 MHz" */
    const char    *sspec;         /* "SX807/SX808" (NULL if not applicable) */
    long           ipc_low_q16;   /* Q16.16 — min expected MIPS/MHz for class_ipc rule */
    long           ipc_high_q16;  /* Q16.16 — max */
    const char    *errata_flags;  /* comma-separated short codes: "fdiv,f00f,no-cmov" */
    const char    *notes;         /* short free-form, one sentence */
} cpu_db_entry_t;

extern const cpu_db_entry_t cpu_db[];
extern const unsigned int   cpu_db_count;

/* Lookup: returns NULL if no match */
const cpu_db_entry_t *cpu_db_lookup(
    const char *cpuid_vendor,
    unsigned char family, unsigned char model, unsigned char stepping,
    const char *legacy_class);
```

**Source data — build from factual references (facts aren't copyrightable):**
- Intel datasheets, sSpec databases (published at ark.intel.com and historical archives)
- AMD CPU documentation
- Cyrix device databooks (available via archive.org / bitsavers.org)
- InstLatx64 (https://www.instlatx64.atw.hu/) for real-hardware CPUID dumps
- sandpile.org for cross-referenced CPUID tables
- Wikipedia List-of processor pages for missing entries (cite source in `hw_db/cpus.csv` header)
- FreeBSD `sys/i386/i386/identcpu.c` — BSD-2-clause, can be adapted into MIT with attribution

**Target coverage for v0.2:** 80–120 entries covering every pre-Pentium and early-Pentium CPU plus the major Cyrix/AMD/IBM/ULSI/TI/NexGen oddities. Explicit list in `hw_db/cpus.csv` header:
- Intel: 8086, 8088, 80186, 80286 (all variants), 386SX/DX/CX/EX/CXSA, 486SX/DX/DX2/DX4, Pentium P5/P54C up through P55C
- AMD: Am286, Am386SX/DX, Am486SX/DX/DX2/DX4, Am5x86, K5, K6 (brief)
- Cyrix: Cx486SLC/DLC, Cx486S/DX/DX2, Cx5x86, 6x86 (M1), MediaGX, 6x86MX (M2)
- IBM: 486SLC2, BL3 variants
- NEC: V20, V30, V33, V53
- ULSI: US486DX
- TI: 486SXL, 486DLC
- NexGen: Nx586, Nx686
- RapidCAD: the 486-class-with-387-FPU pair
- UMC: U5S, U5SD

**Steps:**
1. Create `hw_db/cpus.csv` with the schema above, header row comments explaining each column, and the first 30 entries (the canonical Intel lineup). This is human-editable — designed for community PRs.
2. Write a small Python build script `hw_db/build_cpu_db.py` that reads the CSV, validates every row (valid Q16 ranges, no duplicate matches, referenced errata flags are in a known set), and emits `src/detect/cpu_db.c` as a static array.
3. Add `hw_db/build_cpu_db.py` invocation to the Makefile as a prerequisite for `cpu_db.obj`:
   ```
   src/detect/cpu_db.c : hw_db/cpus.csv hw_db/build_cpu_db.py
       python hw_db/build_cpu_db.py hw_db/cpus.csv src/detect/cpu_db.c
   ```
   This keeps the source of truth in a version-controllable CSV and regenerates C whenever it changes.
4. Write `cpu_db_lookup()` — walks the array, returns first matching entry. O(n) is fine at n=120.
5. Wire into Task 1.1's `detect_cpu()`: after class+CPUID probes populate family/model/stepping, call `cpu_db_lookup`. If found, populate `cpu.friendly`, `cpu.sspec`, `cpu.errata`, `cpu.ipc_expected_low`, `cpu.ipc_expected_high`. If NOT found, populate `cpu.friendly="<family/model/stepping — please submit via /UNKNOWN>"` at confidence MEDIUM and trigger the unknown-hardware path (Task 1.11).
6. Host tests: verify `cpu_db_lookup` with 20 known CPUID signatures returns correct friendly names; verify unknown signatures return NULL; verify the legacy match path (no CPUID) resolves 8088/V20/386 correctly.
7. Fill out `hw_db/cpus.csv` to the 80-entry target.
8. Commit CSV + generated C + Python script.

### Task 1.2 — FPU detection (detect/fpu.c)

**Methodology:** FNINIT, read control word via FNSTSW/FNSTCW, inspect bits.

- No FPU present → status word stays 0xFFFF after FNINIT
- 287 vs 387 distinguished by how infinity is handled (affine vs projective)
- Integrated FPU (486DX, Pentium, …) identified via CPUID feature bit or by the absence of external FPU signaling

**Steps:**
1. Write ASM probe: FNINIT + FNSTSW
2. Write 287/387 distinguishing probe (infinity handling)
3. Wire CPU-integrated-FPU check from CPUID results (Task 1.1 output in table)
4. Test on DOSBox-X presets with/without FPU emulation
5. Commit

### Task 1.2a — FPU identification database (detect/fpu_db.c)

**Goal:** turn the raw FPU probe (present/absent + 287/387/integrated discriminator) into a named FPU with vendor, process node where known, and quirks.

**Files:** `src/detect/fpu_db.h`, `src/detect/fpu_db.c`, `hw_db/fpus.csv`

**Schema:**
```c
typedef struct {
    const char *probe_result;   /* "none", "8087", "287", "387-affine", "387-projective",
                                   "integrated-486dx", "integrated-pentium",
                                   "rapidcad", "cyrix-fasmath", "iit-2c87", ... */
    const char *friendly;       /* "Intel 80387DX (Affine Mode)" */
    const char *vendor;         /* "Intel", "Cyrix", "IIT", "ULSI", "Weitek" */
    const char *notes;           /* "transcendental bug fixed in stepping B2" */
    unsigned char fpu_mhz_matches_cpu_mhz;  /* 1 if same clock as CPU, 0 if asynchronous */
} fpu_db_entry_t;
```

**Target coverage for v0.2 (~20 entries):**
- Intel 8087, 80287 (6MHz / 8MHz / 10MHz variants), 80287XL, 80387SX, 80387DX, 80487SX (the socketed 486SX coprocessor that disables the host CPU)
- Integrated FPUs: 486DX/DX2/DX4, Pentium P5/P54C
- Cyrix FasMath CX-83D87, EMC87
- IIT 2C87, 3C87
- ULSI 83S87, 83C87, 83C287
- Weitek 1167, 3167, 4167 (non-x87 but listed for completeness since they exist on some 386 boards)
- RapidCAD (reports as 486-CPU + 387-FPU but is a single pair)

**Steps:**
1. Create `hw_db/fpus.csv` with 20-entry initial table
2. Build script `hw_db/build_fpu_db.py` — same pattern as CPU DB
3. Wire into `detect_fpu()` — after the probe returns a `probe_result` string, look up in `fpu_db` and populate `fpu.friendly`, `fpu.vendor`, `fpu.notes`
4. Host test: 5 synthetic probe results → correct friendly names
5. Commit

### Task 1.3 — Memory detection (detect/mem.c)

**Methodology:** §5 updated. INT 12h for conventional, INT 15h AH=88h for extended (286+), INT 15h AX=E801h for >64MB reporting on late 486+ BIOSes. **Do NOT call INT 15h AX=E820h on pre-586 hardware** — E820h is an ACPI-era (1994+) extension for 32-bit OS bootloaders and is effectively absent on 386/486-era BIOSes; invoking it may return unexpected error codes or (on BIOSes that repurpose the range) trigger unrelated side effects. EMS/XMS via driver probe (INT 2Fh multiplex).

**Steps:**
1. INT 12h → conventional KB
2. INT 15h AH=88h → extended KB (skip if pre-286). Returns up to 64MB-1KB in AX. Different BIOSes saturate at different values — common observed saturations include 0xFFFF (65535 KB ≈ 64MB), 0xFC00 (64512 KB ≈ 63MB), 0xF000 (61440 KB ≈ 60MB), 0x3C00 (15360 KB = 15MB, the ISA-DMA bounce-buffer floor on some 286 BIOSes). Do NOT hardcode a specific saturation threshold.
3. **Always attempt INT 15h AX=E801h on CPU class ≥ 386** (E801h was defined by Phoenix/Compaq ~1994; many late-386 and most 486 BIOSes implement it, but support is inconsistent). Call returns AX = extended KB up to 16MB, BX = extended KB above 16MB in 64KB units (so BX × 64 + AX = total extended KB). Check CF — on success, compare the E801h total against AH=88h; use the larger value. If they disagree (one saturates, the other doesn't), flag confidence MEDIUM with a note in the INI. If CF is set on E801h, retain AH=88h value at confidence HIGH.
4. INT 2Fh AX=4300h → XMS driver signature. If present, INT 2Fh AX=4310h to get entry point, then call XMS function 08h (query free extended memory).
5. INT 67h function 40h → EMS status, function 42h → total/free page count. Guard with INT 2Fh AX=5300h EMM host probe first.
6. UMB probe via XMS function 10h (allocate UMB) and 11h (deallocate UMB) to measure free UMB without consuming it.
7. Update `docs/methodology.md` with the E820h exclusion rationale (ACPI-era, not available on pre-Pentium BIOSes) and the "use the larger of AH=88h and E801h" approach. Update CERBERUS.md §5 to remove E820h from the memory detection line.
8. Test on DOSBox-X presets with different memory configs: 256KB/640KB conventional; 1MB/4MB/16MB/32MB/64MB/96MB extended (covering all saturation regimes); HIMEM.SYS loaded / not loaded; EMM386 loaded / not loaded. Verify the 96MB case reports 96MB, not a saturated value.
9. Commit

### Task 1.4 — Cache detection (detect/cache.c)

**Methodology:** Timed stride access. Allocate a buffer larger than any plausible L1 (64KB ceiling for 486), walk at increasing strides, note the stride where access time jumps. That's the cache line size. Walk at increasing buffer sizes at cache-line stride, note where time jumps again — L1 size.

**Complications:**
- Below 386, caches are rare (external cache controllers on some 286 boards)
- CPUID cache descriptors on 486+ give direct values, but not all 486s populate them
- Results must be `confidence: MEDIUM` when inferred from timing, `HIGH` when from CPUID

**Steps:**
1. Write stride-walk harness (reuses timing module)
2. Implement line-size inference
3. Implement L1 size inference
4. On CPUID-capable CPUs, cross-check against CPUID leaf 2
5. Test on DOSBox-X 486 preset (emulated cache timing is fake — flag result confidence LOW when running in virtualized CPU)
6. Commit
7. Real-hardware test on a known 486 — this is where emulation falls down

### Task 1.5 — Bus detection (detect/bus.c)

**Methodology:**
- PCI: INT 1Ah AX=B101h BIOS32 services signature
- VLB: no standard probe — infer from CPU class (486) + VLB card presence via known ISA-port signatures
- ISA 8-bit vs 16-bit: read BIOS data area equipment word bits, or probe for 16-bit port responses on ISA range

Keep confidence LOW where inference is indirect. §4 design principle: report uncertainty honestly.

**Steps:**
1. PCI BIOS probe
2. ISA bit-width inference from CPU class + BDA
3. Effective bus clock estimation from a known ISA timing pattern (port write + read of a real card)
4. Skip VLB deep-detection for v0.2 — stub with "possibly VLB" if 486 detected, cite confidence LOW
5. Commit

### Task 1.6 — Video detection (detect/video.c + detect/video_db.c)

**Methodology:**
- INT 10h AH=1Ah (VGA detect, returns adapter type)
- INT 10h AH=12h BL=10h (EGA info)
- Segment probe: MDA at B000:0000h, CGA/EGA/VGA at B800:0000h
- Hercules detection: CGA-like + distinct status register behavior on port 3BAh
- **Chipset identification via signature-scan database** (Task 1.6a below)

**Steps:**
1. Write layered detect: try VGA (INT 10h AH=1A), fall back to EGA (INT 10h AH=12h), fall back to segment probe
2. Hercules probe via status register timing
3. Call `video_db_identify_chipset()` — scans video BIOS segment (C000:0000h to C000:7FFFh, stepping by 16 bytes) for known strings from `video_db`. Populates `video.chipset`, `video.chipset_vendor`, `video.bios_version` where detected.
4. Probe VESA BIOS Extensions: INT 10h AX=4F00h returns VBE info block. Extract: VBE version, OEM string, total video memory, and the supported-modes list. This gives rich identification on ~95% of post-1992 cards.
5. For Trident-specific, ATI-specific, and S3-specific deep-identification, run the vendor-specific register probes (e.g., S3 "unlock" sequence at 3D4h/3D5h indices 38h/39h reveals chip revision).
6. Commit

### Task 1.6a — Video chipset database (detect/video_db.c)

**Goal:** named identification for VGA/SVGA chipsets. "Generic VGA" becomes "Tseng Labs ET4000/W32i, 1MB, VGA BIOS 8.00N."

**Files:** `src/detect/video_db.h`, `src/detect/video_db.c`, `hw_db/video.csv`

**Schema:**
```c
typedef struct {
    const char *bios_signature;   /* substring to scan for in C000:xxxx */
    const char *vendor;            /* "Trident", "Cirrus Logic", "S3", "ATI", "Tseng", ... */
    const char *chipset;           /* "ET4000/W32i", "CL-GD5446", "Trio64V+", "Mach64 VT" */
    const char *family;            /* "svga", "vga", "ega-super", "mda" */
    unsigned int  min_vram_kb;     /* typical minimum VRAM */
    const char *notes;             /* "Tseng-specific register probe available at 3CEh/3CFh" */
} video_db_entry_t;
```

**Target coverage (~60 entries):**
- Pre-VGA: IBM MDA/CGA/EGA/PGC, Hercules HGC/InColor, AT&T DEB
- Early VGA: IBM VGA, Paradise PVGA1, Video7 V7VGA, Chips & Tech F82C441/452, Oak OTI-037/067/077/087
- Late VGA/SVGA: Trident 8800/8900/9000/9400/TGUI9440, Tseng ET3000/ET4000/ET4000-W32/ET6000, Cirrus Logic CL-GD5402..5480, S3 86C911/924/928/80x/Trio/Virge, ATI Mach8/Mach32/Mach64/Rage, Matrox MGA, Number Nine
- Plus VESA-reported chipset strings for the long tail

**Steps:**
1. `hw_db/video.csv` seeded with BIOS signature strings from published chipset references + InstLatx64 / VOGONS contributions
2. Build script → `src/detect/video_db.c`
3. `video_db_identify_chipset()` walks video BIOS segment once, matches first hit
4. Host test: 10 synthetic BIOS-signature dumps → correct chipset identification
5. Commit

### Task 1.7 — Audio detection (detect/audio.c)

### Task 1.7 — Audio detection (detect/audio.c)

**Methodology:** OPL2 uses address port 388h (write register number), data port 389h (write register contents). Canonical probe from the AdLib programmer's manual:

```
Timer probe sequence (OPL2 present iff bits 7:6 of status == 11b after):
  1. out 388h, 04h      ; address Timer Control Register (R4)
  2. out 389h, 60h      ; reset T1+T2 (clear timer status bits)
  3. out 388h, 04h      ; address R4 again
  4. out 389h, 80h      ; clear IRQ status
  5. in  AL, 388h       ; read status — expect bits 7:6 = 00b
  6. out 388h, 02h      ; address Timer 1 Register (R2)
  7. out 389h, 0FFh     ; set Timer 1 to roll over at next tick
  8. out 388h, 04h      ; address R4
  9. out 389h, 21h      ; start Timer 1, mask Timer 2
 10. wait >= 80us       ; use timing module
 11. in  AL, 388h       ; read status — expect bits 7:6 = 11b if OPL2 present
 12. out 388h, 04h      ; reset
 13. out 389h, 60h
```

OPL3 is distinguished by the upper status-register bits having a different pattern after reset, and by the presence of an "OPL3 extended" register bank (register 105h at port 38Bh write) which responds only on OPL3.

Sound Blaster detection uses the `BLASTER` env var. **Actual format is space-separated tokens with a letter prefix, not `key=value`:** e.g., `BLASTER=A220 I5 D1 H5 T6` where A=base port (hex), I=IRQ, D=8-bit DMA, H=16-bit DMA (SB16+), T=card type (1=SB, 2=SBPro, 3=SB2.0, 4=SBPro2, 6=SB16). Parse by walking the env string and switching on the leading letter. DSP reset sequence: write `1` to port `2x6h` (DSP reset), wait 3µs, write `0` to `2x6h`, wait 100µs, read `2xAh` (read data available — bit 7 should be set), read `2xAh` (should be `AAh`). Then DSP version query: write `E1h` to `2xCh` (write command), read major+minor from `2xAh`. DSP version classifies SB1/2/Pro/16/AWE.

**Steps:**
1. PC speaker — unconditional report (always present per IBM PC spec)
2. OPL2 probe — follow the 13-step sequence above exactly. Use `timing_wait_us(100)` from the timing module for the delay. If first status read (step 5) returns 11b already, OPL hardware is in unknown state — repeat the sequence once; if still 11b, flag AdLib probe as confidence LOW and proceed.
3. OPL3 distinguishing probe: after OPL2 confirmed, write to extended register bank (`OUT 38Ah, 05h` / `OUT 38Bh, 01h`) to enable OPL3 mode, re-read status — OPL3 has extra status bits set. If not OPL3, the writes are no-ops on OPL2.
4. BLASTER env parse (via `getenv("BLASTER")`). If set, DSP reset + version query. If BLASTER unset but AdLib detected, note "SB probe skipped (no BLASTER env)" — do NOT guess port 220h.
5. Call `audio_db_identify()` with the collected probe results (OPL type, SB DSP version, MPU-401 response) — populates `audio.friendly`, `audio.model` from the DB.
6. Update `docs/methodology.md` with the exact probe sequences and their sources.
7. Test on DOSBox-X with Sound Blaster disabled, SB1, SB Pro, SB16 configurations.
8. Commit

### Task 1.7a — Audio chipset database (detect/audio_db.c)

**Goal:** turn "SB Pro, OPL3" into "Sound Blaster Pro 2.0 (CT1600), OPL3 (YMF262)."

**Files:** `src/detect/audio_db.h`, `src/detect/audio_db.c`, `hw_db/audio.csv`

**Match keys:** DSP version (major.minor), OPL type (OPL2/OPL3/OPL4), presence of mixer (CT1345/CT1745), MPU-401 UART mode response, Gravis UltraSound detection (INT 11h equipment + GUS-specific port probe), Roland MT-32/SC-55 via MPU-401 system-exclusive probe.

**Target coverage (~30 entries):**
- Creative: SB 1.0 (CT1320), SB 1.5 (CT1320B), SB 2.0 (CT1350), SB Pro (CT1330), SB Pro 2.0 (CT1600), SB16 (CT1740/CT2230/CT2290/CT2940), SB16 ASP (CT1750), SB AWE32 (CT2760/CT3900), SB AWE64 (CT4500)
- AdLib: original AdLib (OPL2), AdLib Gold (OPL3 + YMZ263)
- Gravis: UltraSound Classic (GF1), UltraSound MAX, UltraSound ACE, UltraSound PnP
- MediaVision: Pro AudioSpectrum 8/16/Plus
- ESS: ES688, ES1688, ES1868, ES1869
- OPTi: 82C929, 82C930, 82C931
- Aztech: Sound Galaxy family
- Roland: MT-32 / LAPC-I / CM-32L / SC-55 (via MPU-401 SysEx)

**Steps:**
1. `hw_db/audio.csv` seeded from Creative's CT-number database (publicly documented), VOGONS hardware database (factual), Ralf Brown's Interrupt List
2. Build script → `src/detect/audio_db.c`
3. `audio_db_identify()` matches on composite key (OPL+DSP+mixer)
4. Host test: 8 synthetic probe signatures → correct model identification
5. Commit

### Task 1.8 — BIOS info (detect/bios.c)

Already ostensibly implemented in v0.1 per §3 — but the repo has no code. Implement fresh.

**Methodology:**
- BIOS date string at F000:FFF5h (8 chars, format MM/DD/YY)
- Copyright / vendor string: scan F000h for printable ASCII starting at known offsets (or byte-by-byte with heuristic)
- INT 15h extensions: probe AH=C0h (system config), AH=E820h, etc.
- PnP header: scan F000h for `$PnP` signature

**Steps:**
1. Read BIOS date
2. Scan for vendor string
3. Probe INT 15h extensions and record which are present
4. PnP header scan
5. Call `bios_db_identify()` — matches vendor string against `hw_db/bios.csv`. Populates `bios.vendor`, `bios.flavor` (Award/AMI/Phoenix/IBM/MR BIOS/Quadtel/Microid Research/...), known-version-era, and flags if the BIOS is from a motherboard family with documented quirks.
6. Motherboard OEM string: scan F000:E000h–F000:FFF0h for OEM-specific strings (Asus ID strings, Abit, Intel motherboard IDs). Best-effort, confidence MEDIUM.
7. Commit

### Task 1.8a — BIOS and motherboard database (detect/bios_db.c)

**Goal:** "Award BIOS 4.51PG" becomes "Award Modular BIOS v4.51PG (1995 era, typical on Socket 5/7 boards, known-good memory detection above 64MB)."

**Files:** `src/detect/bios_db.h`, `src/detect/bios_db.c`, `hw_db/bios.csv`, `hw_db/motherboards.csv`

**Schema:** match substrings in the BIOS vendor string against a database of known BIOS families with era, capability flags (E801h support, PnP, shadow RAM regions), and known-quirk notes.

**Target coverage:** Award, AMI, Phoenix, IBM, MR BIOS, Quadtel, Microid Research, DTK, Mylex, Compaq, Dell, HP Vectra, Zenith, Tandy, Olivetti — ~40 BIOS family entries, plus a separate `motherboards.csv` with ~100 common board OEM strings (Asus P55T2P4, Abit IT5H, Intel Advanced/EV, etc.)

**Steps:**
1. Seed `bios.csv` from published BIOS reference lists and motherboards.csv from the VOGONS hardware database
2. Build script
3. Host test
4. Commit

### Task 1.9 — UI: three-pane layout + confidence meter

**Files:** `src/core/display.c` (extend), `src/core/ui.c` (new)

**Steps:**
1. Design the 80×25 text-mode three-pane frame:
   ```
   ╔══════════════════════════════════════════════════════════════════════════════╗
   ║ CERBERUS 0.2.0                              [DETECT] [    ] [    ]    [calib]║
   ╠════════════════════════╦════════════════════════╦═════════════════════════════╣
   ║ DETECT                 ║ DIAGNOSE               ║ BENCHMARK                   ║
   ║ cpu       80486DX   ▓▓▓║                        ║                             ║
   ║ fpu       integrated▓▓▓║                        ║                             ║
   ║ memory    640+15360 ▓▓▒║                        ║                             ║
   ║ ...                    ║                        ║                             ║
   ╠════════════════════════╩════════════════════════╩═════════════════════════════╣
   ║ status                                                                       ║
   ╚══════════════════════════════════════════════════════════════════════════════╝
   ```
2. Confidence meter: 3-char gauge (`▓▓▓` high, `▓▓▒` medium, `▓▒▒` low) next to each value
3. Mono fallback: `[H]`/`[M]`/`[L]` prefix tag where color unavailable
4. Status line: current subsystem under test, time elapsed
5. Test rendering on DOSBox-X MDA, CGA, EGA, VGA presets
6. Commit

### Task 1.11 — Unknown-hardware submission path (detect/unknown.c)

**Goal:** every time CERBERUS encounters a CPU, FPU, video, audio, or BIOS signature not in the database, it captures the raw probe results to a dedicated submission file and prompts the user to contribute back. This is what turns CERBERUS from a closed identification tool into a **community-grown database** — exactly the §13 Barely Booting content angle ("the DOS scene contributes a weird Cyrix the tool didn't know about → next release recognizes it").

**Files:** `src/detect/unknown.c`, `hw_db/submissions/README.md`

**Behavior:**
1. When any `*_db_lookup()` returns NULL for a well-formed probe result, the caller calls `unknown_record(subsystem, raw_probe_data)`.
2. `unknown_record` appends a structured entry to `CERBERUS.UNK` in the same location as the crash breadcrumb (CWD → %TEMP% → disabled) with:
   - Subsystem (cpu/fpu/video/audio/bios)
   - Raw probe result (CPUID dump as hex, EFLAGS probe outcomes, vendor-string scan hits, VBE info block, DSP version, BIOS vendor string bytes)
   - Tool version, date, emulator id, hardware signature
3. At end-of-run, if `CERBERUS.UNK` has any entries, the end-of-run summary card shows:
   ```
   ┌─ UNKNOWN HARDWARE CAPTURED ────────────────────────────────────┐
   │ 1 CPU, 1 video chipset not in the CERBERUS database.            │
   │                                                                 │
   │ Help the database grow — submit CERBERUS.UNK as a GitHub issue: │
   │ https://github.com/tonyuatkins-afk/CERBERUS/issues/new          │
   │ (use the "hardware submission" template)                        │
   └─────────────────────────────────────────────────────────────────┘
   ```
4. `hw_db/submissions/README.md` documents the submission workflow, the CSV schema, and how to add a new entry via PR with one-line `git add hw_db/cpus.csv` + regenerated C.
5. GitHub issue template at `.github/ISSUE_TEMPLATE/hardware-submission.md` asks for: CERBERUS.UNK contents, machine description (make/model/year), any human-known identification the user has.

**Why this is load-bearing for the "feature-rich" goal:** the DB is always the long tail — no single author can cover every Cyrix rebadge, every Taiwan OEM VGA card, every late-90s SB clone. A 120-entry seed DB + a 10-minute submission workflow becomes a 500-entry DB by v1.0.

**Steps:**
1. Implement `unknown_record()` writing append-only to `CERBERUS.UNK`
2. Implement end-of-run summary card rendering for unknowns
3. Create `.github/ISSUE_TEMPLATE/hardware-submission.md`
4. Write `hw_db/submissions/README.md`
5. Host test: call unknown_record with synthetic data, verify file contents match schema
6. Commit

### Task 1.10 — Phase 1 quality gate (5-round adversarial, per TAKEOVER pattern)

User's established pattern from `feedback_quality_gate_patterns.md` and `project_takeover.md`: 5 rounds, target ~12 bugs across rounds.

**Round focus rotation:**
1. Correctness on canonical hardware (486DX, 386DX, 286, 8088 DOSBox-X presets)
2. Edge hardware (V20/V30, Cyrix, RapidCAD, AMD 5x86 if emulation exists)
3. Minimal hardware (8088 + 256KB + MDA — can it even boot and run?)
4. Adversarial input (corrupted BIOS areas, unusual CMOS, hang recovery via breadcrumb)
5. Cross-consistency (do detect modules agree with each other? e.g., CPUID says 486 but FPU probe says no FPU on a DX part)

**Mandatory real-hardware validation (gate-blocking — not optional):**
- [x] Run on ≥1 real 486-class machine from the vintage fleet — BEK-V409 / i486DX-2 / 64MB / S3 Trio64 VLB / Vibra 16S (2026-04-18, corpus at [`tests/captures/486-real-2026-04-18/`](../../tests/captures/486-real-2026-04-18/))
- [ ] Run on ≥1 real 386-class machine (or 286 if no 386 is available)
- [ ] Run on ≥1 real 8088/V20/V30-class machine
- [ ] Attach each resulting INI file to the GitHub v0.2 release
- [x] Any discrepancy between DOSBox-X output and real-hardware output where both claim HIGH confidence is a gate failure — either fix detection or downgrade confidence (five discrepancies surfaced on 486 gate, all fixed in `eeba319`)

**Hardware database validation:**
- [ ] CPU DB has ≥80 entries; FPU DB ≥20; video DB ≥60; audio DB ≥30; BIOS DB ≥40; motherboard DB ≥50 (current: CPU 34 / FPU 14 / video 28 / audio 31 / BIOS 21 — 128 total, all under target; opportunistic growth via community submissions and future real-hardware gates)
- [x] Every real-hardware test machine gets a friendly-named identification (not "unknown") — 486 DX-2 bench box identified correctly post-`eeba319` + `7e4bdcb`; residual OPL intermittency ([#2](https://github.com/tonyuatkins-afk/CERBERUS/issues/2)) on one of two cold boots lets the audio T-key lookup fall back but does not produce "unknown"
- [x] If any real-hardware machine produces an unknown identification, either add the entry to the CSV (with source-of-data citation in the commit message) or document why it must remain unknown — S3 Trio64 CR30 chipid probe + Vibra 16S DSP 4.13 T6 entries added in `eeba319` / `7e4bdcb`
- [x] `CERBERUS.UNK` submission file generated correctly on a synthetic unknown-CPU test
- [x] End-of-run "unknown hardware captured" summary renders correctly when unknowns are present

Same real-hardware requirement applies to every subsequent phase gate (2, 3, 4). Reason: Phase 4's consistency engine will be calibrated against whatever data Phase 1/2/3 produce; if those phases only saw emulator data, the rule thresholds will be tuned to emulation artifacts and will false-positive on real hardware in v0.5. DOSBox-X cache/timing is explicitly synthetic (see Task 1.4 step 5).

**Exit criteria:** Zero reproducible crashes. All high-confidence results accurate on both emulator and real hardware. Uncertain results carry appropriate confidence levels. Real-hardware INIs attached to the release.

**Commit tag:** `v0.2.0`, create GitHub release with INI samples from 4+ hardware configs (mix of emulator + real iron) attached.

---

## Phase 2: v0.3 — DIAGNOSE

**Goal:** Every subsystem detected in Phase 1 gets a correctness test. Results attach `verdict_t` to the result table.

**Entry criteria:** Phase 1 gate passed and v0.2 tagged.

**Exit criteria:** §5 Head II table fully implemented. Quality gate passed. On a known-bad machine (e.g., RAM with stuck bits), CERBERUS accurately identifies the fault.

**Module contracts:** every `diag/<subsys>.c` exposes `diag_<subsys>(result_table_t *t)`. Reads existing detect results from the table, runs targeted tests, attaches verdicts.

**Planning approach:** re-run the `planning` skill at the start of Phase 2. Use the then-current `src/detect/*` as ground truth. Bite-sized tasks should follow the same shape as Phase 1 (test → implement → DOSBox-X verify → commit).

**Key design notes (pre-planning):**
- ALU integrity: run 1000s of precomputed operation pairs, compare against expected
- Memory test: moving-inversion pattern, address-in-address, random with seed
- FPU correctness: known-answer tests for add/mul/div at each precision; transcendental table lookup
- Cache coherence: write pattern, invalidate (where possible), read back
- Video RAM: direct-write-and-read at every offset, snow pattern on CGA, plane-walk on EGA/VGA
- DMA liveness: program a harmless DMA transfer (speaker buffer, idle channel), verify completion

**Risks:** diagnostics MUST NOT damage hardware. No speaker-destroying frequencies. No prolonged bus contention. No DMA that overwrites DOS kernel memory. Include a "safety preflight" in the orchestrator.

**Quality gate:** 5 rounds, same pattern as Phase 1.

---

## Phase 3: v0.4 — BENCHMARK

**Goal:** Every subsystem detected in Phase 1 produces repeatable measurements (quick mode: 1 pass; calibrated: N passes with median + CoV).

**Entry criteria:** Phase 2 gate passed and v0.3 tagged.

**Exit criteria:** §5 Head III table implemented. On a 486DX/33, CERBERUS reports MIPS/MFLOPS/bandwidth numbers that match published Landmark / PC Magazine Labs benchmarks within 10%. Quality gate passed.

**Module contracts:** every `bench/<subsys>.c` exposes `bench_<subsys>(result_table_t *t, run_mode_t mode, unsigned char runs)`.

**Planning approach:** re-run `planning` at phase start. Timing framework already exists from Phase 0 — benchmarks are about instruction-mix design and buffer layout.

**Key design notes:**
- CPU integer (Dhrystone-equivalent): fixed instruction mix, PIT-timed. Do not use variable-time operations. Pre-computed data pool to defeat constant folding.
- FPU (Whetstone-equivalent): x87 instruction mix, PIT-timed. Precision-control–aware (80bit vs 64bit vs 32bit results reported separately).
- Memory bandwidth: REP MOVSW (copy), REP STOSW (write), scan loop (read). Buffer sized to defeat L1+L2.
- Memory latency: pointer-chase through shuffled buffer, buffer ≥4× L2 size.
- Cache bandwidth: stride tests at working-set sizes 1KB, 4KB, 16KB, 64KB — each should show a knee at the relevant cache boundary.
- Video throughput: direct writes to B000h (mono) / B800h (CGA/text) / A000h (VGA graphics). KB/s.
- Disk throughput: INT 13h raw sector reads. Skip if no disk or explicit /NODISK flag.

**Thermal notes (fed into Phase 4):** every benchmark records per-pass values, not just the median. Phase 4's thermal module walks those values looking for monotonic drift.

**Quality gate:** 5 rounds.

---

## Phase 4: v0.5 — Consistency Engine + Thermal Stability

**Goal:** The project's signature feature. Cross-check detect/diagnose/benchmark, report disagreements as a foregrounded UI moment. Track drift across calibrated passes.

**Entry criteria:** Phase 3 gate passed and v0.4 tagged.

**Exit criteria:** Consistency rules fully documented in `docs/consistency-rules.md`. At least 8 canonical rules implemented and test-fixture-verified. Disagreement alert renders distinctly on MDA/CGA/EGA/VGA. Thermal drift detection flags a synthetic monotonic-decline test fixture. Quality gate passed.

**Module design:**

```c
typedef struct {
    const char *lhs_key;       /* e.g., "cpu.detected" */
    const char *op;            /* "implies", "contradicts", "requires" */
    const char *rhs_key;       /* e.g., "bench.cpu.mips_equivalent" */
    int (*check)(const result_table_t *t);
    const char *explanation;
} consist_rule_t;
```

Each rule's `check` returns 0=OK, 1=WARN, 2=FAIL. Explanations are human-readable strings that land in the INI `[consistency]` section.

**Canonical rules to implement (extend this list in phase planning):**
1. `cpu.detected=486DX` → `fpu.detected=integrated`
2. `cpu.detected=486SX` → `fpu.detected=none|287|387` (external only)
3. `cpu.detected=386SX` → `bus.width=16`
4a. **CPU clock measurement vs independent reference.** `cpu.clock_mhz` is derived from PIT Channel 2. Cross-check against an independent time source: count CPU cycles that elapse between two BIOS tick transitions at 0040:006Ch (BIOS ticks are ~18.2 Hz, driven by PIT Channel 0 — independent of Channel 2). If the two clock estimates disagree by >15%, flag `timing_inconsistency`. This catches systematic `timing.c` bias (including the 16-bit overflow trap if it ever slips past host tests).
4b. **MIPS vs clock×IPC expectation.** Only if rule 4a passes: `bench.cpu.mips ∈ [cpu.clock_mhz × class_ipc_low, cpu.clock_mhz × class_ipc_high]` where class_ipc is a known range per CPU class (e.g., 486 ≈ 0.3–0.5 MIPS/MHz, 386 ≈ 0.15–0.25). If benchmark falls outside, flag — possible counterfeit, cache disabled, thermal throttle, faulty pin. **Document explicitly in `docs/consistency-rules.md` that this rule cannot detect faults affecting both the clock measurement and the MIPS measurement equally (e.g., uniform emulator-induced slowdown) — it only catches asymmetric failures.**
5. `diagnose.fpu=pass` ↔ `bench.fpu` produced a result
6. `detect.memory.extended > 0` → `cpu.class ≥ 286`
7. `detect.video=VGA` → available benchmark modes include VGA
8. `detect.cache.l1_size` consistency with `bench.cache` stride knees — **but flag as low-confidence rule because both detect and bench use stride timing through the same `timing.c` infrastructure; an implementation bug in either side will be correlated, not independent. Consider this rule advisory until a CPUID-sourced cache-size value exists as the independent reference.**

Additional rules are easier to add once real data comes in; the framework is what matters. Document each rule's failure-mode coverage (what it detects, what it cannot) in `docs/consistency-rules.md` — this is what distinguishes CERBERUS from opaque benchmark scores.

**Thermal module:** given an array of per-pass benchmark values, detect monotonic drift using the **Mann-Kendall trend test**. Mann-Kendall is a non-parametric rank-based test specifically designed for small-N time series — it does not assume Gaussian residuals or a minimum sample size, unlike linear regression with t-testing. For N=7 (the new default per Task 0.2 and PF-7) with α=0.05, the critical S statistic is ±17 (two-tailed). If S < −17 AND median absolute deviation exceeds the measurement noise floor (established in Phase 0 timing tests), flag `thermal_stability=unstable`.

Default calibrated N bumped from 5 to **7** in PF-7 CLI options — minimum 7 passes to give Mann-Kendall enough signal to detect ~2%/pass drift at 80% power. Users can still pass `/C:5` for a faster quick-calibrated run but will get `thermal_stability=insufficient_samples` instead of a false-positive or false-negative.

**Acceptance test for Phase 4:** the fixture exercises three synthetic data patterns — (a) flat within noise → expect `stable`, (b) 1%/pass decline over N=7 → expect `stable` (within noise, shouldn't false-positive), (c) 3%/pass decline over N=7 → expect `unstable`. Document the sensitivity envelope in `docs/methodology.md`.

**UI signature moment — disagreement alert:**

```
╔══════════════════════════════════════════════════════════════════════════════╗
║ ◄◄◄ CONSISTENCY FLAG ►►►                                                     ║
║                                                                              ║
║ HARDWARE CLAIMS:     80486DX                                                 ║
║ BENCHMARK MEASURES:  286-class performance                                   ║
║                                                                              ║
║ This machine identifies as a 486DX but runs integer benchmarks at            ║
║ roughly 1/10th the expected rate. Possible causes:                           ║
║   - Counterfeit or remarked CPU                                              ║
║   - Cache disabled in BIOS                                                   ║
║   - Thermal throttling                                                       ║
║   - Faulty CPU pin / bus timing                                              ║
║                                                                              ║
║ See docs/consistency-rules.md rule #4 for methodology.                       ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

On color modes, this renders with yellow/red accents. On mono, inverse video. This is the screenshot people post to VOGONS.

**Quality gate:** 5 rounds, with extra focus on rule false-positive rates.

---

## Phase 5: v0.6 — NetISA Upload (DECOUPLEABLE)

**Goal:** `/U` flag triggers HTTPS POST to NetISA endpoint, passing the INI payload.

**Risk:** blocked by NetISA availability (separate project per memory `project_barelybooting.md`). If NetISA slips, CERBERUS v1.0 should ship **without** upload rather than waiting.

**Decoupling plan:**
- Phase 0's `src/upload/upload.c` ships as a stub that returns `UPLOAD_DISABLED` and prints "Upload disabled in this build (rebuild with NETISA=1 to enable)." This stub compiles into every build so the `main.c` call site remains unconditional — no `#ifdef` at the call site, no dead-code warnings.
- Phase 5's real implementation is gated at compile time: `upload.c` contains `#ifdef NETISA_ENABLED` around the real impl; the else branch keeps the stub body. `wmake NETISA=1` defines `NETISA_ENABLED`. `main.c` remains untouched across the decoupling boundary.
- Default build excludes NetISA code. `wmake NETISA=1` includes it.
- Phase 5's CI/QA must verify BOTH paths: (a) default build → `/U` prints "disabled" and exits with non-zero, INI still written; (b) `NETISA=1` build → `/U` attempts upload. Both paths have target-side tests.
- If NetISA ships before CERBERUS v1.0, flip the default define in the Makefile
- If it doesn't, release v1.0 without NetISA, follow with v1.1 when available

**Entry criteria:** NetISA TSR API stable and documented. (External dependency — status check before phase start.)

**Exit criteria:** On a system with NetISA TSR loaded, `CERBERUS /U` uploads the INI and prints the server's acknowledgment. Failure modes (TSR absent, network down, endpoint 5xx) handled gracefully.

**Module contracts:** `upload/upload.c` exposes `upload_ini(path) → status`. Calls NetISA via INT (TBD NetISA INT number).

**Quality gate:** 3 rounds minimum (less surface than DETECT/DIAGNOSE/BENCHMARK), extra rounds on the failure-mode paths.

---

## Phase 6: v1.0 — Release

**Goal:** Tagged public release on GitHub, itch.io page, VOGONS announcement, dosgame.club post.

**Entry criteria:** Phases 0–5 gates passed (or Phase 5 explicitly descoped per decoupling plan). License decided (MIT — decided in PF-1 effectively; confirm before tag).

**Exit criteria:** v1.0 tag, release artifacts uploaded, itch.io page live, VOGONS thread posted.

### Task 6.1 — Sample INI corpus

Run CERBERUS on every machine / DOSBox-X preset available. Commit results under `samples/`:
```
samples/
  8088-256k-mda.ini
  286-1m-cga.ini
  386sx-4m-ega.ini
  386dx-8m-vga.ini
  486dx-16m-vga.ini
  486dx4-32m-vga.ini
```

### Task 6.2 — Site + README updates

Per `feedback_update_site.md`:
- Update barelybooting.com with CERBERUS project page
- Link to itch.io, GitHub, sample outputs
- Short demo GIF or screenshot set (use `capture.ps1` per `reference_capture_toolkit.md`)

### Task 6.3 — Packaging

Three artifacts per §12:
1. `cerberus-1.0.zip` — EXE + README + sample INIs
2. `cerberus-1.0-dosboxx.zip` — EXE + preconfigured DOSBox-X conf for modern users
3. `cerberus-1.0-src.tar.gz` — source for archivists

### Task 6.4 — Announcement

- VOGONS thread in the appropriate subforum (General Old Hardware or DOS Programming)
- dosgame.club Mastodon post
- itch.io devlog
- YouTube build-log episode if one's queued per §13

### Task 6.5 — Tag and release

```
git tag -a v1.0.0 -m "CERBERUS 1.0 — first public release"
git push --tags
gh release create v1.0.0 --title "CERBERUS 1.0" --notes-file RELEASE_NOTES.md \
  cerberus-1.0.zip cerberus-1.0-dosboxx.zip cerberus-1.0-src.tar.gz
```

---

## Cross-Cutting Concerns

### Testing strategy

**Host-side (runs on dev Windows box via Watcom Win32/Linux build):**
- `tests/host/` directory
- Unit tests for pure-logic modules: report (INI serialization, signature), consist (rule evaluation), timing math helpers, crumb (file ops)
- Run in CI pre-commit (eventually — GitHub Actions with a Watcom Linux container)
- Fast feedback loop for the >50% of code that's portable

**Target-side (runs in DOSBox-X):**
- `tests/target/` directory — DOS batch scripts invoking CERBERUS with fixture inputs
- `dosrun.py` automation per memory `reference_devenv_relay.md`
- Golden-INI comparison with tolerance for timing-derived values
- Presets for 8088 / 286 / 386 / 486 cover the hardware class matrix

**Real hardware:**
- Pre-release validation on at least one machine per class (user's vintage rigs)
- Document the test fleet in `docs/test-fleet.md`

**Emulation caveats:**
- Benchmark numbers in DOSBox-X are synthetic; confidence is capped at MEDIUM when emulation is detected
- Emulator detection is implemented in Task 1.0 (`detect/env.c`), runs before all other detect modules, and feeds `confidence_penalty` to downstream `report_add` calls
- Cache/timing tests particularly unreliable in emulation — confidence penalty captures this; document in `docs/methodology.md`

### Quality gates

User's pattern from `feedback_quality_gate_patterns.md` and `project_takeover.md`: 5 rounds of adversarial review per milestone, expect ~12 bugs total surfaced.

**Gate template per milestone:**
1. Fresh-eyes read of new code (not the author)
2. Hardware-class coverage check (every preset exercised)
3. Edge-hardware pass (Cyrix, V20/V30, RapidCAD, AMD 5x86)
4. Adversarial input (corrupted BIOS ROM, unusual CMOS, manually-hung breadcrumb recovery)
5. Cross-consistency check (results internally coherent)

Every bug found during the gate is fixed, and the fix gets its own "review round" check that it didn't introduce a regression (per `feedback_quality_gate_patterns.md`).

### UI layer

Single text-mode UI with progressive enhancement per earlier discussion:
- MDA/Hercules: intensity + underline + inverse only
- CGA: green/red/yellow palette (avoid magenta/cyan), retrace-synced writes
- EGA/VGA: full 16-color semantic palette (cyan headers, green pass, red fail, yellow warn, white data)
- VGA optional: cracktro splash (skippable, opt-in via /INTRO flag — default off so the tool stays instrument-grade)

Three-pane frame populated left-to-right as DETECT → DIAGNOSE → BENCHMARK run. Confidence meters next to every value. Disagreement alert is the signature visual moment (Phase 4).

### Documentation

Maintained in parallel with code, not after:
- `README.md` — kept fresh per `feedback_update_site.md`
- `CERBERUS.md` — the master spec, updated as decisions propagate (PF-1 through PF-5)
- `docs/methodology.md` — filled in as each subsystem's measurement method stabilizes
- `docs/consistency-rules.md` — one section per rule, methodology + explanation
- `docs/contributing.md` — coding conventions (DOS conventions per `feedback_dos_conventions.md`), AI assistance disclosure (§14), PR flow

---

## Risk Register

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| NetISA delivery slips past CERBERUS v1.0 timeline | Medium | High | Phase 5 decoupling plan — ship v1.0 without `/U`, follow with v1.1 |
| Name collision with existing "Cerberus" tool | Low (check PF-1) | High if discovered post-launch | Close PF-1 before any public announcement |
| Benchmark numbers don't match published references on real hardware | Medium | Medium | Cross-reference Landmark / PC Mag Labs; accept ±10% as success |
| 8088 floor binary size exceeds 64KB with full feature set | Medium | Medium | Monitor per-phase; if breached, switch to compact model and split INI writer into overlay |
| DOSBox-X emulation fidelity insufficient for consistency-engine rule validation | High | Medium | Document emulation caveats; rely on real hardware for final validation |
| Real-hardware flakiness stalls quality gates | Medium | Medium | Crash-recovery breadcrumb (Task 0.5); maintain a small fleet of known-good machines |
| Cyrix/NEC/AMD edge cases require per-vendor quirk code | High | Low | Budget 2 sittings per vendor in Phase 1 Task 1.1 |
| PIT Channel 2 wiring quirks on XT clones | Medium | Medium | Runtime sanity probe at timing_init(); fall back to BIOS tick counter if PIT C2 looks wrong; document in methodology.md |

---

## Pre-flight Items Owned by Author

Close before Phase 0 starts:

- [x] **PF-1** Name collision check — do first, before anything else
- [x] **PF-2** Memory model — update CERBERUS.md §9 to "medium memory model + far-buffer convention for >64KB benchmark data"
- [x] **PF-3** "Consistency Engine" vs "Truth Engine" — drop the parenthetical in §6
- [x] **PF-4** Schema version — add `schema_version` to §6 example
- [x] **PF-5** Upload privacy scope — add non-collection list to §10
- [x] **PF-6** Re-plan schema-review protocol — note at top of plan header applies
- [x] **PF-7** CLI switch naming consistency + default calibrated N bumped to 7 — update §7 of spec and `print_help()` in main.c

---

## Open Questions (Defer to Phase Start)

- **Phase 1:** How deep to go on chipset signature detection for video? Best-effort vs exhaustive.
- **Phase 2:** Safety preflight design for diagnostic module — what does "cannot damage hardware" look like as a shared harness?
- **Phase 3:** How much instruction-mix tuning matters for Dhrystone/Whetstone equivalence vs. just "our own benchmark"? Propose the latter — don't chase Landmark compatibility, chase methodology transparency.
- **Phase 4:** How many consistency rules is "enough" for v0.5? Proposal: start with 8 canonical, add on discovery, document each in `docs/consistency-rules.md`.
- **Phase 5:** If NetISA API changes mid-development, does CERBERUS bump a minor or patch version on its next release?
