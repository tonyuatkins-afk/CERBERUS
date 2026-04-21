#ifndef CERBERUS_NETWORK_H
#define CERBERUS_NETWORK_H

#include "../cerberus.h"

/* Runs the transport probe ladder (netisa → pktdrv → mtcp → wattcp →
 * none) and emits network.transport into the result table. Call once
 * during detect_all. */
void        detect_network(result_table_t *t);

/* Read the previously-emitted transport row. Returns 1 if transport
 * is anything other than "none"; 0 otherwise. */
int         network_is_online(const result_table_t *t);

/* Returns the current transport string. Guaranteed non-NULL. Returns
 * "none" if detect_network has not yet run. */
const char *network_transport_str(const result_table_t *t);

#endif
