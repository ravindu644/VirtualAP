/* What net/ipt/'s own files share, and the tests check: not part of its
 * public header. Included as "internal.h", or by path from tests/.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NET_IPT_INTERNAL_H
#define VAP_NET_IPT_INTERNAL_H

#include <linux/netfilter_ipv4/ip_tables.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "net/ipt/ipt.h"

/* ipt.c: The raw ip_tables / ip6_tables engine: read a table, insert one
 * rule at the head of a chain, remove the rules a matcher picks, all through
 * the kernel's IPT_SO_* socket options. The two families share the machine
 * and differ in the rule header, which ipt_view hides.
 *
 * Android safety contract: never flush a chain, never change a policy, never
 * touch a rule the matcher did not pick, insert only rules scoped to us. */

/* The match fields of one rule entry, whichever family. */
struct ipt_view {
  char *iniface, *outiface;
  unsigned char *in_mask, *out_mask;
  void *src, *smsk, *dst, *dmsk;
  uint16_t *proto;
  uint8_t *flags, *invflags;
};
void ipt_view(int family, void *entry, struct ipt_view *v);
size_t ipt_addr_len(int family);
/* The aligned size of the family's rule header, where matches or the target
 * start. */
size_t ipt_entry_size(int family);
/* A rule entry's target, NULL when the entry is malformed. */
const struct xt_entry_target *ipt_target(int family, const unsigned char *e);

/* buf zeroed and laid out for a rule with no matches and a target of
 * target_sz bytes; returns the target. buf holds ipt_entry_size + target_sz. */
struct xt_entry_target *ipt_rule_begin(int family, unsigned char *buf,
                                       size_t target_sz);

typedef bool (*ipt_match_fn)(int family, const unsigned char *entry,
                             const void *arg);

/* A table as the kernel hands it out: its info and its entries. */
struct ipt_table {
  struct ipt_getinfo info;
  unsigned char *blob; /* info.size bytes */
};

/* The pure halves of insert and remove, so the offset arithmetic is tested
 * without a kernel. Both fill out with a new info and a malloc'd blob. */
int ipt_blob_insert(int family, const struct ipt_table *in, unsigned hook,
                    const void *rule, unsigned rule_sz, struct ipt_table *out);
/* Returns how many entries matched in hook's chain (0: out untouched), or
 * -errno. Policy entries are never removed. */
int ipt_blob_remove(int family, const struct ipt_table *in, unsigned hook,
                    ipt_match_fn match, const void *arg, struct ipt_table *out);
bool ipt_blob_find(int family, const struct ipt_table *t, unsigned hook,
                   ipt_match_fn match, const void *arg);

/* The kernel side. fd from ipt_open; every call returns -errno on failure,
 * and retries on EAGAIN, which netd changing the same table at the same
 * moment causes. Inserts and removes hold Android's xtables lock across their
 * read and replace, as netd and `iptables -w` do. */
int ipt_open(int family);
int ipt_get(int fd, int family, const char *table, struct ipt_table *t);
void ipt_free(struct ipt_table *t);
/* Inserts rule at the head of hook's chain unless match finds it there, in
 * the same snapshot. 0 when it is in place. */
int ipt_insert(int fd, int family, const char *table, unsigned hook,
               const void *rule, unsigned rule_sz, ipt_match_fn match,
               const void *arg);
/* How many it removed, or -errno. */
int ipt_remove(int fd, int family, const char *table, unsigned hook,
               ipt_match_fn match, const void *arg);
/* 1, 0, or -errno when the table cannot be read. */
int ipt_find(int fd, int family, const char *table, unsigned hook,
             ipt_match_fn match, const void *arg);
/* Does the kernel know this target (MASQUERADE in nat, TTL in mangle)? */
bool ipt_target_known(int fd, int family, const char *name);

/* ipt_rules.c: The firewall rules the AP needs, raw socket first and the
 * iptables binary for whatever the raw path cannot reach. */

/* Our rules as the kernel stores them; buf holds 256 bytes. Size or -1. */
int ipt_build_accept(int family, const struct ipt_iface_arg *a,
                     unsigned char *buf);
int ipt_build_masq(int family, const struct ipt_net_arg *a, unsigned char *buf);
int ipt_net_parse(int family, const char *cidr, struct ipt_net_arg *out);

#endif
