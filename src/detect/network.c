/*
 * Network transport detection — v0.7.0 T1.
 *
 * Detects what network transport (if any) is available at startup.
 * Emits a [network] transport=<value> row into the result table so
 * the INI captures it and the upload orchestrator can decide whether
 * to prompt the user.
 *
 * Priority order (first hit wins):
 *
 *   1. NetISA via INT 63h. Project-specific custom card; reserved
 *      for v0.8.0 TLS path. Detected now so future runs on NetISA
 *      hardware show up labeled correctly.
 *
 *   2. Packet driver via INT 60h-7Fh scan for "PKT DRVR" signature.
 *      Standard Crynwr packet-driver interface. Present on most
 *      DOS-networking machines from the 90s.
 *
 *   3. mTCP via MTCP_CFG environment variable. Brutman's modern
 *      TCP/IP stack; if env var is set we assume mTCP is usable.
 *
 *   4. WATTCP via WATTCP.CFG or WATTCP environment variable. Older
 *      DOS TCP/IP stack still in use on many systems.
 *
 *   5. none — offline. No error, no nag. CERBERUS just runs without
 *      upload.
 *
 * Safety: every probe is non-destructive. We never WRITE to interrupt
 * vectors or CALL detected handlers — only read signatures. Reading a
 * far pointer at a vector that happens to point into RAM can't fault
 * on 8088 (real mode), so the packet-driver scan is safe on every
 * supported CPU.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include "detect.h"
#include "network.h"
#include "../core/report.h"

/* ----------------------------------------------------------------------- */
/* Packet driver scan                                                       */
/* ----------------------------------------------------------------------- */

/* The Crynwr packet-driver spec says: a driver installed at INT <N>
 * places the signature "PKT DRVR" (8 bytes, with spaces) starting at
 * offset 3 from the interrupt handler's entry point. The first 3 bytes
 * are a near JMP over the signature to the actual dispatch code. */
static int pktdrv_check_vector(int vec)
{
    unsigned long __far *ivt = (unsigned long __far *)MK_FP(0x0000, 0x0000);
    unsigned long seg_off = ivt[vec];
    unsigned int seg = (unsigned int)(seg_off >> 16);
    unsigned int off = (unsigned int)(seg_off & 0xFFFFUL);
    unsigned char __far *handler;
    const char *sig = "PKT DRVR";
    int i;

    /* Null vector → no driver. BIOS also leaves vectors pointing into
     * the BIOS ROM; those won't match the signature either. */
    if (seg == 0 && off == 0) return 0;

    handler = (unsigned char __far *)MK_FP(seg, off);
    for (i = 0; i < 8; i++) {
        if (handler[3 + i] != (unsigned char)sig[i]) return 0;
    }
    return 1;
}

static int pktdrv_scan(int *out_vector)
{
    int v;
    for (v = 0x60; v <= 0x7F; v++) {
        if (pktdrv_check_vector(v)) {
            if (out_vector) *out_vector = v;
            return 1;
        }
    }
    return 0;
}

/* ----------------------------------------------------------------------- */
/* NetISA INT 63h probe                                                     */
/* ----------------------------------------------------------------------- */

/* Placeholder: NetISA's custom API is project-specific (Tony's own
 * card). For v0.7.0 detection we look at vector 63h — if it points
 * somewhere non-trivial (not null, not into BIOS ROM-as-unused), we
 * assume a NetISA driver is resident. Not a strong signature; any
 * later v0.8.0 TLS-capable NetISA driver should publish a proper
 * signature we can match here. */
static int netisa_probe(void)
{
    unsigned long __far *ivt = (unsigned long __far *)MK_FP(0x0000, 0x0000);
    unsigned long seg_off = ivt[0x63];
    unsigned int seg = (unsigned int)(seg_off >> 16);
    unsigned int off = (unsigned int)(seg_off & 0xFFFFUL);
    /* A default (BIOS-installed) vector 63h typically points into the
     * BIOS IRET stub area. Without a proper NetISA signature to match,
     * we conservatively return 0 here. When NetISA's driver lands a
     * signature we'll swap this for a real check. */
    (void)seg; (void)off;
    return 0;
}

/* ----------------------------------------------------------------------- */
/* TCP-stack environment-variable probes                                    */
/* ----------------------------------------------------------------------- */

static int mtcp_probe(void)
{
    const char *env = getenv("MTCP_CFG");
    return (env && *env) ? 1 : 0;
}

static int wattcp_probe(void)
{
    const char *env = getenv("WATTCP");
    if (env && *env) return 1;
    env = getenv("WATTCP_CFG");
    return (env && *env) ? 1 : 0;
}

/* ----------------------------------------------------------------------- */
/* Public entry                                                             */
/* ----------------------------------------------------------------------- */

/* Storage for the transport string so report_add_str can keep its
 * lifetime valid. Report stores the pointer verbatim. */
static char transport_buf[16];

void detect_network(result_table_t *t)
{
    const char *transport = "none";
    int pkt_vec = 0;

    if (netisa_probe()) {
        transport = "netisa";
    } else if (pktdrv_scan(&pkt_vec)) {
        transport = "pktdrv";
    } else if (mtcp_probe()) {
        transport = "mtcp";
    } else if (wattcp_probe()) {
        transport = "wattcp";
    }

    strncpy(transport_buf, transport, sizeof(transport_buf) - 1);
    transport_buf[sizeof(transport_buf) - 1] = '\0';

    report_add_str(t, "network.transport", transport_buf,
                   CONF_HIGH, VERDICT_UNKNOWN);

    /* Diagnostic detail row: packet-driver vector if found. Lives under
     * the [network] section alongside the transport value. */
    if (strcmp(transport, "pktdrv") == 0 && pkt_vec != 0) {
        static char vec_buf[8];
        sprintf(vec_buf, "0x%02X", pkt_vec);
        report_add_str(t, "network.pktdrv_vector", vec_buf,
                       CONF_HIGH, VERDICT_UNKNOWN);
    }
}

/* Exposed predicate for the upload orchestrator. Reads the already-
 * emitted transport row rather than re-probing. */
int network_is_online(const result_table_t *t)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, "network.transport") == 0) {
            if (t->results[i].type == V_STR && t->results[i].v.s) {
                return strcmp(t->results[i].v.s, "none") != 0;
            }
            return 0;
        }
    }
    return 0;
}

const char *network_transport_str(const result_table_t *t)
{
    unsigned int i;
    for (i = 0; i < t->count; i++) {
        if (strcmp(t->results[i].key, "network.transport") == 0) {
            if (t->results[i].type == V_STR && t->results[i].v.s) {
                return t->results[i].v.s;
            }
        }
    }
    return "none";
}
