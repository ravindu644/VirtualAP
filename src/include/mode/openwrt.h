/* An OpenWrt container's LAN for the hotspot, provisioned automatically.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_OPENWRT_H
#define VAP_OPENWRT_H

#include "core/ipaddr.h"

/* A LAN on the container's VAP_PORT at the gateway's address, its DHCP and
 * router advertisements, and NAT out of its WAN. Best effort: what fails is
 * logged as a warning. */
void openwrt_provision(const char *container, const struct subnet *net);

#endif
