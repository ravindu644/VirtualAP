/* What net/'s own files share, and the tests check: not part of its
 * public header. Included as "internal.h", or by path from tests/.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NET_INTERNAL_H
#define VAP_NET_INTERNAL_H

#include "net/net.h"

/* nl.c: rtnetlink transport: the socket, requests, dumps and attribute
 * helpers that link, addr, route, rule and neigh build their messages with. */

/* The raw bytes after the current end, for VETH_INFO_PEER's embedded
 * ifinfomsg. NULL when it does not fit. */
void *nl_reserve(struct nl_req *r, size_t len);

/* route.c: Routes and the routing-table lookups the upstream is resolved
 * with. */

/* route.c: Which network the hotspot's traffic leaves through, as Android's
 * netd sees it. */

/* netd's default-network table, from its rule "from all fwmark 0x0/0xffff
 * iif lo lookup <table>", the lowest priority when several. 0 or -ENOENT
 * (airplane mode, or not Android). */
int netd_default_table(struct nl *nl, int family, int *table);

#endif
