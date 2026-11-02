/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/ipv6.h"

#include <errno.h>
#include <linux/if_addr.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/core.h"
#include "mode/nat66.h"
#include "mode/rules.h"
#include "mode/v6shared.h"
#include "monitor/v6disc.h"
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

/* The shared /64's client discovery: a raw ICMPv6 socket, its replies read
 * in the monitor's own poll loop, so no ping process to spawn and parse every
 * beat. */
#define MAX_KNOWN 256
#define ECHO_ID 0x7661 /* "va" */

/* The addresses installed, and whether each answered since the last
 * refresh. */
static struct {
  struct in6_addr a;
  bool seen;
} known[MAX_KNOWN];
static int nknown;
static bool full_said;

void v6disc_reset(void) { nknown = 0; }

void v6disc_refresh(void) {
  int n = 0;
  for (int i = 0; i < nknown; i++)
    if (known[i].seen) {
      known[n] = known[i];
      known[n++].seen = false;
    }
  nknown = n;
  full_said = false;
}

int v6disc_open(const struct v6 *v) {
  struct sockaddr_in6 me = {.sin6_family = AF_INET6};
  if (inet_pton(AF_INET6, v->gw, &me.sin6_addr) != 1)
    return -1;
  int fd =
      socket(AF_INET6, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, IPPROTO_ICMPV6);
  if (fd < 0)
    return -1;
  struct icmp6_filter f;
  ICMP6_FILTER_SETBLOCKALL(&f);
  ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &f);
  unsigned idx = if_nametoindex(AP_IFACE);
  int off = 0;
  if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &f, sizeof(f)) < 0 ||
      setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, AP_IFACE, sizeof(AP_IFACE)) <
          0 ||
      setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_IF, &idx, sizeof(idx)) < 0 ||
      setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &off, sizeof(off)) <
          0 ||
      bind(fd, (struct sockaddr *)&me, sizeof(me)) < 0) {
    close(fd);
    return -1;
  }
  return fd;
}

void v6disc_ping(int fd) {
  static uint16_t seq;
  struct icmp6_hdr echo = {.icmp6_type = ICMP6_ECHO_REQUEST};
  echo.icmp6_id = htons(ECHO_ID);
  echo.icmp6_seq = htons(++seq);
  struct sockaddr_in6 all = {.sin6_family = AF_INET6,
                             .sin6_scope_id = if_nametoindex(AP_IFACE)};
  inet_pton(AF_INET6, "ff02::1", &all.sin6_addr);
  /* The kernel fills in the ICMPv6 checksum on a raw ICMPv6 socket. */
  sendto(fd, &echo, sizeof(echo), 0, (struct sockaddr *)&all, sizeof(all));
}

/* Not a client: ours, or a host the phone already reaches on the upstream
 * link (the router above all). A client answering for one of those would
 * have us claim it on the upstream with a proxy entry and swallow its
 * traffic. */
static bool upstream_side(struct nl *nl, const struct v6 *v,
                          const struct in6_addr *a, const char *text) {
  if (neigh_known(nl, text, v->up))
    return true;
  /* The phone's own upstream addresses sit in the very /64 it shares. */
  struct addr_info mine[32];
  int idx = (int)if_nametoindex(v->up);
  int n = idx ? addr_list(nl, AF_INET6, idx, mine, 32) : 0;
  for (int i = 0; i < n; i++) {
    struct in6_addr m;
    if (inet_pton(AF_INET6, mine[i].addr, &m) == 1 && !memcmp(&m, a, sizeof(m)))
      return true;
  }
  return false;
}

void v6disc_read(int fd, struct nl *nl, const struct v6 *v) {
  struct in6_addr pfx, gw;
  inet_pton(AF_INET6, v->prefix, &pfx);
  inet_pton(AF_INET6, v->gw, &gw);
  for (;;) {
    struct icmp6_hdr h;
    struct sockaddr_in6 from;
    socklen_t len = sizeof(from);
    ssize_t n = recvfrom(fd, &h, sizeof(h), 0, (struct sockaddr *)&from, &len);
    if (n < 0)
      return;
    const struct in6_addr *a = &from.sin6_addr;
    /* Answers to our own pings only, from inside our /64 (outside means a
     * client made it up): never proxy anything else. */
    if (n < (ssize_t)sizeof(h) || ntohs(h.icmp6_id) != ECHO_ID ||
        memcmp(a->s6_addr, pfx.s6_addr, 8) || !memcmp(a, &gw, sizeof(gw)))
      continue;
    int i;
    for (i = 0; i < nknown && memcmp(&known[i].a, a, sizeof(*a)); i++)
      ;
    char text[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, a, text, sizeof(text));
    if (i < nknown) {
      /* Re-installed once per refresh: an ap0 that went down and up took
       * the /128 with it. */
      if (!known[i].seen) {
        known[i].seen = true;
        shared_add_client(nl, text, v->up, false);
      }
      continue;
    }
    if (nknown == MAX_KNOWN) {
      if (!full_said)
        vap_warn("IPv6: %d client addresses known, %s waits for the next "
                 "refresh.",
                 MAX_KNOWN, text);
      full_said = true;
      continue;
    }
    if (upstream_side(nl, v, a, text))
      continue;
    if (shared_add_client(nl, text, v->up, true) == 0) {
      known[nknown].a = *a;
      known[nknown++].seen = true;
      vap_log("IPv6 client %s routed.", text);
    }
  }
}
