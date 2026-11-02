/* Network interfaces: create, delete, configure.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_LINK_H
#define VAP_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "net/nl.h"

/* Every call returns 0 or -errno, as the kernel answered. */
int link_set_up(struct nl *nl, const char *ifname, bool up);
/* 0 also when it was already gone. */
int link_del(struct nl *nl, const char *ifname);
/* master NULL releases the link from its bridge. */
int link_set_master(struct nl *nl, const char *ifname, const char *master);
int link_add_bridge(struct nl *nl, const char *name);
/* A veth pair, both ends at mtu (0 = the default). With netns_fd >= 0 the peer
 * is born inside that namespace, already under its final name and with
 * peer_mac if given: a link created on the host and moved in afterwards shows
 * up there under its temporary name first on kernels before 5.x, and
 * OpenWrt's netifd does not look at it again after the rename. */
int link_add_veth(struct nl *nl, const char *host, const char *peer,
                  int netns_fd, const uint8_t *peer_mac, unsigned mtu);
/* The kernel's permanent address (IFLA_PERM_ADDRESS, 5.5+) as aa:bb:..,
 * -ENOENT when the driver reports none. */
int link_permaddr(struct nl *nl, const char *ifname, char out[18]);

#endif
