/*
 * Cache detection — Phase 1 Task 1.4 (minimum viable).
 *
 * Honest minimum per plan §4: report what we know HIGH-confidence from
 * the CPU class, defer anything requiring synthetic timing to a follow-
 * up that integrates with the Phase 3 bench infrastructure.
 *
 * What we report now:
 *   cache.present     yes if CPU class >= 486 (all 486DX/SX/DX2/DX4 and
 *                     every later x86 have on-die L1), else no. HIGH
 *                     confidence from the class probe.
 *   cache.probe       "class-inference"  (records the method we used)
 *
 * What's deferred:
 *   - L1 size and line-size inference via stride timing. Needs the Phase
 *     3 bench timing infrastructure; DOSBox-X timing for cache is
 *     synthetic per plan Task 1.4 so LOW confidence is the only honest
 *     outcome under emulation.
 *   - CPUID leaf 2 descriptor decode for Pentium+. Leaf 2 isn't defined
 *     on 486, and the target hardware for v0.2 skews toward 486-era,
 *     so this is a Phase 3/4 expansion rather than Phase 1 scope.
 *   - L2 cache detection — typically a motherboard-mounted chipset
 *     probe, covered under Task 1.5 (bus detection) for the chipset
 *     identity side and a Phase 4 consistency cross-check for size.
 *
 * The plan's rule 8 already flags cache.l1_size as an advisory rule
 * pending a real probe; Phase 1 doesn't promise more than it delivers.
 */

#include "detect.h"
#include "cpu.h"
#include "env.h"
#include "../core/report.h"

void detect_cache(result_table_t *t)
{
    cpu_class_t cls = cpu_get_class();
    int has_l1 = (cls == CPU_CLASS_486_NOCPUID) || (cls == CPU_CLASS_CPUID);

    report_add_str(t, "cache.present",
                   has_l1 ? "yes" : "no",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);
    report_add_str(t, "cache.probe", "class-inference",
                   env_clamp(CONF_HIGH), VERDICT_UNKNOWN);

    /* Explicit unknown placeholders for the values we don't probe yet —
     * the consistency engine's advisory rule 8 expects to read these. */
    report_add_str(t, "cache.l1_size_kb", "unknown",
                   env_clamp(CONF_LOW), VERDICT_UNKNOWN);
    report_add_str(t, "cache.line_size_bytes", "unknown",
                   env_clamp(CONF_LOW), VERDICT_UNKNOWN);
}
