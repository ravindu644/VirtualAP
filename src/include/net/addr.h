/* Interface addresses.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_ADDR_H
#define VAP_ADDR_H

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>

#include "net/nl.h"

/* Add (or refresh) addr/prefix on ifname. nodad skips duplicate address
 * detection, for our own router address on a link we own: waiting it out
 * would leave the address unusable for the first second. 0 or -errno. */
int addr_add(struct nl *nl, const char *ifname, int family, const char *addr,
             int prefix, bool nodad);

struct addr_info {
  int ifindex;
  int family;
  int prefix;
  int scope;      /* RT_SCOPE_UNIVERSE for global */
  uint32_t flags; /* IFA_F_DEPRECATED, IFA_F_TENTATIVE... */
  char addr[INET6_ADDRSTRLEN];
};
/* Every address of family (AF_UNSPEC: both) on ifindex (0: every interface),
 * in the kernel's order, which puts an interface's primary IPv4 address first.
 * Returns how many were stored in out, or -errno. */
int addr_list(struct nl *nl, int family, int ifindex, struct addr_info *out,
              int max);

/* Remove the addresses of family (AF_UNSPEC: all) on ifname, only the global
 * ones when global_only. 0 or -errno. */
int addr_flush(struct nl *nl, const char *ifname, int family, bool global_only);

#endif
