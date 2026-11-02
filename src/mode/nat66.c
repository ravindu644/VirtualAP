/* Droidspaces' NAT66 (network.c), for ap0: a ULA /64, router address ::1,
 * ip6tables MASQUERADE out of the upstream. The prefix is VirtualAP's own,
 * fd76:6170::/32 ("vap"), clear of Droidspaces' fd64:7370::/48, so a
 * container's NAT never collides with ours.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/nat66.h"

#include <stdio.h>

#include "core/log.h"
#include "mode/rules.h"
#include "net/addr.h"
#include "net/ipt_rules.h"
#include "net/route.h"
#include "virtualap.h"

bool nat66_possible(struct nl *nl, const struct upstream *up) {
  char oif[IF_NAMESIZE];
  return route_default_oif(nl, AF_INET6, up->table, oif) == 0 &&
         ipt_nat_available(AF_INET6);
}

void nat66_prefix(const struct subnet *net, struct v6 *v) {
  /* The /64 numbered after the IPv4 subnet, 192.168.42.1 gets
   * fd76:6170:42::/64, so a client sees the same number in both. */
  snprintf(v->prefix, sizeof(v->prefix), "fd76:6170:%u::", net->third);
  snprintf(v->net, sizeof(v->net), "%s/64", v->prefix);
  snprintf(v->gw, sizeof(v->gw), "%s1", v->prefix);
}

int nat66_setup(struct nl *nl, const struct upstream *up, struct v6 *v,
                const char **why) {
  vap_log("IPv6: NAT66 on %s out of %s.", v->net, v->up);

  if (addr_add(nl, AP_IFACE, AF_INET6, v->gw, 64, true) < 0) {
    *why = "could not add the NAT66 address to " AP_IFACE;
    return -1;
  }
  /* Android files an interface's own routes, the /64 and fe80::/64 too, in
   * table 1000+ifindex, which no rule reads; main gets them by hand. */
  struct route lan = {AF_INET6, v->net, AP_IFACE, NULL, RT_TABLE_MAIN, 0,
                      RT_SCOPE_LINK};
  struct route ll = {AF_INET6, "fe80::/64", AP_IFACE, NULL, RT_TABLE_MAIN, 0,
                     RT_SCOPE_LINK};
  if (route_replace(nl, &lan) < 0 || route_replace(nl, &ll) < 0 ||
      rules_add(nl, up, NULL, v, true) < 0) {
    *why = "routes or policy rules failed";
    return -1;
  }
  if (ipt_masquerade(AF_INET6, v->net, true) < 0) {
    *why = "ip6tables MASQUERADE failed";
    return -1;
  }
  return 0;
}
