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
#include "net/route.h"
#include "virtualap.h"

/* Droidspaces' NAT66 (network.c), for ap0: a ULA /64, router address ::1,
 * ip6tables MASQUERADE out of the upstream. The prefix is VirtualAP's own,
 * fd76:6170::/32 ("vap"), clear of Droidspaces' fd64:7370::/48, so a
 * container's NAT never collides with ours. */
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
  struct route lan = {AF_INET6,      v->net, AP_IFACE,     NULL,
                      RT_TABLE_MAIN, 0,      RT_SCOPE_LINK};
  struct route ll = {AF_INET6,      "fe80::/64", AP_IFACE,     NULL,
                     RT_TABLE_MAIN, 0,           RT_SCOPE_LINK};
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

/* ap0's address in the /64: EUI-64 from its MAC, so it cannot clash. */
static bool eui64(struct in6_addr *a) {
  char mac[32];
  unsigned m[6];
  if (read_file("/sys/class/net/" AP_IFACE "/address", mac, sizeof(mac)) < 17 ||
      sscanf(mac, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4],
             &m[5]) != 6)
    return false;
  a->s6_addr[8] = (unsigned char)(m[0] ^ 2);
  a->s6_addr[9] = (unsigned char)m[1];
  a->s6_addr[10] = (unsigned char)m[2];
  a->s6_addr[11] = 0xff;
  a->s6_addr[12] = 0xfe;
  a->s6_addr[13] = (unsigned char)m[3];
  a->s6_addr[14] = (unsigned char)m[4];
  a->s6_addr[15] = (unsigned char)m[5];
  return true;
}

void shared_prefix(const struct in6_addr *upaddr, struct v6 *v) {
  struct in6_addr pfx = *upaddr;
  memset(pfx.s6_addr + 8, 0, 8);
  inet_ntop(AF_INET6, &pfx, v->prefix, sizeof(v->prefix));
  snprintf(v->net, sizeof(v->net), "%s/64", v->prefix);
}

int shared_setup(struct nl *nl, const struct upstream *up,
                 const struct in6_addr *upaddr, struct v6 *v,
                 const char **why) {
  struct in6_addr gw;
  inet_pton(AF_INET6, v->prefix, &gw);
  if (!eui64(&gw)) {
    *why = "cannot read " AP_IFACE "'s MAC";
    return -1;
  }
  inet_ntop(AF_INET6, &gw, v->gw, sizeof(v->gw));
  char ua[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, upaddr, ua, sizeof(ua));
  vap_log("IPv6: %s has %s/64 - sharing %s with the clients.", v->up, ua,
          v->net);

  conf6_get(v->up, "proxy_ndp", v->prev_proxy, sizeof(v->prev_proxy));
  if (conf6_set(v->up, "proxy_ndp", "1") < 0) {
    *why = "could not enable proxy_ndp";
    return -1;
  }
  /* /64 so dnsmasq advertises the prefix from it, nodad so it is usable at
   * once. The connected route it brings goes: the /64 must not be on-link
   * here, the upstream owns it and each client gets a /128. */
  if (addr_add(nl, AP_IFACE, AF_INET6, v->gw, 64, true) < 0) {
    *why = "could not add the address to " AP_IFACE;
    return -1;
  }
  struct route lan = {AF_INET6, v->net, AP_IFACE, NULL, RT_TABLE_MAIN, 0, 0};
  struct route ll = {AF_INET6,      "fe80::/64", AP_IFACE,     NULL,
                     RT_TABLE_MAIN, 0,           RT_SCOPE_LINK};
  route_del(nl, &lan);
  route_replace(nl, &ll);
  if (rules_add(nl, up, NULL, v, true) < 0) {
    *why = "policy rules failed";
    return -1;
  }
  /* shared_add_client creates it with the first client. */
  write_file(V6CLIENTS, "");
  return 0;
}

int shared_add_client(struct nl *nl, const char *addr, const char *up,
                      bool record) {
  struct route host = {AF_INET6, addr, AP_IFACE, NULL, RT_TABLE_MAIN, 0, 0};
  if (route_replace(nl, &host) < 0 || neigh_proxy(nl, true, addr, up) < 0)
    return -1;
  FILE *f = record ? fopen(V6CLIENTS, "ae") : NULL;
  if (f) {
    fprintf(f, "%s\n", addr);
    fclose(f);
  }
  return 0;
}

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
