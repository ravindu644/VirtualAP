/* start-ap's v6_sync_loop, a raw socket instead of busybox ping in a shell.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "monitor/v6disc.h"

#include <errno.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "core/log.h"
#include "mode/v6shared.h"
#include "net/addr.h"
#include "net/neigh.h"
#include "virtualap.h"

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
  int fd = socket(AF_INET6, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK,
                  IPPROTO_ICMPV6);
  if (fd < 0)
    return -1;
  struct icmp6_filter f;
  ICMP6_FILTER_SETBLOCKALL(&f);
  ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &f);
  unsigned idx = if_nametoindex(AP_IFACE);
  int off = 0;
  if (setsockopt(fd, IPPROTO_ICMPV6, ICMP6_FILTER, &f, sizeof(f)) < 0 ||
      setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, AP_IFACE,
                 sizeof(AP_IFACE)) < 0 ||
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
    ssize_t n =
        recvfrom(fd, &h, sizeof(h), 0, (struct sockaddr *)&from, &len);
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
