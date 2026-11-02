/* add from Droidspaces-OSS src/net/netlink.c; list and flush are new.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/addr.h"

#include <errno.h>
#include <linux/if_addr.h>
#include <net/if.h>
#include <string.h>

#ifndef IFA_FLAGS
#define IFA_FLAGS 8
#endif

int addr_add(struct nl *nl, const char *ifname, int family, const char *addr,
             int prefix, bool nodad) {
  unsigned char bin[16];
  unsigned idx = if_nametoindex(ifname);
  if (!idx)
    return -ENODEV;
  if (inet_pton(family, addr, bin) != 1)
    return -EINVAL;
  int alen = family == AF_INET6 ? 16 : 4;
  struct nl_req r;
  struct ifaddrmsg *a =
      nl_req_init(&r, RTM_NEWADDR, NLM_F_CREATE | NLM_F_REPLACE, sizeof(*a));
  a->ifa_family = (unsigned char)family;
  a->ifa_prefixlen = (unsigned char)prefix;
  a->ifa_index = idx;
  a->ifa_scope = RT_SCOPE_UNIVERSE;
  /* The old 8-bit flag, so it works on the 3.10 floor. */
  if (nodad)
    a->ifa_flags = IFA_F_NODAD;
  nl_attr(&r, IFA_LOCAL, bin, alen);
  nl_attr(&r, IFA_ADDRESS, bin, alen);
  if (family == AF_INET && prefix < 32) {
    uint32_t ip, bcast;
    memcpy(&ip, bin, 4);
    bcast = ip | htonl(0xffffffffu >> prefix);
    nl_attr(&r, IFA_BROADCAST, &bcast, 4);
  }
  return nl_talk(nl, &r);
}

struct list {
  struct addr_info *out;
  int ifindex, max, n;
};

static void list_cb(const struct nlmsghdr *h, void *arg) {
  struct list *l = arg;
  if (h->nlmsg_type != RTM_NEWADDR || l->n >= l->max)
    return;
  struct ifaddrmsg *a = NLMSG_DATA(h);
  if (l->ifindex && (int)a->ifa_index != l->ifindex)
    return;
  struct rtattr *tb[IFA_FLAGS + 1];
  nl_parse(tb, IFA_FLAGS, IFA_RTA(a), (int)IFA_PAYLOAD(h));
  /* IFA_LOCAL is the address itself; IFA_ADDRESS is the peer on a
   * point-to-point link and the address everywhere else. */
  const struct rtattr *src = tb[IFA_LOCAL] ? tb[IFA_LOCAL] : tb[IFA_ADDRESS];
  if (!src)
    return;
  struct addr_info *o = &l->out[l->n];
  o->ifindex = (int)a->ifa_index;
  o->family = a->ifa_family;
  o->prefix = a->ifa_prefixlen;
  o->scope = a->ifa_scope;
  o->flags = tb[IFA_FLAGS] ? nl_u32(tb[IFA_FLAGS]) : a->ifa_flags;
  if (!inet_ntop(a->ifa_family, RTA_DATA(src), o->addr, sizeof(o->addr)))
    return;
  l->n++;
}

int addr_list(struct nl *nl, int family, int ifindex, struct addr_info *out,
              int max) {
  struct nl_req r;
  struct ifaddrmsg *a = nl_req_init(&r, RTM_GETADDR, 0, sizeof(*a));
  a->ifa_family = (unsigned char)family;
  struct list l = {out, ifindex, max, 0};
  int ret = nl_dump(nl, &r, list_cb, &l);
  return ret < 0 ? ret : l.n;
}

int addr_flush(struct nl *nl, const char *ifname, int family,
               bool global_only) {
  int idx = (int)if_nametoindex(ifname);
  if (!idx)
    return -ENODEV;
  struct addr_info addrs[64];
  /* A full batch may have left more behind (privacy addresses pile up). */
  for (int pass = 0; pass < 16; pass++) {
    int n = addr_list(nl, family, idx, addrs, 64), done = 0;
    if (n < 0)
      return n;
    for (int i = 0; i < n; i++) {
      if (global_only && addrs[i].scope != RT_SCOPE_UNIVERSE)
        continue;
      unsigned char bin[16];
      if (inet_pton(addrs[i].family, addrs[i].addr, bin) != 1)
        continue;
      struct nl_req r;
      struct ifaddrmsg *a = nl_req_init(&r, RTM_DELADDR, 0, sizeof(*a));
      a->ifa_family = (unsigned char)addrs[i].family;
      a->ifa_prefixlen = (unsigned char)addrs[i].prefix;
      a->ifa_index = (unsigned)idx;
      nl_attr(&r, IFA_LOCAL, bin, addrs[i].family == AF_INET6 ? 16 : 4);
      int ret = nl_talk(nl, &r);
      /* Deleting a primary IPv4 address takes its secondaries with it. */
      if (ret < 0 && ret != -EADDRNOTAVAIL && ret != -ENOENT)
        return ret;
      done++;
    }
    if (n < 64 || !done)
      break;
  }
  return 0;
}
