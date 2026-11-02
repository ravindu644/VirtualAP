/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/ipv6.h"

#include <linux/if_addr.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core/fsutil.h"
#include "core/log.h"
#include "mode/nat66.h"
#include "mode/rules.h"
#include "mode/v6shared.h"
#include "net/addr.h"
#include "net/ipt_rules.h"
#include "net/neigh.h"
#include "virtualap.h"

void ipv6_keep_ra(const char *iface) {
  /* Forwarding turns interfaces with accept_ra=1 into routers that ignore
   * RAs; the upstream keeps 2 so it keeps its address and default route. */
  conf6_set(iface, "accept_ra", "2");
}

int conf6_set(const char *iface, const char *key, const char *val) {
  char p[128];
  snprintf(p, sizeof(p), "/proc/sys/net/ipv6/conf/%s/%s", iface, key);
  return write_file(p, val);
}

void conf6_get(const char *iface, const char *key, char *out, size_t size) {
  char p[128];
  snprintf(p, sizeof(p), "/proc/sys/net/ipv6/conf/%s/%s", iface, key);
  if (read_file(p, out, size) < 0)
    out[0] = '\0';
}

bool v6_global_addr(struct nl *nl, const char *iface, bool only64,
                    struct in6_addr *out) {
  struct addr_info a[32];
  int idx = (int)if_nametoindex(iface);
  int n = idx ? addr_list(nl, AF_INET6, idx, a, 32) : 0;
  for (int i = 0; i < n; i++) {
    if (a[i].scope != RT_SCOPE_UNIVERSE || (only64 && a[i].prefix != 64) ||
        (a[i].flags & (IFA_F_DEPRECATED | IFA_F_TENTATIVE)) ||
        inet_pton(AF_INET6, a[i].addr, out) != 1)
      continue;
    if ((out->s6_addr[0] & 0xfe) == 0xfc)
      continue;
    return true;
  }
  return false;
}

void ipv6_save(const struct v6 *v, struct state *st) {
  static const char *const names[] = {"", "nat66", "shared"};
  state_set(st, "ipv6_mode", names[v->mode]);
  state_set(st, "ipv6_net", v->mode ? v->net : "");
  state_set(st, "ipv6_gw", v->mode ? v->gw : "");
  state_set(st, "ipv6_iface", v->mode ? v->up : "");
  state_set(st, "prev_ip6_forward", v->prev_fwd);
  state_set(st, "prev_ip6_proxy_ndp", v->prev_proxy);
}

void ipv6_teardown(struct nl *nl, const struct state *st) {
  const char *up = state_get(st, "ipv6_iface");
  const char *mode = state_get(st, "ipv6_mode");
  rules_del(nl, AF_INET6);
  ipt_forward_accept(AF_INET6, AP_IFACE, false);
  if (!strcmp(mode, "nat66") && *state_get(st, "ipv6_net"))
    ipt_masquerade(AF_INET6, state_get(st, "ipv6_net"), false);
  FILE *f = *up ? fopen(V6CLIENTS, "re") : NULL;
  if (f) {
    char line[INET6_ADDRSTRLEN + 2];
    while (fgets(line, sizeof(line), f)) {
      line[strcspn(line, "\r\n")] = '\0';
      if (*line)
        neigh_proxy(nl, false, line, up);
    }
    fclose(f);
  }
  unlink(V6CLIENTS);
  /* The /128 routes go with the interface; the address is flushed for the
   * rollback paths where ap0 lives on. */
  addr_flush(nl, AP_IFACE, AF_INET6, true);
  const char *proxy = state_get(st, "prev_ip6_proxy_ndp");
  if (*up && *proxy)
    conf6_set(up, "proxy_ndp", proxy);
  if (!strcmp(state_get(st, "prev_ip6_forward"), "0"))
    conf6_set("all", "forwarding", "0");
}

/* A failure after the sysctls were touched: undo it all and leave v off, so
 * the state file records an IPv4-only AP. */
static void abort_v6(struct nl *nl, struct v6 *v, const char *why) {
  vap_warn("IPv6: %s - IPv4 only.", why);
  struct state st = {0};
  ipv6_save(v, &st);
  ipv6_teardown(nl, &st);
  memset(v, 0, sizeof(*v));
}

const char *ipv6_decide(struct nl *nl, const struct vap_opts *o,
                        const struct upstream *up, const struct subnet *net,
                        struct v6 *v, struct in6_addr *upaddr) {
  memset(v, 0, sizeof(*v));
  if (strcmp(o->ipv6, "1"))
    return NULL;
  const char *up6 = upstream_v6_iface(up);
  if (!*up6)
    return "upstream interface unknown";
  snprintf(v->up, sizeof(v->up), "%s", up6);
  /* NAT66 needs a global address on the upstream to masquerade to; the
   * shared /64 needs that address in a /64 of its own. */
  static char why[96];
  if (!v6_global_addr(nl, up6, false, upaddr)) {
    snprintf(why, sizeof(why), "%s has no global IPv6 address", up6);
    return why;
  }
  if (nat66_possible(nl, up)) {
    v->mode = V6_NAT66;
    nat66_prefix(net, v);
    return NULL;
  }
  if (!v6_global_addr(nl, up6, true, upaddr)) {
    snprintf(why, sizeof(why),
             "%s has no global /64 prefix and the kernel cannot NAT66", up6);
    return why;
  }
  v->mode = V6_SHARED;
  shared_prefix(upaddr, v);
  return NULL;
}

void ipv6_setup(struct nl *nl, const struct vap_opts *o,
                const struct upstream *up, const struct subnet *net,
                struct v6 *v) {
  struct in6_addr upaddr;
  const char *off = ipv6_decide(nl, o, up, net, v, &upaddr);
  if (!v->mode) {
    if (off)
      vap_log("IPv6: %s - IPv4 only.", off);
    memset(v, 0, sizeof(*v));
    return;
  }
  const char *up6 = v->up;

  /* ap0 holds addresses but never configures itself from (rogue) RAs: we are
   * the router on this link. */
  conf6_set(AP_IFACE, "disable_ipv6", "0");
  conf6_set(AP_IFACE, "accept_ra", "0");
  conf6_set(AP_IFACE, "autoconf", "0");
  conf6_get("all", "forwarding", v->prev_fwd, sizeof(v->prev_fwd));
  ipv6_keep_ra(up6);
  if (conf6_set("all", "forwarding", "1") < 0) {
    abort_v6(nl, v, "could not enable forwarding");
    return;
  }

  const char *why = NULL;
  int ret = v->mode == V6_NAT66 ? nat66_setup(nl, up, v, &why)
                                : shared_setup(nl, up, &upaddr, v, &why);
  if (ret < 0) {
    abort_v6(nl, v, why);
    return;
  }
  if (ipt_forward_accept(AF_INET6, AP_IFACE, true) < 0)
    abort_v6(nl, v, "ip6tables FORWARD rules failed");
}
