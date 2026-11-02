/* netd_default_table is Droidspaces-OSS's ds_nl_get_android_default.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/netd.h"

#include <errno.h>
#include <linux/fib_rules.h>
#include <stdio.h>
#include <string.h>

#include "core/log.h"
#include "net/route.h"

struct best {
  int family, table, found;
  uint32_t prio;
};

static void default_cb(const struct nlmsghdr *h, void *arg) {
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
   * to keep; start-ap dropped every uidrange rule and missed it. */
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
  int ret = nl_dump(nl, &r, default_cb, &b);
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
  /* The table its internet routes live in, else main, like start-ap. */
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
