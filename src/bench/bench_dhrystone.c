/*
 * Dhrystone 2.1 — integer-throughput benchmark, Reinhold Weicker, 1988.
 *
 * Ported from the public-domain ANSI C reference implementation. Semantics
 * match version 2.1 (the canonical DOS-scene reference); we do not adopt
 * 2.0 or 2.2 variations. Source lineage: ACM SIGPLAN Notices August 1988,
 * widely mirrored via netlib. Public domain per Weicker's release note.
 *
 * Port adaptations for CERBERUS:
 *   - No malloc. Rec_Type instances are static, not heap-allocated; the
 *     reference's malloc pair becomes two file-scope storage objects.
 *   - No stdio banner output. The reference prints a long "DHRYSTONE
 *     PROGRAM ... Execution starts" preamble — we emit rows to the INI
 *     result table instead, saving ~500 bytes of DGROUP.
 *   - Arr_2_Glob [50][50] = 5000 bytes is too large for near data in
 *     Watcom medium model. Declared __far per the codebase's pre-flight
 *     PF-2 far-buffer convention. Access via __far pointers in Proc_8.
 *   - Auto-calibration: a sub-second warmup measures scale factor, then
 *     the real run targets ~5 seconds on the detected CPU. Fallback to a
 *     fixed 50,000 iterations if warmup returns nonsense.
 *   - PIT-timed via timing_start / timing_stop rather than the reference's
 *     time() / clock() which don't work portably on real-mode DOS.
 *
 * CheckIt reference on the BEK-V409 bench box: 33,609 Dhrystones/sec at
 * DSP-measured 66.74 MHz. Our port targets ±5% of that number. Wider
 * divergence is a port defect, not a measurement quirk.
 */

#include <stdio.h>
#include <string.h>
#include "bench.h"
#include "../core/timing.h"
#include "../core/report.h"

/* ------------------------------------------------------------------- */
/* Dhrystone type system                                                */
/* ------------------------------------------------------------------- */

typedef enum { Ident_1, Ident_2, Ident_3, Ident_4, Ident_5 } Enumeration;

typedef int   One_Thirty;
typedef int   One_Fifty;
typedef char  Capital_Letter;
typedef int   Boolean;
typedef char  Str_30[31];
typedef int   Arr_1_Dim[50];
typedef int   Arr_2_Dim[50][50];

typedef struct record {
    struct record *Ptr_Comp;
    Enumeration    Discr;
    union {
        struct {
            Enumeration    Enum_Comp;
            int            Int_Comp;
            char           Str_Comp[31];
        } var_1;
        struct {
            Enumeration    E_Comp_2;
            char           Str_2_Comp[31];
        } var_2;
        struct {
            Capital_Letter Ch_1_Comp;
            Capital_Letter Ch_2_Comp;
        } var_3;
    } variant;
} Rec_Type;

typedef Rec_Type *Rec_Pointer;

/* ------------------------------------------------------------------- */
/* Global storage                                                       */
/* ------------------------------------------------------------------- */

/* `volatile` on every loop-touched global is load-bearing for DCE
 * suppression. Weicker 1984 §"Implementation Pitfalls" spells out the
 * failure mode: an optimizing compiler that sees a write-only static
 * is free to eliminate the store. Watcom -ox does exactly that and
 * produced a 23× overreporting v7 run on the 486 DX-2 (782,472 vs
 * CheckIt's 33,609). `volatile` here forces every read/write to memory
 * regardless of what downstream code does with the value. Belt and
 * braces: the anti-DCE checksum emit at the end of bench_dhrystone()
 * below gives the optimizer an externally-linked consumer it cannot
 * prove is unused. Either mechanism alone should suffice; both together
 * guarantee correctness even if the compiler gets cleverer. */
static volatile Rec_Pointer Ptr_Glob, Next_Ptr_Glob;
static volatile int         Int_Glob;
static volatile Boolean     Bool_Glob;
static volatile char        Ch_1_Glob, Ch_2_Glob;
static volatile int         Arr_1_Glob[50];
static volatile int __far   Arr_2_Glob[50][50];  /* 5000B FAR per PF-2 */

