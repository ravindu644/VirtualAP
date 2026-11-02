/* Managed mode: ap0 is a port on a bridge whose other port is a veth into a
 * Droidspaces container, which owns all L3 (DHCP, DNS, NAT).
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MANAGED_H
#define VAP_MANAGED_H

#include <stdbool.h>
#include <sys/types.h>

#include "cli/cli.h"
#include "core/core.h"

/* The whole managed start after the settings were validated and saved, lan
 * the subnet of the gateway start checked. 0, or -1 after logging why and
 * tearing down what was set up. */
int managed_start(const struct vap_opts *o, const struct subnet *lan);
/* Whether the port is still in the container: the host end is there and the
 * process it was plugged into is still in that namespace. */
bool managed_plugged(void);
/* The container restarted as pid: a new veth goes into its namespace,
 * replacing a host end left in the old one. Its OpenWrt LAN config survived
 * the restart and picks the port up again. */
int managed_replug(const struct vap_opts *o, pid_t pid);
/* Undo a managed AP, tolerant of what is already gone. Deleting the veth
 * takes the container's end with it; its OpenWrt LAN config stays. */
void managed_teardown(const char *container);

#endif
