/* The lookups are from Droidspaces-OSS src/net/netlink.c; replace and del
 * take the table, source and scope the AP's routes need.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/route.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/fib_rules.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>

#include "core/core.h"
#include "net/netd.h"
#include "net/rule.h"

static int build(struct nl_req *r, int type, int flags,
                 const struct route *rt) {
  struct rtmsg *m = nl_req_init(r, type, flags, sizeof(*m));
  int alen = rt->family == AF_INET6 ? 16 : 4;
  m->rtm_family = (unsigned char)rt->family;
  m->rtm_table = RT_TABLE_UNSPEC;
  m->rtm_protocol = (unsigned char)(rt->proto ? rt->proto : RTPROT_BOOT);
  m->rtm_scope = (unsigned char)rt->scope;
  m->rtm_type = RTN_UNICAST;
  if (rt->dst) {
    unsigned char bin[16];
    int len;
    if (!prefix_parse(rt->family, rt->dst, bin, &len))
      return -EINVAL;
    m->rtm_dst_len = (unsigned char)len;
    nl_attr(r, RTA_DST, bin, alen);
  }
  if (rt->src) {
    unsigned char bin[16];
    if (inet_pton(rt->family, rt->src, bin) != 1)
      return -EINVAL;
    nl_attr(r, RTA_PREFSRC, bin, alen);
  }
  if (rt->dev) {
    unsigned idx = if_nametoindex(rt->dev);
    if (!idx)
      return -ENODEV;
    nl_attr_u32(r, RTA_OIF, idx);
  }
  /* Always the 32-bit attribute: netd's tables are numbered past 255. */
  nl_attr_u32(r, RTA_TABLE, rt->table ? (uint32_t)rt->table : RT_TABLE_MAIN);
  return 0;
}

int route_replace(struct nl *nl, const struct route *rt) {
  struct nl_req r;
  int ret = build(&r, RTM_NEWROUTE, NLM_F_CREATE | NLM_F_REPLACE, rt);
  return ret < 0 ? ret : nl_talk(nl, &r);
}

int route_del(struct nl *nl, const struct route *rt) {
  struct nl_req r;
  int ret = build(&r, RTM_DELROUTE, 0, rt);
  if (ret == -ENODEV)
    return 0;
  if (ret < 0)
    return ret;
  /* Any scope and protocol: the kernel matches only what is set. */
  struct rtmsg *m = NLMSG_DATA(&r.n);
  m->rtm_scope = RT_SCOPE_NOWHERE;
  m->rtm_protocol = 0;
  ret = nl_talk(nl, &r);
  return ret == -ESRCH || ret == -ENOENT ? 0 : ret;
}

struct walk {
  int family, table, target;
  int best_oif, best_len, found;
  uint32_t best_metric;
};

/* One route of a dump, as (table, oif, dst_len, metric), unicast only. */
static int route_fields(const struct nlmsghdr *h, int family, int *table,
                        int *oif, uint32_t *metric) {
  if (h->nlmsg_type != RTM_NEWROUTE)
    return -1;
  struct rtmsg *m = NLMSG_DATA(h);
  if (m->rtm_family != family || m->rtm_type != RTN_UNICAST)
    return -1;
  struct rtattr *tb[RTA_MAX + 1];
  nl_parse(tb, RTA_MAX, RTM_RTA(m), (int)RTM_PAYLOAD(h));
  *table = tb[RTA_TABLE] ? (int)nl_u32(tb[RTA_TABLE]) : m->rtm_table;
  *oif = (int)nl_u32(tb[RTA_OIF]);
  *metric = nl_u32(tb[RTA_PRIORITY]);
  return m->rtm_dst_len;
}

static void default_cb(const struct nlmsghdr *h, void *arg) {
  struct walk *w = arg;
  int table, oif;
  uint32_t metric;
  if (route_fields(h, w->family, &table, &oif, &metric) != 0)
    return;
  if (table != w->table || oif <= 0)
    return;
  if (!w->found || metric < w->best_metric) {
    w->best_oif = oif;
    w->best_metric = metric;
    w->found = 1;
  }
}

static struct nl_req *route_dump_req(struct nl_req *r, int family) {
  struct rtmsg *m = nl_req_init(r, RTM_GETROUTE, 0, sizeof(*m));
  m->rtm_family = (unsigned char)family;
  return r;
}

int route_default_oif(struct nl *nl, int family, int table,
                      char ifname[IF_NAMESIZE]) {
  struct nl_req r;
  struct walk w = {.family = family, .table = table};
  int ret = nl_dump(nl, route_dump_req(&r, family), default_cb, &w);
  if (ret < 0)
    return ret;
  if (!w.found)
    return -ENOENT;
  ifname[0] = '\0';
  return if_indextoname((unsigned)w.best_oif, ifname) ? 0 : -ENODEV;
}

static void iface_cb(const struct nlmsghdr *h, void *arg) {
  struct walk *w = arg;
  int table, oif;
  uint32_t metric;
  int len = route_fields(h, w->family, &table, &oif, &metric);
  if (len < 0 || len >= w->best_len || oif != w->target ||
      table == RT_TABLE_LOCAL)
    return;
  w->table = table;
  w->best_len = len;
  w->found = 1;
}

