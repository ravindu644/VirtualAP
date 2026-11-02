/* The ip_tables engine and the rules built on it. Kept apart from net/net.h:
 * on musl the kernel's netfilter headers and libc's networking headers
 * cannot meet in one file.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NET_IPT_H
#define VAP_NET_IPT_H

#include <stdbool.h>

/* ipt_rules.c: The firewall rules the AP needs, raw socket first and the
 * iptables binary for whatever the raw path cannot reach. */

/* filter FORWARD -i iface -j ACCEPT and -o iface -j ACCEPT, at the head of
 * the chain, ahead of netd's own. 0 or -1. Removing what is not there is 0. */
int ipt_forward_accept(int family, const char *iface, bool add);
/* nat POSTROUTING -s cidr ! -d cidr -j MASQUERADE, interface-less so it
 * survives an upstream change. 0 or -1. */
int ipt_masquerade(int family, const char *cidr, bool add);
/* Both forward rules, and the MASQUERADE unless cidr is NULL (the shared /64
 * has none), in place: 1, 0 when any is missing,
 * -1 when the tables cannot be read (the rules then live wherever the binary
 * put them, so "unknown", never "missing"). Cheap, no fork: the monitor asks
 * after every netd change. */
int ipt_rules_present(int family, const char *iface, const char *cidr);
/* Can this kernel masquerade the family at all: the nat table and the
 * MASQUERADE target. For IPv6 that is CONFIG_IP6_NF_NAT, which stock GKI
 * kernels leave out. */
bool ipt_nat_available(int family);

/* The match functions, for the tests. arg is what the builders take. */
struct ipt_iface_arg {
  const char *iface;
  bool out;
};
struct ipt_net_arg {
  unsigned char net[16], mask[16];
};
bool ipt_match_accept(int family, const unsigned char *e, const void *arg);
bool ipt_match_masq(int family, const unsigned char *e, const void *arg);

/* ipt_rules.c: The TTL fix: forwarded packets leave with TTL (hop limit) 64,
 * so tethered traffic looks like the phone's own to the operator. */

/* Does the kernel take the TTL target? Asked by building the real rule in a
 * chain nothing jumps to, so no packet ever sees it; one test covers a kernel
 * without the target and an iptables without the extension. */
bool ttl_supported(void);
/* Our rules live in a chain of their own, vap_ttl, so clearing them never
 * depends on knowing which subnet they were written for. */
void ttl_clear(void);
/* src4 = the tethered IPv4 source subnet, src6 the IPv6 one or NULL. 0, or -1
 * when the TTL target is missing (everything undone). */
int ttl_apply(const char *src4, const char *src6);
/* For a new IPv6 source prefix (NULL: none) while the fix is on: the IPv6
 * half again with the IPv4 rule left in place, so an IPv6 change never leaves
 * IPv4 unfixed while iptables runs; everything again when the IPv4 rule is
 * gone. ttl_apply's 0 or -1. */
int ttl_refresh(const char *src4, const char *src6);

#endif
