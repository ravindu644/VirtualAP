/* Address arithmetic the engine needs, nothing more.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_IPADDR_H
#define VAP_IPADDR_H

#include <stdbool.h>

/* A dotted IPv4 address as inet_pton reads it: four octets of at most three
 * digits, no leading zero. */
bool valid_ip4(const char *s);

/* The AP's /24, derived from its gateway: the last octet is the host part. */
struct subnet {
  char gw[16];    /* 192.168.42.1 */
  char net[20];   /* 192.168.42.0/24 */
  char begin[16]; /* .10, the DHCP pool */
  char end[16];   /* .50 */
};
/* false when gw is not a host address (.0 and .255 refused). */
bool subnet_from_gw(const char *gw, struct subnet *out);

#endif