int route_iface_table(struct nl *nl, int family, const char *ifname,
                      int *table) {
  int idx = (int)if_nametoindex(ifname);
  if (!idx)
    return -ENODEV;
  struct nl_req r;
  struct walk w = {.family = family, .target = idx, .best_len = 129};
  int ret = nl_dump(nl, route_dump_req(&r, family), iface_cb, &w);
  if (ret < 0)
    return ret;
  if (!w.found)
    return -ENOENT;
  *table = w.table;
  return 0;
}

/* From Droidspaces-OSS src/net/netlink.c (rule_op, rule_exists), extended
 * with iif, oif and blackhole. */
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

/* netd_default_table is Droidspaces-OSS's ds_nl_get_android_default. */
struct best {
  int family, table, found;
  uint32_t prio;
};

static void netd_default_cb(const struct nlmsghdr *h, void *arg) {
  struct best *b = arg;
  if (h->nlmsg_type != RTM_NEWRULE)
    return;
  struct fib_rule_hdr *f = NLMSG_DATA(h);
  /* A table lookup from all to all; other actions and selectors are other
   * kinds of rule. */
  if (f->family != b->family || f->action != FR_ACT_TO_TBL || f->src_len ||
      f->dst_len)
    return;
  struct rtattr *tb[FRA_MAX + 1];
  nl_parse(tb, FRA_MAX, (struct rtattr *)(f + 1),
           (int)(h->nlmsg_len - NLMSG_LENGTH(sizeof(*f))));
  /* Older kernels send FRA_FWMARK only for a non-zero mark, newer ones send
   * it as 0 alongside a partial mask; both are `ip rule`'s 0x0/0xffff. */
  if (!tb[FRA_FWMASK] || nl_u32(tb[FRA_FWMASK]) != 0xffff)
    return;
  if (tb[FRA_FWMARK] && nl_u32(tb[FRA_FWMARK]) != 0)
    return;
  if (!tb[FRA_IIFNAME] || strcmp(RTA_DATA(tb[FRA_IIFNAME]), "lo"))
    return;
  /* Android 12+ per-app and work-profile preferences share the signature at
   * a higher precedence, for some uids only. Android 15+ writes the default
   * network itself as a range over every uid, 0-2147483647, which is the one
   * to keep, so a uid range is not enough to skip a rule. */
  const struct rtattr *ur = tb[FRA_UID_RANGE];
  if (ur) {
    const struct fib_rule_uid_range *r = RTA_DATA(ur);
    if (RTA_PAYLOAD(ur) < sizeof(*r) || r->start != 0 || r->end < 0x7fffffffu)
      return;
  }
  int table = tb[FRA_TABLE] ? (int)nl_u32(tb[FRA_TABLE]) : f->table;
  uint32_t prio = nl_u32(tb[FRA_PRIORITY]);
  if (table <= 0)
    return;
  if (!b->found || prio < b->prio) {
    b->table = table;
    b->prio = prio;
    b->found = 1;
  }
}

int netd_default_table(struct nl *nl, int family, int *table) {
  struct nl_req r;
  struct fib_rule_hdr *f = nl_req_init(&r, RTM_GETRULE, 0, sizeof(*f));
  f->family = (unsigned char)family;
  struct best b = {.family = family};
  int ret = nl_dump(nl, &r, netd_default_cb, &b);
  if (ret < 0)
    return ret;
  if (!b.found)
    return -ENOENT;
  *table = b.table;
  return 0;
}

int upstream_resolve(struct nl *nl, const char *want, struct upstream *up,
                     bool verbose) {
  memset(up, 0, sizeof(*up));
  if (!strcmp(want, "auto")) {
    /* IPv6 only when there is no IPv4 rule at all; netd writes both. */
    if (netd_default_table(nl, AF_INET, &up->table) < 0 &&
        netd_default_table(nl, AF_INET6, &up->table) < 0) {
      if (verbose) {
        vap_error("Could not auto-detect the upstream network (no netd "
                  "default-network rule).");
        vap_error("Select an upstream interface explicitly with -o <iface>.");
      }
      return -1;
    }
    if (route_default_oif(nl, AF_INET, up->table, up->iface) < 0)
      route_default_oif(nl, AF_INET6, up->table, up->iface);
    if (verbose && up->iface[0])
      vap_log("Auto upstream: table %d (%s)", up->table, up->iface);
    else if (verbose)
      vap_log("Auto upstream: table %d", up->table);
    return 0;
  }

  if (strlen(want) >= IF_NAMESIZE || !if_nametoindex(want)) {
    if (verbose)
      vap_error("Upstream interface '%s' does not exist.", want);
    return -1;
  }
  snprintf(up->iface, sizeof(up->iface), "%s", want);
  /* The table its internet routes live in, else main. */
  if (route_iface_table(nl, AF_INET, want, &up->table) < 0 &&
      route_iface_table(nl, AF_INET6, want, &up->table) < 0)
    up->table = RT_TABLE_MAIN;
  if (verbose)
    vap_log("Upstream: %s (table %d)", up->iface, up->table);
  return 0;
}

const char *upstream_v6_iface(const struct upstream *up) {
  return strncmp(up->iface, "v4-", 3) ? up->iface : up->iface + 3;
}
