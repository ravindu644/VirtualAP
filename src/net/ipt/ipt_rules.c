/* The rules are USB-Arsenal's raw_ensure_iface_accept and
 * raw_ensure_masquerade, the matchers its removal fingerprints.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "internal.h"
#include "net/ipt/ipt.h"

#include <linux/netfilter.h>
#include <linux/netfilter/nf_nat.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/core.h"

/* /system/bin/iptables or ip6tables -w -t table args..., args ending in
 * NULL. -w waits for netd's xtables lock instead of failing while netd holds
 * it. Its exit code. */
static int ipt_bin(int family, const char *table, const char *const args[]) {
  const char *argv[20] = {family == AF_INET6 ? "/system/bin/ip6tables"
                                             : "/system/bin/iptables",
                          "-w", "-t", table};
  int n = 4;
  for (int i = 0; args[i] && n < 19; i++)
    argv[n++] = args[i];
  argv[n] = NULL;
  return run_quiet(argv);
}

/* op chain spec... through the binary. */
static int bin_rule(int family, const char *table, const char *op,
                    const char *chain, const char *const spec[]) {
  const char *args[16] = {op, chain};
  int n = 2;
  for (int i = 0; spec[i] && n < 15; i++)
    args[n++] = spec[i];
  args[n] = NULL;
  return ipt_bin(family, table, args);
}

static bool all_zero(const void *p, size_t len) {
  const unsigned char *b = p;
  for (size_t i = 0; i < len; i++)
    if (b[i])
      return false;
  return true;
}

/* Bound to exactly this interface name, as iptables -i/-o writes it. */
static bool iface_is(const char *name, const unsigned char *mask,
                     const char *want) {
  size_t len = strnlen(want, IFNAMSIZ);
  if (!len || len >= IFNAMSIZ || strncmp(name, want, IFNAMSIZ))
    return false;
  for (size_t i = 0; i < IFNAMSIZ; i++)
    if (mask[i] != (i <= len ? 0xff : 0))
      return false;
  return true;
}

static bool is_accept(const struct xt_entry_target *t) {
  if (!strcmp(t->u.user.name, "ACCEPT"))
    return true;
  return !t->u.user.name[0] &&
         t->u.target_size >= XT_ALIGN(sizeof(struct xt_standard_target)) &&
         ((const struct xt_standard_target *)t)->verdict == -NF_ACCEPT - 1;
}

bool ipt_match_accept(int family, const unsigned char *e, const void *arg) {
  const struct ipt_iface_arg *a = arg;
  const struct xt_entry_target *t = ipt_target(family, e);
  struct ipt_view v;
  ipt_view(family, (void *)(uintptr_t)e, &v);
  size_t alen = ipt_addr_len(family);
  /* Exactly our rule: no match extensions, no addresses, no protocol, the
   * interface on one side only. Anyone else's ACCEPT stays. */
  if (!t || (const unsigned char *)t != e + ipt_entry_size(family) ||
      !is_accept(t) || *v.proto || *v.flags || *v.invflags ||
      !all_zero(v.smsk, alen) || !all_zero(v.dmsk, alen))
    return false;
  if (a->out)
    return iface_is(v.outiface, v.out_mask, a->iface) &&
           all_zero(v.in_mask, IFNAMSIZ);
  return iface_is(v.iniface, v.in_mask, a->iface) &&
         all_zero(v.out_mask, IFNAMSIZ);
}

bool ipt_match_masq(int family, const unsigned char *e, const void *arg) {
  const struct ipt_net_arg *a = arg;
  const struct xt_entry_target *t = ipt_target(family, e);
  struct ipt_view v;
  ipt_view(family, (void *)(uintptr_t)e, &v);
  size_t alen = ipt_addr_len(family);
  /* Exactly ours: no match extensions, no protocol or flags, only the
   * destination inverted. A -p tcp MASQUERADE for the same subnet is not. */
  return t && (const unsigned char *)t == e + ipt_entry_size(family) &&
         !strcmp(t->u.user.name, "MASQUERADE") && !*v.proto && !*v.flags &&
         *v.invflags == IPT_INV_DSTIP && !memcmp(v.src, a->net, alen) &&
         !memcmp(v.smsk, a->mask, alen) && !memcmp(v.dst, a->net, alen) &&
         !memcmp(v.dmsk, a->mask, alen) && all_zero(v.in_mask, IFNAMSIZ) &&
         all_zero(v.out_mask, IFNAMSIZ);
}

int ipt_net_parse(int family, const char *cidr, struct ipt_net_arg *out) {
  int len;
  memset(out, 0, sizeof(*out));
  if (!prefix_parse(family, cidr, out->net, &len))
    return -1;
  for (int i = 0; i < 16; i++, len -= 8) {
    out->mask[i] = len >= 8  ? 0xff
                   : len > 0 ? (unsigned char)(0xff << (8 - len))
                             : 0;
    out->net[i] &= out->mask[i];
  }
  return 0;
}

