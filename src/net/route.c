/* The lookups are from Droidspaces-OSS src/net/netlink.c; replace and del
 * take the table, source and scope the AP's routes need.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "internal.h"
#include "net/net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fnmatch.h>
#include <linux/fib_rules.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/core.h"

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
  if (len < 0 || len >= w->best_len || oif != w->target)
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
  /* Carriers hand out a /8 and Wi-Fi a /24, and neither reaches the internet
   * on its own, so only a prefix wider than any connected subnet counts. */
  struct walk w = {.family = family, .target = idx, .best_len = 8};
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

/* The rest is Droidspaces-OSS's src/net/route/uplink.c. */

/* The only interface families that terminate internet access on Android,
 * highest precedence first: Wi-Fi, ethernet, CLAT, Qualcomm's mobile data,
 * MediaTek's. */
static const char *const uplink_patterns[] = {
    "wlan*", "eth*", "v4-*", "rmnet*", "*ccmni*",
};

/* Never an uplink. swlan, ap, usb, rndis and ncm share the phone's connection
 * with clients instead of reaching the internet; tun and ppp are VPNs, which
 * only an explicit -o goes through; the rest are loopback, placeholders and
 * Droidspaces' own links. */
static const char *const uplink_excludes[] = {
    "ds-*",   "lo",   "dummy*", "swlan*", "ap*",  "usb*",
    "rndis*", "ncm*", "p2p*",   "tun*",   "ppp*", "bt-pan",
};

static bool iface_running(const char *ifname) {
  int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return false;
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", ifname);
  bool up = ioctl(fd, SIOCGIFFLAGS, &ifr) == 0 &&
            (ifr.ifr_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING);
  close(fd);
  return up;
}

static bool candidate_ok(const char *ifname) {
  if (!ifname[0])
    return false;
  for (size_t i = 0; i < sizeof(uplink_excludes) / sizeof(uplink_excludes[0]);
       i++)
    if (fnmatch(uplink_excludes[i], ifname, 0) == 0)
      return false;
  return iface_running(ifname);
}

static bool scan_whitelist(struct nl *nl, struct upstream *up) {
  struct if_nameindex *all = if_nameindex();
  if (!all)
    return false;
  bool found = false;
  for (size_t p = 0;
       !found && p < sizeof(uplink_patterns) / sizeof(uplink_patterns[0]); p++)
    for (struct if_nameindex *i = all; !found && i->if_index; i++)
      if (fnmatch(uplink_patterns[p], i->if_name, 0) == 0 &&
          candidate_ok(i->if_name) &&
          route_iface_table(nl, AF_INET, i->if_name, &up->table) == 0) {
        snprintf(up->iface, sizeof(up->iface), "%s", i->if_name);
        found = true;
      }
  if_freenameindex(all);
  return found;
}

/* -o's list in priority order: the first entry that is up with a route wins.
 * No excludes, the user picked these, and no fallback to auto: traffic leaves
 * through the list or not at all. */
static const char *probe_pinned(struct nl *nl, int family, const char *want,
                                struct upstream *up) {
  for (const char *p = want + strspn(want, ", "); *p; p += strspn(p, ", ")) {
    size_t n = strcspn(p, ", ");
    if (n < IF_NAMESIZE) {
      char name[IF_NAMESIZE];
      memcpy(name, p, n);
      name[n] = '\0';
      if (iface_running(name) &&
          route_iface_table(nl, family, name, &up->table) == 0) {
        snprintf(up->iface, sizeof(up->iface), "%s", name);
        return "pinned";
      }
    }
    p += n;
  }
  return NULL;
}

/* One family's detection tiers, first hit wins: netd's default-network rule,
 * the kernel's ground truth, swapped on every handoff and never pointing at
 * IMS or MMS; the main table's default, for ROMs that populate it; a scan of
 * the known uplink names. The scan is IPv4 only because a name match is a
 * guess and the IMS bearer fits it, an rmnetX that is always up, IPv6, with a
 * default route of its own. */
static const char *probe_auto(struct nl *nl, int family, struct upstream *up) {
  int table;
  if (netd_default_table(nl, family, &table) == 0 &&
      route_default_oif(nl, family, table, up->iface) == 0 &&
      candidate_ok(up->iface)) {
    up->table = table;
    return "netd rule";
  }
  if (route_default_oif(nl, family, RT_TABLE_MAIN, up->iface) == 0 &&
      candidate_ok(up->iface)) {
    up->table = RT_TABLE_MAIN;
    return "main table";
  }
  if (family == AF_INET && scan_whitelist(nl, up))
    return "whitelist scan";
  return NULL;
}

/* The start and every look of the monitor land here, and only what changed is
 * logged: the last upstream named, and whether the lack of one was warned
 * about. The monitor is forked from the start and inherits both. */
int upstream_resolve(struct nl *nl, const char *want, struct upstream *up) {
  static char last[IF_NAMESIZE];
  static bool warned;
  bool pinned = strcmp(want, "auto") != 0;
  const char *how = NULL;
  /* IPv4 through every tier first, so dual-stack and CLAT networks resolve to
   * what carries IPv4. IPv6 only when nothing does. Trying it per tier would
   * let the netd rule return rmnetX on a CLAT network before the scan finds
   * v4-rmnetX, and cost the clients their IPv4. */
  for (int i = 0; !how && i < 2; i++) {
    int family = i ? AF_INET6 : AF_INET;
    memset(up, 0, sizeof(*up));
    how = pinned ? probe_pinned(nl, family, want, up)
                 : probe_auto(nl, family, up);
  }
  if (!how) {
    memset(up, 0, sizeof(*up));
    last[0] = '\0';
    if (!warned) {
      if (pinned)
        vap_warn("No upstream: none of '%s' is up with a route. Clients have "
                 "no internet until one is, the monitor will wire it.",
                 want);
      else
        vap_warn("No upstream network. Clients have no internet until one "
                 "appears, the monitor will wire it.");
      warned = true;
    }
    return -1;
  }
  warned = false;
  if (strcmp(last, up->iface)) {
    vap_log("Upstream: %s (table %d) [%s]", up->iface, up->table, how);
    snprintf(last, sizeof(last), "%s", up->iface);
  }
  return 0;
}

const char *upstream_v6_iface(const struct upstream *up) {
  return strncmp(up->iface, "v4-", 3) ? up->iface : up->iface + 3;
}
