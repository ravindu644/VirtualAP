/* Managed mode: ap0 is a port on a bridge whose other port is a veth into a
 * Droidspaces container, which owns all L3 (DHCP, DNS, NAT).
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MANAGED_H
#define VAP_MANAGED_H

#include <sys/types.h>

#include "cli/options.h"
#include "core/ipaddr.h"

/* The whole managed start after the settings were validated and saved, lan
 * the subnet of the gateway start checked. 0, or -1 after logging why and
 * tearing down what was set up. */
int managed_start(const struct vap_opts *o, const struct subnet *lan);
/* The container restarted as pid: its network namespace, and with it the
 * veth (both ends), went away, so a new one is made in the new namespace.
 * Its OpenWrt LAN config survived the restart and picks the port up again. */
int managed_replug(const struct vap_opts *o, pid_t pid);
/* Undo a managed AP, tolerant of what is already gone. Deleting the veth
 * takes the container's end with it; its OpenWrt LAN config stays. */
void managed_teardown(const char *container);

#endif
