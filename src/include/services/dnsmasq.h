/* dnsmasq: DHCP and DNS for the clients, and the IPv6 router advertisements.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_DNSMASQ_H
#define VAP_DNSMASQ_H

#include <stdbool.h>

#include "core/ipaddr.h"
#include "virtualap.h"

#define DNSMASQ_CONF VAP_RUN "/dnsmasq.conf"
#define DNSMASQ_PID VAP_RUN "/dnsmasq.pid"

/* The router advertisement for the clients' IPv6, NULL for IPv4 only. */
struct ra {
  const char *prefix; /* "x:y:z:w::", the /64 advertised */
  const char *dns;    /* our own address in it, the RDNSS */
  bool off_link;      /* shared /64: clients send everything to us */
};

/* dns is the comma separated upstream list, blanks and empties dropped.
 * 0 or -1. */
int dnsmasq_write_conf(const char *path, const struct subnet *net,
                       const char *dns, const struct ra *ra);
/* Write the conf and start dnsmasq. Its parent exits only once the daemon has
 * written its pid file and is serving, so a zero exit is the liveness check.
 * 0, or -1 after logging why. */
int dnsmasq_start(const struct subnet *net, const char *dns,
                  const struct ra *ra);
/* True when dns holds at least one address. */
bool dns_any(const char *dns);

#endif
