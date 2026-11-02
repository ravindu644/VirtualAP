/* IPv6 for the clients in routed mode: NAT66 on a private prefix where the
 * kernel can masquerade IPv6, else a share of the upstream's own /64, else
 * none. Never fatal: on any problem the AP stays IPv4 only.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_IPV6_H
#define VAP_IPV6_H

#include <arpa/inet.h>
#include <net/if.h>

#include "cli/cli.h"
#include "core/core.h"
#include "net/netd.h"

enum { V6_OFF, V6_NAT66, V6_SHARED };

struct v6 {
  int mode;
  char net[INET6_ADDRSTRLEN + 8]; /* "x::/64" */
  char prefix[INET6_ADDRSTRLEN];  /* "x::", what dnsmasq advertises */
  char gw[INET6_ADDRSTRLEN + 2];  /* ap0's own address in it */
  char up[IF_NAMESIZE];           /* the IPv6 upstream interface */
  char prev_fwd[8], prev_proxy[8];
};

/* What the clients should get now, touching nothing: v->mode, v->up and the
 * prefix filled, upaddr the upstream's global address. Returns why IPv6 is
 * off when it is (NULL when -6 0 asked for that). */
const char *ipv6_decide(struct nl *nl, const struct vap_opts *o,
                        const struct upstream *up, const struct subnet *net,
                        struct v6 *v, struct in6_addr *upaddr);
void ipv6_setup(struct nl *nl, const struct vap_opts *o,
                const struct upstream *up, const struct subnet *net,
                struct v6 *v);
void ipv6_save(const struct v6 *v, struct state *st);
/* Undo whatever st says was set up, tolerant of half a setup, and of a
 * run.state with no IPv6 keys at all (v2.1.0's shell engine had none). */
void ipv6_teardown(struct nl *nl, const struct state *st);

/* The first address on iface the internet routes back to: global scope, not
 * ULA (fc00::/7), neither deprecated nor tentative; a /64 only when only64.
 * Wi-Fi routers without IPv6 internet hand out ULAs alone. */
bool v6_global_addr(struct nl *nl, const char *iface, bool only64,
                    struct in6_addr *out);

/* The IPv6 upstream's accept_ra at 2, which it needs once forwarding is on
 * to go on taking RAs, and with them its address and default route. */
void ipv6_keep_ra(const char *iface);

/* /proc/sys/net/ipv6/conf/<iface>/<key>. */
int conf6_set(const char *iface, const char *key, const char *val);
void conf6_get(const char *iface, const char *key, char *out, size_t size);

#endif
