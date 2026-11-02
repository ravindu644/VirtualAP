/* Routes and the routing-table lookups the upstream is resolved with.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_ROUTE_H
#define VAP_ROUTE_H

#include <net/if.h>

#include "net/nl.h"

struct route {
  int family;
  const char *dst; /* "a.b.c.d/len" or "x::/len"; NULL for the default route */
  const char *dev; /* the outgoing interface */
  const char *src; /* preferred source address, NULL for none */
  int table;
  int proto; /* RTPROT_*, 0 for RTPROT_BOOT as ip(8) uses */
  int scope; /* RT_SCOPE_LINK for a connected route */
};

/* `ip route replace`: adds or overwrites, so a restart never stacks
 * duplicates. 0 or -errno. */
int route_replace(struct nl *nl, const struct route *rt);
/* 0 also when there was no such route. */
int route_del(struct nl *nl, const struct route *rt);

/* The interface of the default route in table, the lowest metric when
 * several, the tie-break the kernel applies. On an IPv6-only mobile network
 * the IPv4 default in the default network's table is the CLAT interface
 * (v4-rmnet_dataX), which is what IPv4 forwarding needs. 0 or -ENOENT. */
int route_default_oif(struct nl *nl, int family, int table,
                      char ifname[IF_NAMESIZE]);
/* The table carrying ifname's routes: the one with the shortest prefix out
 * of it, so a default route wins, then a split-tunnel VPN's 0.0.0.0/5 and
 * friends, then whatever subnet an interface with neither reaches (a
 * corporate VPN's 10.0.0.0/16, USB Ethernet's own LAN). The local table's
 * own-address routes do not count. 0, -ENODEV or -ENOENT. */
int route_iface_table(struct nl *nl, int family, const char *ifname,
                      int *table);

#endif