int ipt_build_accept(int family, const struct ipt_iface_arg *a,
                     unsigned char *buf) {
  size_t tsz = XT_ALIGN(sizeof(struct xt_standard_target));
  struct xt_standard_target *st =
      (struct xt_standard_target *)ipt_rule_begin(family, buf, tsz);
  struct ipt_view v;
  ipt_view(family, buf, &v);
  size_t len = strnlen(a->iface, IFNAMSIZ);
  if (!len || len >= IFNAMSIZ)
    return -1;
  memcpy(a->out ? v.outiface : v.iniface, a->iface, len);
  memset(a->out ? v.out_mask : v.in_mask, 0xff, len + 1);
  st->target.u.target_size = (uint16_t)tsz;
  st->verdict = -NF_ACCEPT - 1;
  return (int)(ipt_entry_size(family) + tsz);
}

int ipt_build_masq(int family, const struct ipt_net_arg *a,
                   unsigned char *buf) {
  /* The kernel checks the payload size per family: IPv4 wants the old
   * multi-range struct with rangesize 1, IPv6 a plain nf_nat_range. All
   * zero means "the outgoing interface's address". */
  size_t payload = family == AF_INET6
                       ? sizeof(struct nf_nat_range)
                       : sizeof(struct nf_nat_ipv4_multi_range_compat);
  size_t tsz = XT_ALIGN(sizeof(struct xt_entry_target) + payload);
  struct xt_entry_target *t = ipt_rule_begin(family, buf, tsz);
  struct ipt_view v;
  ipt_view(family, buf, &v);
  size_t alen = ipt_addr_len(family);
  memcpy(v.src, a->net, alen);
  memcpy(v.smsk, a->mask, alen);
  memcpy(v.dst, a->net, alen);
  memcpy(v.dmsk, a->mask, alen);
  *v.invflags = IPT_INV_DSTIP;
  t->u.target_size = (uint16_t)tsz;
  strcpy(t->u.user.name, "MASQUERADE");
  if (family == AF_INET)
    ((struct nf_nat_ipv4_multi_range_compat *)(void *)t->data)->rangesize = 1;
  return (int)(ipt_entry_size(family) + tsz);
}

/* Raw first; the binary only when the raw path could not read the table
 * (no ip_tables socket, an nftables-only kernel). A table it read but found
 * nothing in has nothing for the binary to find either. */
static int apply(int family, const char *table, unsigned hook,
                 const char *chain, bool add, ipt_match_fn match,
                 const void *arg, const unsigned char *rule, int rule_sz,
                 const char *const spec[]) {
  int fd = ipt_open(family), ret = -1;
  if (fd >= 0) {
    if (add) {
      ret = ipt_insert(fd, family, table, hook, rule, (unsigned)rule_sz, match,
                       arg);
    } else {
      ret = ipt_remove(fd, family, table, hook, match, arg);
      if (ret > 0)
        ret = 0;
    }
    close(fd);
  }
  if (ret == 0)
    return 0;
  if (!add) {
    /* Gone is gone: a rule that was never there is not an error. */
    bin_rule(family, table, "-D", chain, spec);
    return 0;
  }
  if (bin_rule(family, table, "-C", chain, spec) == 0)
    return 0;
  return bin_rule(family, table, "-I", chain, spec) == 0 ? 0 : -1;
}

int ipt_forward_accept(int family, const char *iface, bool add) {
  /* iptables reads -i "" as any interface: the binary would then delete
   * someone else's unconditional ACCEPT. */
  if (!iface || !*iface)
    return -1;
  int ret = 0;
  for (int out = 0; out < 2; out++) {
    struct ipt_iface_arg a = {iface, out};
    unsigned char rule[256];
    int sz = ipt_build_accept(family, &a, rule);
    const char *const spec[] = {out ? "-o" : "-i", iface, "-j", "ACCEPT", NULL};
    if (sz < 0 || apply(family, "filter", NF_INET_FORWARD, "FORWARD", add,
                        ipt_match_accept, &a, rule, sz, spec) < 0)
      ret = -1;
  }
  return ret;
}

int ipt_masquerade(int family, const char *cidr, bool add) {
  struct ipt_net_arg a;
  if (ipt_net_parse(family, cidr, &a) < 0)
    return -1;
  unsigned char rule[256];
  int sz = ipt_build_masq(family, &a, rule);
  const char *const spec[] = {"-s", cidr, "!",          "-d",
                              cidr, "-j", "MASQUERADE", NULL};
  return apply(family, "nat", NF_INET_POST_ROUTING, "POSTROUTING", add,
               ipt_match_masq, &a, rule, sz, spec);
}

