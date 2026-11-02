/* The lookups are from Droidspaces-OSS src/net/netlink.c; replace and del
 * take the table, source and scope the AP's routes need.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/route.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "core/ipaddr.h"

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
