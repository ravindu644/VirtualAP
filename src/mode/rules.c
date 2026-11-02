/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/rules.h"

#include <string.h>

#include "core/log.h"
#include "net/rule.h"
#include "virtualap.h"

static void add(struct nl *nl, const struct rule *ru, bool verbose, int *ret) {
  int err = rule_add(nl, ru);
  if (err >= 0)
    return;
  if (verbose)
    vap_warn("Policy rule %d (IPv%c) could not be added: %s", ru->prio,
             ru->family == AF_INET6 ? '6' : '4', strerror(-err));
  *ret = -1;
}

int rules_add(struct nl *nl, const struct upstream *up,
              const struct subnet *net, const struct v6 *v, bool verbose) {
  int ret = 0;
  /* Replies to the clients resolve via main, above every netd rule (VPN
   * catch-alls included); everything from them goes to the upstream's
   * table. */
  if (net) {
    struct rule to = {AF_INET, NULL,          net->net,       NULL,
                      NULL,    RT_TABLE_MAIN, PRIO_TO_SUBNET, false};
    struct rule from = {AF_INET, NULL,      NULL,         AP_IFACE,
                        NULL,    up->table, PRIO_FROM_AP, false};
    add(nl, &to, verbose, &ret);
    add(nl, &from, verbose, &ret);
  }
  if (!v || !v->mode)
    return ret;
  /* The sink before the rule that opens the upstream: until it is there, a
   * packet for an address no client holds bounces back out. */
  struct rule to = {AF_INET6, NULL,          v->net,         NULL,
                    NULL,     RT_TABLE_MAIN, PRIO_TO_SUBNET, false};
  struct rule link = {AF_INET6, NULL,          NULL,         NULL,
                      AP_IFACE, RT_TABLE_MAIN, PRIO_V6_LINK, false};
  struct rule sink = {AF_INET6, NULL, v->net,       v->up,
                      NULL,     0,    PRIO_V6_SINK, true};
  struct rule from = {AF_INET6, NULL,      NULL,         AP_IFACE,
                      NULL,     up->table, PRIO_FROM_AP, false};
  add(nl, &to, verbose, &ret);
  add(nl, &link, verbose, &ret);
  if (v->mode == V6_SHARED)
    add(nl, &sink, verbose, &ret);
  add(nl, &from, verbose, &ret);
  return ret;
}

void rules_del_upstream(struct nl *nl) {
  /* A survivor would sit ahead of the new table's rule at the same
   * priority, and the kernel takes the first. */
  for (int i = 0; i < 2; i++) {
    int family = i ? AF_INET6 : AF_INET;
    int err = rule_del_prio(nl, family, PRIO_FROM_AP);
    if (err < 0)
      vap_warn("Policy rule %d (IPv%c) could not be removed: %s", PRIO_FROM_AP,
               i ? '6' : '4', strerror(-err));
  }
}

void rules_del(struct nl *nl, int family) {
  /* The upstream rule first, so nothing leaves while the rest go. */
  rule_del_prio(nl, family, PRIO_FROM_AP);
  rule_del_prio(nl, family, PRIO_TO_SUBNET);
  if (family != AF_INET6)
    return;
  rule_del_prio(nl, family, PRIO_V6_SINK);
  rule_del_prio(nl, family, PRIO_V6_LINK);
}
