/* The firewall rules the AP needs, raw socket first and the iptables binary
 * for whatever the raw path cannot reach.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_IPT_RULES_H
#define VAP_IPT_RULES_H

#include <stdbool.h>

/* filter FORWARD -i iface -j ACCEPT and -o iface -j ACCEPT, at the head of
 * the chain, ahead of netd's own. 0 or -1. Removing what is not there is 0. */
int ipt_forward_accept(int family, const char *iface, bool add);
/* nat POSTROUTING -s cidr ! -d cidr -j MASQUERADE, interface-less so it
 * survives an upstream change. 0 or -1. */
int ipt_masquerade(int family, const char *cidr, bool add);
/* /system/bin/iptables or ip6tables -w -t table args..., args ending in
 * NULL. -w waits for netd's xtables lock instead of failing while netd holds
 * it. Its exit code. */
int ipt_bin(int family, const char *table, const char *const args[]);

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
/* Our rules as the kernel stores them; buf holds 256 bytes. Size or -1. */
int ipt_build_accept(int family, const struct ipt_iface_arg *a,
                     unsigned char *buf);
int ipt_build_masq(int family, const struct ipt_net_arg *a, unsigned char *buf);
int ipt_net_parse(int family, const char *cidr, struct ipt_net_arg *out);

#endif
