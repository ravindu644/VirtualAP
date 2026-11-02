/* From Droidspaces-OSS src/net/netlink.c (rule_op, rule_exists), extended
 * with iif, oif and blackhole.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/rule.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/fib_rules.h>
#include <net/if.h>
#include <string.h>

#include "core/ipaddr.h"

struct prefix {
  unsigned char bin[16];
  int len;
};

static int parse(int family, const char *s, struct prefix *p) {
  memset(p, 0, sizeof(*p));
  return !s || prefix_parse(family, s, p->bin, &p->len) ? 0 : -EINVAL;
}

struct match {
  const struct rule *ru;
  struct prefix from, to;
  bool found;
};

static bool same_name(const struct rtattr *a, const char *want) {
  if (!want)
    return !a;
  return a && !strncmp(RTA_DATA(a), want, RTA_PAYLOAD(a));
}

static bool same_addr(const struct rtattr *a, const struct prefix *p,
                      int alen) {
  if (!p->len)
    return !a;
  return a && RTA_PAYLOAD(a) == (unsigned)alen &&
         !memcmp(RTA_DATA(a), p->bin, (size_t)alen);
}

static void exists_cb(const struct nlmsghdr *h, void *arg) {
  struct match *m = arg;
  const struct rule *ru = m->ru;
  if (h->nlmsg_type != RTM_NEWRULE)
    return;
  struct fib_rule_hdr *f = NLMSG_DATA(h);
  int alen = ru->family == AF_INET6 ? 16 : 4;
  if (f->family != ru->family || f->src_len != m->from.len ||
      f->dst_len != m->to.len ||
      f->action != (ru->blackhole ? FR_ACT_BLACKHOLE : FR_ACT_TO_TBL))
    return;
  struct rtattr *tb[FRA_MAX + 1];
  nl_parse(tb, FRA_MAX, (struct rtattr *)(f + 1),
           (int)(h->nlmsg_len - NLMSG_LENGTH(sizeof(*f))));
  /* Ours never carry a mark, a uid range or the rest; a rule that does is
   * someone else's, however alike. The kernel sends suppress_prefixlen on
   * every rule, -1 when unused. */
  if ((tb[FRA_FWMARK] && nl_u32(tb[FRA_FWMARK])) || tb[FRA_FWMASK] ||
      tb[FRA_UID_RANGE] ||
      (tb[FRA_SUPPRESS_PREFIXLEN] &&
       nl_u32(tb[FRA_SUPPRESS_PREFIXLEN]) != 0xffffffffu) ||
      tb[FRA_L3MDEV] || tb[FRA_IP_PROTO] || tb[FRA_SPORT_RANGE] ||
      tb[FRA_DPORT_RANGE] || f->tos || (f->flags & FIB_RULE_INVERT))
    return;
  int table = tb[FRA_TABLE] ? (int)nl_u32(tb[FRA_TABLE]) : f->table;
  if ((int)nl_u32(tb[FRA_PRIORITY]) != ru->prio ||
      (!ru->blackhole && table != ru->table) ||
      !same_name(tb[FRA_IIFNAME], ru->iif) ||
      !same_name(tb[FRA_OIFNAME], ru->oif) ||
      !same_addr(tb[FRA_SRC], &m->from, alen) ||
      !same_addr(tb[FRA_DST], &m->to, alen))
    return;
  m->found = true;
}

static struct nl_req *dump_req(struct nl_req *r, int family) {
  struct fib_rule_hdr *f = nl_req_init(r, RTM_GETRULE, 0, sizeof(*f));
  f->family = (unsigned char)family;
  return r;
}

bool rule_exists(struct nl *nl, const struct rule *ru) {
  struct match m = {.ru = ru};
  if (parse(ru->family, ru->from, &m.from) < 0 ||
      parse(ru->family, ru->to, &m.to) < 0)
    return false;
  struct nl_req r;
  return nl_dump(nl, dump_req(&r, ru->family), exists_cb, &m) == 0 && m.found;
}

int rule_add(struct nl *nl, const struct rule *ru) {
  struct prefix from, to;
  if (parse(ru->family, ru->from, &from) < 0 ||
      parse(ru->family, ru->to, &to) < 0)
    return -EINVAL;
  if (rule_exists(nl, ru))
    return 0;
  int alen = ru->family == AF_INET6 ? 16 : 4;
  struct nl_req r;
  struct fib_rule_hdr *f =
      nl_req_init(&r, RTM_NEWRULE, NLM_F_CREATE | NLM_F_EXCL, sizeof(*f));
  f->family = (unsigned char)ru->family;
  f->src_len = (unsigned char)from.len;
  f->dst_len = (unsigned char)to.len;
  f->action = ru->blackhole ? FR_ACT_BLACKHOLE : FR_ACT_TO_TBL;
  if (from.len)
    nl_attr(&r, FRA_SRC, from.bin, alen);
  if (to.len)
    nl_attr(&r, FRA_DST, to.bin, alen);
  if (ru->iif)
    nl_attr_str(&r, FRA_IIFNAME, ru->iif);
  if (ru->oif)
    nl_attr_str(&r, FRA_OIFNAME, ru->oif);
  if (!ru->blackhole)
    nl_attr_u32(&r, FRA_TABLE, (uint32_t)ru->table);
  nl_attr_u32(&r, FRA_PRIORITY, (uint32_t)ru->prio);
  int ret = nl_talk(nl, &r);
  return ret == -EEXIST ? 0 : ret;
}

int rule_del_prio(struct nl *nl, int family, int prio) {
  /* With only a priority given the kernel deletes the first rule there; a
   * bound keeps a kernel that keeps answering 0 from looping us forever. */
  for (int i = 0; i < 64; i++) {
    struct nl_req r;
    struct fib_rule_hdr *f = nl_req_init(&r, RTM_DELRULE, 0, sizeof(*f));
    f->family = (unsigned char)family;
    nl_attr_u32(&r, FRA_PRIORITY, (uint32_t)prio);
    int ret = nl_talk(nl, &r);
    if (ret == -ENOENT || ret == -ESRCH)
      return 0;
    if (ret < 0)
      return ret;
  }
  return 0;
}