static Rec_Type    Rec_1_Storage, Rec_2_Storage;

/* INI-emit display buffers — report_add_* stores pointers verbatim. */
static char bench_dhry_elapsed_val[24];
static char bench_dhry_iters_val[24];
static char bench_dhry_per_sec_val[24];

/* ------------------------------------------------------------------- */
/* Procedures + functions — Dhrystone 2.1 reference                     */
/* ------------------------------------------------------------------- */

/* Forward declarations so Proc_1 can reference Func_1 etc. */
static Enumeration Func_1(Capital_Letter Ch_1_Par_Val, Capital_Letter Ch_2_Par_Val);
static Boolean     Func_2(Str_30 Str_1_Par_Ref, Str_30 Str_2_Par_Ref);
static Boolean     Func_3(Enumeration Enum_Par_Val);
static void        Proc_1(Rec_Pointer Ptr_Val_Par);
static void        Proc_2(One_Fifty *Int_Par_Ref);
static void        Proc_3(Rec_Pointer *Ptr_Ref_Par);
static void        Proc_4(void);
static void        Proc_5(void);
static void        Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par);
static void        Proc_7(One_Fifty Int_1_Par_Val, One_Fifty Int_2_Par_Val,
                          One_Fifty *Int_Par_Ref);
static void        Proc_8(volatile int *Arr_1_Par_Ref,
                          volatile int (__far *Arr_2_Par_Ref)[50],
                          int Int_1_Par_Val, int Int_2_Par_Val);

static void Proc_1(Rec_Pointer Ptr_Val_Par)
{
    Rec_Pointer Next_Record = Ptr_Val_Par->Ptr_Comp;
    *Ptr_Val_Par->Ptr_Comp = *Ptr_Glob;
    Ptr_Val_Par->variant.var_1.Int_Comp = 5;
    Next_Record->variant.var_1.Int_Comp = Ptr_Val_Par->variant.var_1.Int_Comp;
    Next_Record->Ptr_Comp = Ptr_Val_Par->Ptr_Comp;
    Proc_3(&Next_Record->Ptr_Comp);
    if (Next_Record->Discr == Ident_1) {
        Next_Record->variant.var_1.Int_Comp = 6;
        Proc_6(Ptr_Val_Par->variant.var_1.Enum_Comp,
               &Next_Record->variant.var_1.Enum_Comp);
        Next_Record->Ptr_Comp = Ptr_Glob->Ptr_Comp;
        Proc_7(Next_Record->variant.var_1.Int_Comp, 10,
               &Next_Record->variant.var_1.Int_Comp);
    } else {
        *Ptr_Val_Par = *Ptr_Val_Par->Ptr_Comp;
    }
}

static void Proc_2(One_Fifty *Int_Par_Ref)
{
    One_Fifty  Int_Loc;
    Enumeration Enum_Loc;
    Int_Loc = *Int_Par_Ref + 10;
    Enum_Loc = Ident_2;
    do {
        if (Ch_1_Glob == 'A') {
            Int_Loc -= 1;
            *Int_Par_Ref = Int_Loc - Int_Glob;
            Enum_Loc = Ident_1;
        }
    } while (Enum_Loc != Ident_1);
}

static void Proc_3(Rec_Pointer *Ptr_Ref_Par)
{
    if (Ptr_Glob != (Rec_Pointer)0) {
        *Ptr_Ref_Par = Ptr_Glob->Ptr_Comp;
    }
    Proc_7(10, Int_Glob, &Ptr_Glob->variant.var_1.Int_Comp);
}

static void Proc_4(void)
{
    Boolean Bool_Loc;
    Bool_Loc  = Ch_1_Glob == 'A';
    Bool_Glob = Bool_Loc | Bool_Glob;
    Ch_2_Glob = 'B';
}

static void Proc_5(void)
{
    Ch_1_Glob = 'A';
    Bool_Glob = 0;  /* FALSE */
}

