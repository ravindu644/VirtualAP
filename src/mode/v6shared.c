/* start-ap's upstream_v6_addr and the shared-/64 half of setup_ipv6, and the
 * per-client step of v6_sync_loop.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/v6shared.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core/fsutil.h"
#include "core/log.h"
#include "net/addr.h"
#include "net/neigh.h"
#include "net/route.h"
#include "net/rule.h"
#include "virtualap.h"

/* ap0's address in the /64: EUI-64 from its MAC, so it cannot clash. */
static bool eui64(struct in6_addr *a) {
  char mac[32];
  unsigned m[6];
  if (read_file("/sys/class/net/" AP_IFACE "/address", mac, sizeof(mac)) <
          17 ||
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
  struct route ll = {AF_INET6, "fe80::/64", AP_IFACE, NULL, RT_TABLE_MAIN, 0,
                     RT_SCOPE_LINK};
  route_del(nl, &lan);
  route_replace(nl, &ll);
  struct rule to = {AF_INET6, NULL, v->net, NULL, NULL, RT_TABLE_MAIN,
                    PRIO_TO_SUBNET, false};
  struct rule sink = {AF_INET6, NULL, v->net, v->up, NULL, 0, PRIO_V6_SINK,
                      true};
  struct rule link = {AF_INET6, NULL, NULL, NULL, AP_IFACE, RT_TABLE_MAIN,
                      PRIO_V6_LINK, false};
  struct rule from = {AF_INET6, NULL, NULL, AP_IFACE, NULL, up->table,
                      PRIO_FROM_AP, false};
  if (rule_add(nl, &to) < 0 || rule_add(nl, &sink) < 0 ||
      rule_add(nl, &link) < 0 || rule_add(nl, &from) < 0) {
    *why = "policy rules failed";
    return -1;
  }
  int fd = open(V6CLIENTS, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd >= 0)
    close(fd);
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
