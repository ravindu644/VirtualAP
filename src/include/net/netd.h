/* Which network the hotspot's traffic leaves through, as Android's netd sees
 * it.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NETD_H
#define VAP_NETD_H

#include <net/if.h>
#include <stdbool.h>

#include "net/nl.h"

/* netd's default-network table, from its rule "from all fwmark 0x0/0xffff
 * iif lo lookup <table>", the lowest priority when several. 0 or -ENOENT
 * (airplane mode, or not Android). */
int netd_default_table(struct nl *nl, int family, int *table);

struct upstream {
  int table;
  char iface[IF_NAMESIZE]; /* empty when auto found a table with no default */
};
/* want is "auto" or an interface name. With verbose, logs what it found and
 * why when it found nothing; -1 then. The monitor asks quietly. */
int upstream_resolve(struct nl *nl, const char *want, struct upstream *up,
                     bool verbose);

/* The interface IPv6 leaves through: on an IPv6-only network the IPv4
 * upstream is the CLAT interface v4-<iface>, which carries no IPv6. */
const char *upstream_v6_iface(const struct upstream *up);

#endif
