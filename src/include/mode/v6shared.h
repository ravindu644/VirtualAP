/* The upstream's own /64 shared with the clients, what Android's tethering
 * does: ap0 advertises it off-link, every client address is routed as a /128
 * to ap0 and answered for on the upstream with an NDP proxy entry. No NAT,
 * so no ip6table_nat, which stock GKI kernels lack.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_V6SHARED_H
#define VAP_V6SHARED_H

#include "mode/ipv6.h"

/* v->prefix and net: upaddr's /64. */
void shared_prefix(const struct in6_addr *upaddr, struct v6 *v);
/* 0, or -1 with the reason in why. */
int shared_setup(struct nl *nl, const struct upstream *up,
                 const struct in6_addr *upaddr, struct v6 *v,
                 const char **why);
/* A client address found on ap0: its /128 to ap0 and its proxy entry on the
 * upstream, recorded in run/v6clients for the teardown when record (a
 * re-install of a known one is not). */
int shared_add_client(struct nl *nl, const char *addr, const char *up,
                      bool record);

#define V6CLIENTS VAP_RUN "/v6clients"

#endif