static void Proc_6(Enumeration Enum_Val_Par, Enumeration *Enum_Ref_Par)
{
    *Enum_Ref_Par = Enum_Val_Par;
    if (!Func_3(Enum_Val_Par)) {
        *Enum_Ref_Par = Ident_4;
    }
    switch (Enum_Val_Par) {
        case Ident_1: *Enum_Ref_Par = Ident_1; break;
        case Ident_2:
            if (Int_Glob > 100) *Enum_Ref_Par = Ident_1;
            else                *Enum_Ref_Par = Ident_4;
            break;
        case Ident_3: *Enum_Ref_Par = Ident_2; break;
        case Ident_4: break;
        case Ident_5: *Enum_Ref_Par = Ident_3; break;
    }
}

static void Proc_7(One_Fifty Int_1_Par_Val, One_Fifty Int_2_Par_Val,
                   One_Fifty *Int_Par_Ref)
{
    One_Fifty Int_Loc;
    Int_Loc       = Int_1_Par_Val + 2;
    *Int_Par_Ref  = Int_2_Par_Val + Int_Loc;
}

static void Proc_8(volatile int *Arr_1_Par_Ref,
                   volatile int (__far *Arr_2_Par_Ref)[50],
                   int Int_1_Par_Val, int Int_2_Par_Val)
{
    One_Fifty Int_Index;
    One_Fifty Int_Loc;
    Int_Loc = Int_1_Par_Val + 5;
    Arr_1_Par_Ref[Int_Loc] = Int_2_Par_Val;
    Arr_1_Par_Ref[Int_Loc + 1] = Arr_1_Par_Ref[Int_Loc];
    Arr_1_Par_Ref[Int_Loc + 30] = Int_Loc;
    for (Int_Index = Int_Loc; Int_Index <= Int_Loc + 1; ++Int_Index) {
        Arr_2_Par_Ref[Int_Loc][Int_Index] = Int_Loc;
    }
    Arr_2_Par_Ref[Int_Loc][Int_Loc - 1] += 1;
    Arr_2_Par_Ref[Int_Loc + 20][Int_Loc] = Arr_1_Par_Ref[Int_Loc];
    Int_Glob = 5;
}

static Enumeration Func_1(Capital_Letter Ch_1_Par_Val,
                          Capital_Letter Ch_2_Par_Val)
{
    Capital_Letter Ch_1_Loc;
    Capital_Letter Ch_2_Loc;
    Ch_1_Loc = Ch_1_Par_Val;
    Ch_2_Loc = Ch_1_Loc;
    if (Ch_2_Loc != Ch_2_Par_Val) return Ident_1;
    return Ident_2;
}

static Boolean Func_2(Str_30 Str_1_Par_Ref, Str_30 Str_2_Par_Ref)
{
    One_Thirty     Int_Loc;
    Capital_Letter Ch_Loc = 'A';  /* init silences Watcom -w3 */
    Int_Loc = 2;
    while (Int_Loc <= 2) {
        if (Func_1(Str_1_Par_Ref[Int_Loc], Str_2_Par_Ref[Int_Loc + 1]) == Ident_1) {
            Ch_Loc = 'A';
            Int_Loc += 1;
        }
    }
    if (Ch_Loc >= 'W' && Ch_Loc < 'Z') Int_Loc = 7;
    if (Ch_Loc == 'R') return 1;
    if (strcmp(Str_1_Par_Ref, Str_2_Par_Ref) > 0) {
        Int_Loc += 7;
        Int_Glob = Int_Loc;
        return 1;
    }
    return 0;
}

static Boolean Func_3(Enumeration Enum_Par_Val)
{
    Enumeration Enum_Loc;
    Enum_Loc = Enum_Par_Val;
    if (Enum_Loc == Ident_3) return 1;
    return 0;
}

/* ------------------------------------------------------------------- */
/* Main loop — one "Dhrystone" iteration                                */
/* ------------------------------------------------------------------- */