int ipt_rules_present(int family, const char *iface, const char *cidr) {
  struct ipt_net_arg net;
  if (cidr && ipt_net_parse(family, cidr, &net) < 0)
    return -1;
  int fd = ipt_open(family);
  if (fd < 0)
    return -1;
  struct ipt_iface_arg in = {iface, false}, out = {iface, true};
  struct ipt_table t;
  int present = -1;
  /* One read of filter for both forward rules. */
  if (ipt_get(fd, family, "filter", &t) == 0) {
    present =
        ipt_blob_find(family, &t, NF_INET_FORWARD, ipt_match_accept, &in) &&
        ipt_blob_find(family, &t, NF_INET_FORWARD, ipt_match_accept, &out);
    ipt_free(&t);
    if (present && cidr) {
      int masq = ipt_find(fd, family, "nat", NF_INET_POST_ROUTING,
                          ipt_match_masq, &net);
      present = masq < 0 ? -1 : masq;
    }
  }
  close(fd);
  return present;
}

static bool nat_probe(int family);

/* The kernel's answer does not change while a process runs, and the monitor
 * asks every few seconds. */
bool ipt_nat_available(int family) {
  static int known[2] = {-1, -1};
  int i = family == AF_INET6;
  if (known[i] < 0)
    known[i] = nat_probe(family);
  return known[i];
}

static bool nat_probe(int family) {
  int fd = ipt_open(family);
  if (fd >= 0) {
    struct ipt_table t;
    bool ok = ipt_get(fd, family, "nat", &t) == 0;
    if (ok) {
      ipt_free(&t);
      ok = ipt_target_known(fd, family, "MASQUERADE");
    }
    close(fd);
    return ok;
  }
  /* No raw API: only the binary can ask. Listing the table would not load
   * the target, so the real rule goes into a chain nothing jumps to, and out
   * again. No packet ever sees it. */
  const char *const none[] = {NULL}, *const masq[] = {"-j", "MASQUERADE", NULL};
  bin_rule(family, "nat", "-N", "vap_nat_probe", none);
  bool ok = bin_rule(family, "nat", "-A", "vap_nat_probe", masq) == 0;
  bin_rule(family, "nat", "-F", "vap_nat_probe", none);
  bin_rule(family, "nat", "-X", "vap_nat_probe", none);
  return ok;
}

/* A custom chain in the raw table format is a lot of code for one optional
 * rule, so the TTL fix stays on the iptables binary. */
#define MANGLE(fam, ...)                                                       \
  ipt_bin(fam, "mangle", (const char *const[]){__VA_ARGS__, NULL})

bool ttl_supported(void) {
  MANGLE(AF_INET, "-N", "vap_ttl_probe");
  bool ok = MANGLE(AF_INET, "-A", "vap_ttl_probe", "-j", "TTL", "--ttl-set",
                   "64") == 0;
  MANGLE(AF_INET, "-F", "vap_ttl_probe");
  MANGLE(AF_INET, "-X", "vap_ttl_probe");
  return ok;
}

static void clear(int family) {
  /* Bounded: a delete that keeps succeeding must not spin a stop forever. */
  for (int n = 0; n < 16; n++)
    if (MANGLE(family, "-D", "FORWARD", "-j", "vap_ttl") != 0)
      break;
  MANGLE(family, "-F", "vap_ttl");
  MANGLE(family, "-X", "vap_ttl");
}

void ttl_clear(void) {
  clear(AF_INET);
  clear(AF_INET6);
}

/* IPv6 calls the same field hop limit, a target of its own that a kernel
 * with TTL can still lack; IPv4 stays fixed either way, with a word. */
static void apply6(const char *src6) {
  if (!src6 || !*src6)
    return;
  MANGLE(AF_INET6, "-N", "vap_ttl");
  if (MANGLE(AF_INET6, "-A", "vap_ttl", "-s", src6, "-j", "HL", "--hl-set",
             "64") != 0 ||
      MANGLE(AF_INET6, "-I", "FORWARD", "-j", "vap_ttl") != 0) {
    MANGLE(AF_INET6, "-F", "vap_ttl");
    MANGLE(AF_INET6, "-X", "vap_ttl");
    vap_warn("TTL fix: no IPv6 hop limit target, IPv6 leaves unchanged.");
  }
}

/* FORWARD only sees forwarded packets, after the kernel took its one off the
 * TTL, so the phone's own traffic is left alone and 64 is what leaves.
 * Matching the source keeps it to outbound packets: replies on their way to
 * the clients keep the TTL they arrived with. */
int ttl_apply(const char *src4, const char *src6) {
  ttl_clear();
  MANGLE(AF_INET, "-N", "vap_ttl");
  if (MANGLE(AF_INET, "-A", "vap_ttl", "-s", src4, "-j", "TTL", "--ttl-set",
             "64") != 0 ||
      MANGLE(AF_INET, "-I", "FORWARD", "-j", "vap_ttl") != 0) {
    ttl_clear();
    return -1;
  }
  apply6(src6);
  return 0;
}

int ttl_refresh(const char *src4, const char *src6) {
  /* The IPv4 jump gone means netd or a firewall app flushed mangle: all of
   * it goes back. */
  if (MANGLE(AF_INET, "-C", "FORWARD", "-j", "vap_ttl") != 0)
    return ttl_apply(src4, src6);
  clear(AF_INET6);
  apply6(src6);
  return 0;
}
