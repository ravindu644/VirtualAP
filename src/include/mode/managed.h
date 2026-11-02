/* Managed mode: ap0 is a port on a bridge whose other port is a veth into a
 * Droidspaces container, which owns all L3 (DHCP, DNS, NAT).
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MANAGED_H
#define VAP_MANAGED_H

/* Undo a managed AP, tolerant of what is already gone. Deleting the veth
 * takes the container's end with it; its OpenWrt LAN config stays. */
void managed_teardown(const char *container);

#endif