static void run_dhrystone_iterations(unsigned long iters)
{
    int         Int_1_Loc, Int_2_Loc, Int_3_Loc;
    char        Ch_Index;
    Enumeration Enum_Loc;
    Str_30      Str_1_Loc;
    Str_30      Str_2_Loc;
    unsigned long Number_Of_Runs;

    /* Initialization — mirror of the reference's main() setup */
    Next_Ptr_Glob = &Rec_1_Storage;
    Ptr_Glob      = &Rec_2_Storage;
    Ptr_Glob->Ptr_Comp                       = Next_Ptr_Glob;
    Ptr_Glob->Discr                          = Ident_1;
    Ptr_Glob->variant.var_1.Enum_Comp        = Ident_3;
    Ptr_Glob->variant.var_1.Int_Comp         = 40;
    strcpy(Ptr_Glob->variant.var_1.Str_Comp, "DHRYSTONE PROGRAM, SOME STRING");
    strcpy(Str_1_Loc, "DHRYSTONE PROGRAM, 1'ST STRING");
    Arr_2_Glob[8][7] = 10;

    /* Benchmark loop */
    for (Number_Of_Runs = 0; Number_Of_Runs < iters; Number_Of_Runs++) {
        Proc_5();
        Proc_4();
        Int_1_Loc = 2;
        Int_2_Loc = 3;
        strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 2'ND STRING");
        Enum_Loc = Ident_2;
        Bool_Glob = !Func_2(Str_1_Loc, Str_2_Loc);
        while (Int_1_Loc < Int_2_Loc) {
            Int_3_Loc = 5 * Int_1_Loc - Int_2_Loc;
            Proc_7(Int_1_Loc, Int_2_Loc, &Int_3_Loc);
            Int_1_Loc += 1;
        }
        Proc_8(Arr_1_Glob, Arr_2_Glob, Int_1_Loc, Int_3_Loc);
        Proc_1(Ptr_Glob);
        for (Ch_Index = 'A'; Ch_Index <= Ch_2_Glob; ++Ch_Index) {
            if (Enum_Loc == Func_1(Ch_Index, 'C')) {
                Proc_6(Ident_1, &Enum_Loc);
                strcpy(Str_2_Loc, "DHRYSTONE PROGRAM, 3'RD STRING");
                Int_2_Loc = (int)Number_Of_Runs;
                Int_Glob  = (int)Number_Of_Runs;
            }
        }
        Int_2_Loc = Int_2_Loc * Int_1_Loc;
        Int_1_Loc = Int_2_Loc / Int_3_Loc;
        Int_2_Loc = 7 * (Int_2_Loc - Int_3_Loc) - Int_1_Loc;
        Proc_2(&Int_1_Loc);
    }
}

/* ------------------------------------------------------------------- */
/* CERBERUS entry point                                                 */
/* ------------------------------------------------------------------- */

/* Target runtime for the main run: ~5 seconds on a 486 DX-2.
 * Warmup estimates Dhrystones/second via a short run, then scales. */
#define WARMUP_ITERS       2000UL
#define MIN_MAIN_ITERS     2000UL
#define MAX_MAIN_ITERS     2000000UL
#define TARGET_MAIN_US     5000000UL   /* 5 seconds in us */

