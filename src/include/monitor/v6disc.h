/* Finding the clients' global addresses in a shared /64.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_V6DISC_H
#define VAP_V6DISC_H

#include "mode/ipv6.h"

/* The source is an echo to all-nodes from ap0's global address: every
 * client answers from its own global one, and nothing else reveals it. The
 * prefix is off-link, so a client solicits only the router's link-local and
 * ap0's neighbour table never sees the address. */

/* A raw ICMPv6 socket on ap0, from v's address, echo replies only. -1 when
 * it cannot be had. */
int v6disc_open(const struct v6 *v);
void v6disc_ping(int fd);
/* Every reply waiting: a new address in v's /64 gets its /128 and proxy
 * entry (shared_add_client). Routes and proxy entries stay for the AP's
 * lifetime: a /128 for a client that left only drops traffic to it, while
 * dropping the entry of a dozing client that missed a round would cut it
 * off. Only replies to our own echo id count, and never for an address that
 * is the phone's own or a neighbour on the upstream. */
void v6disc_read(int fd, struct nl *nl, const struct v6 *v);
/* Forget every address, for a new /64. */
void v6disc_reset(void);
/* Once a minute: drop the addresses that did not answer since the last
 * refresh (rotated privacy addresses, clients gone), and have the rest
 * re-installed on their next answer, since an ap0 that went down and up took
 * every /128 with it. */
void v6disc_refresh(void);

#endif