void bench_dhrystone(result_table_t *t, const opts_t *o)
{
    us_t warmup_us, main_us;
    unsigned long main_iters;
    unsigned long dhry_per_sec;
    (void)o;

    /* Warmup. If warmup_us is 0 we're running faster than PIT resolution
     * (unlikely, but emulator artifact) — fall back to a fixed count. */
    timing_start();
    run_dhrystone_iterations(WARMUP_ITERS);
    warmup_us = timing_stop();

    if (warmup_us == 0) {
        main_iters = 50000UL;
    } else {
        /* main_iters = WARMUP_ITERS * TARGET_MAIN_US / warmup_us */
        main_iters = (WARMUP_ITERS * TARGET_MAIN_US) / (unsigned long)warmup_us;
        if (main_iters < MIN_MAIN_ITERS) main_iters = MIN_MAIN_ITERS;
        if (main_iters > MAX_MAIN_ITERS) main_iters = MAX_MAIN_ITERS;
    }

    /* Real run */
    timing_start();
    run_dhrystone_iterations(main_iters);
    main_us = timing_stop();

    /* Anti-DCE observer — consume final Dhrystone state so Watcom -ox
     * cannot eliminate the writes that produced it. Emitting the
     * checksum via report_add_u32 (defined in an external translation
     * unit: report.c) forms the barrier — Watcom cannot prove the
     * checksum unused, so every read feeding it must be preserved,
     * which cascades backward through the assignments in the main loop.
     * Ptr_Glob struct-member reads force the chain of writes through
     * Ptr_Val_Par / Next_Record in Proc_1 to be preserved.
     *
     * Anti-DCE row is not for consistency-engine use; it is a compiler
     * barrier. Do not remove or inline. Do not filter from the INI. */
    {
        unsigned long checksum = 0UL;
        unsigned int  k_cs;
        checksum ^= (unsigned long)(unsigned int)Int_Glob;
        checksum ^= (unsigned long)(unsigned int)Bool_Glob;
        checksum ^= (unsigned long)(unsigned char)Ch_1_Glob;
        checksum ^= (unsigned long)(unsigned char)Ch_2_Glob;
        for (k_cs = 0; k_cs < 50; k_cs++) {
            checksum ^= (unsigned long)(unsigned int)Arr_1_Glob[k_cs];
            /* Sample one cell per row from Arr_2_Glob — full 2500-cell
             * XOR would dominate reported runtime via __far access cost. */
            checksum ^= (unsigned long)(unsigned int)Arr_2_Glob[k_cs][k_cs];
        }
        if (Ptr_Glob) {
            checksum ^= (unsigned long)(unsigned int)Ptr_Glob->variant.var_1.Int_Comp;
            checksum ^= (unsigned long)(unsigned int)Ptr_Glob->variant.var_1.Enum_Comp;
            checksum ^= (unsigned long)(unsigned char)Ptr_Glob->variant.var_1.Str_Comp[0];
            checksum ^= (unsigned long)(unsigned char)Ptr_Glob->variant.var_1.Str_Comp[29];
        }
        report_add_u32(t, "bench.cpu.dhrystones_checksum",
                       checksum, (const char *)0,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }

    /* Emit raw values */
    sprintf(bench_dhry_elapsed_val, "%lu", (unsigned long)main_us);
    report_add_u32(t, "bench.cpu.dhrystones_elapsed_us",
                   (unsigned long)main_us, bench_dhry_elapsed_val,
                   CONF_HIGH, VERDICT_UNKNOWN);

    sprintf(bench_dhry_iters_val, "%lu", main_iters);
    report_add_u32(t, "bench.cpu.dhrystones_iterations",
                   main_iters, bench_dhry_iters_val,
                   CONF_HIGH, VERDICT_UNKNOWN);

    if (main_us > 0) {
        /* dhry_per_sec = main_iters * 1,000,000 / main_us.
         * Guard against 32-bit overflow: main_iters * 1000000 fits in
         * unsigned long only when main_iters < 4294. For larger counts
         * compute via (main_iters / main_us) * 1000000 + residue. */
        if (main_iters < 4294UL) {
            dhry_per_sec = (main_iters * 1000000UL) / (unsigned long)main_us;
        } else {
            /* Rearranged for 32-bit safety: 1e6 / (us / iters) */
            unsigned long us_per_iter_x1000 =
                ((unsigned long)main_us * 1000UL) / main_iters;
            if (us_per_iter_x1000 > 0) {
                dhry_per_sec = 1000000000UL / us_per_iter_x1000;
            } else {
                dhry_per_sec = 0;
            }
        }
        sprintf(bench_dhry_per_sec_val, "%lu", dhry_per_sec);
        report_add_u32(t, "bench.cpu.dhrystones",
                       dhry_per_sec, bench_dhry_per_sec_val,
                       CONF_HIGH, VERDICT_UNKNOWN);
    } else {
        report_add_str(t, "bench.cpu.dhrystones",
                       "inconclusive (elapsed=0)",
                       CONF_LOW, VERDICT_WARN);
    }
}
