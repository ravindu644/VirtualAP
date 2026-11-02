/* From Droidspaces-OSS src/net/netlink.c (links, bridge, veth).
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/link.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_addr.h>
#include <linux/if_link.h>
#include <linux/neighbour.h>
#include <linux/veth.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>

#include "net/addr.h"
#include "net/neigh.h"

/* RTM_NEWLINK on an existing link, by index. */
static struct ifinfomsg *change(struct nl_req *r, const char *ifname,
                                int *err) {
  unsigned idx = if_nametoindex(ifname);
  if (!idx) {
    *err = -ENODEV;
    return NULL;
  }
  struct ifinfomsg *i = nl_req_init(r, RTM_NEWLINK, 0, sizeof(*i));
  i->ifi_family = AF_UNSPEC;
  i->ifi_index = (int)idx;
  *err = 0;
  return i;
}

int link_set_up(struct nl *nl, const char *ifname, bool up) {
  struct nl_req r;
  int err;
  struct ifinfomsg *i = change(&r, ifname, &err);
  if (!i)
    return err;
  i->ifi_flags = up ? IFF_UP : 0;
  i->ifi_change = IFF_UP;
  return nl_talk(nl, &r);
}

int link_del(struct nl *nl, const char *ifname) {
  struct nl_req r;
  int err;
  if (!change(&r, ifname, &err))
    return 0;
  r.n.nlmsg_type = RTM_DELLINK;
  err = nl_talk(nl, &r);
  return err == -ENODEV ? 0 : err;
}

int link_set_master(struct nl *nl, const char *ifname, const char *master) {
  uint32_t midx = 0;
  if (master && !(midx = if_nametoindex(master)))
    return -ENODEV;
  struct nl_req r;
  int err;
  if (!change(&r, ifname, &err))
    return err;
  nl_attr_u32(&r, IFLA_MASTER, midx);
  return nl_talk(nl, &r);
}

/* RTM_NEWLINK creating `name` of `kind`; returns the IFLA_INFO_DATA nest
 * opened for the kind's own attributes when data is set. */
static struct rtattr *create(struct nl_req *r, const char *name,
                             const char *kind, unsigned mtu,
                             struct rtattr **linfo, int data) {
  struct ifinfomsg *i =
      nl_req_init(r, RTM_NEWLINK, NLM_F_CREATE | NLM_F_EXCL, sizeof(*i));
  i->ifi_family = AF_UNSPEC;
  nl_attr_str(r, IFLA_IFNAME, name);
  if (mtu)
    nl_attr_u32(r, IFLA_MTU, mtu);
  *linfo = nl_nest_begin(r, IFLA_LINKINFO);
  nl_attr_str(r, IFLA_INFO_KIND, kind);
  return data ? nl_nest_begin(r, IFLA_INFO_DATA) : NULL;
}

int link_add_bridge(struct nl *nl, const char *name) {
  struct nl_req r;
  struct rtattr *linfo;
  create(&r, name, "bridge", 0, &linfo, 0);
  nl_nest_end(&r, linfo);
  return nl_talk(nl, &r);
}

/* VETH_INFO_PEER holds a whole ifinfomsg followed by the peer's own IFLA_*
 * attributes, the way iproute2's link_veth.c builds it. */
int link_add_veth(struct nl *nl, const char *host, const char *peer,
                  int netns_fd, const uint8_t *peer_mac, unsigned mtu) {
  struct nl_req r;
  struct rtattr *linfo;
  struct rtattr *data = create(&r, host, "veth", mtu, &linfo, 1);
  struct rtattr *p = nl_nest_begin(&r, VETH_INFO_PEER);
  struct ifinfomsg *pi = nl_reserve(&r, sizeof(*pi));
  if (!pi)
    return -ENOSPC;
  memset(pi, 0, sizeof(*pi));
  pi->ifi_family = AF_UNSPEC;
  nl_attr_str(&r, IFLA_IFNAME, peer);
  if (mtu)
    nl_attr_u32(&r, IFLA_MTU, mtu);
  if (netns_fd >= 0)
    nl_attr_u32(&r, IFLA_NET_NS_FD, (uint32_t)netns_fd);
  if (peer_mac)
    nl_attr(&r, IFLA_ADDRESS, peer_mac, 6);
  nl_nest_end(&r, p);
  nl_nest_end(&r, data);
  nl_nest_end(&r, linfo);
  return nl_talk(nl, &r);
}

#ifndef IFLA_PERM_ADDRESS
#define IFLA_PERM_ADDRESS 54
#endif

struct perm {
  char *out;
  int found;
};

static void perm_cb(const struct nlmsghdr *h, void *arg) {
  struct perm *p = arg;
  struct rtattr *tb[IFLA_PERM_ADDRESS + 1];
  if (h->nlmsg_type != RTM_NEWLINK)
    return;
  nl_parse(tb, IFLA_PERM_ADDRESS, IFLA_RTA(NLMSG_DATA(h)),
           (int)IFLA_PAYLOAD(h));
  const struct rtattr *a = tb[IFLA_PERM_ADDRESS];
  if (!a || RTA_PAYLOAD(a) != 6)
    return;
  const uint8_t *m = RTA_DATA(a);
  if (!(m[0] | m[1] | m[2] | m[3] | m[4] | m[5]))
    return;
  snprintf(p->out, 18, "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3],
           m[4], m[5]);
  p->found = 1;
}

int link_permaddr(struct nl *nl, const char *ifname, char out[18]) {
  struct nl_req r;
  int err;
  if (!change(&r, ifname, &err))
    return err;
  r.n.nlmsg_type = RTM_GETLINK;
  struct perm p = {out, 0};
  err = nl_get(nl, &r, perm_cb, &p);
  return err < 0 ? err : p.found ? 0 : -ENOENT;
}

/* Addresses: addr_add is from Droidspaces-OSS src/net/netlink.c; list and
 * flush are new. */
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

int neigh_proxy(struct nl *nl, bool add, const char *addr, const char *dev) {
  unsigned char bin[16];
  unsigned idx = if_nametoindex(dev);
  if (!idx)
    return add ? -ENODEV : 0;
  if (inet_pton(AF_INET6, addr, bin) != 1)
    return -EINVAL;
  struct nl_req r;
  struct ndmsg *nd =
      nl_req_init(&r, add ? RTM_NEWNEIGH : RTM_DELNEIGH,
                  add ? NLM_F_CREATE | NLM_F_REPLACE : 0, sizeof(*nd));
  nd->ndm_family = AF_INET6;
  nd->ndm_ifindex = (int)idx;
  nd->ndm_flags = NTF_PROXY;
  nd->ndm_state = NUD_PERMANENT;
  nl_attr(&r, NDA_DST, bin, 16);
  int ret = nl_talk(nl, &r);
  return !add && ret == -ENOENT ? 0 : ret;
}

struct seek {
  unsigned char want[16];
  bool found;
};

static void seek_cb(const struct nlmsghdr *h, void *arg) {
  struct seek *k = arg;
  if (h->nlmsg_type != RTM_NEWNEIGH)
    return;
  struct ndmsg *nd = NLMSG_DATA(h);
  struct rtattr *tb[NDA_MAX + 1];
  nl_parse(tb, NDA_MAX,
           (struct rtattr *)((char *)nd + NLMSG_ALIGN(sizeof(*nd))),
           (int)(h->nlmsg_len - NLMSG_LENGTH(sizeof(*nd))));
  if (!(nd->ndm_flags & NTF_PROXY) && tb[NDA_DST] &&
      RTA_PAYLOAD(tb[NDA_DST]) == 16 &&
      !memcmp(RTA_DATA(tb[NDA_DST]), k->want, 16))
    k->found = true;
}

bool neigh_known(struct nl *nl, const char *addr, const char *dev) {
  struct seek k = {.found = false};
  unsigned idx = if_nametoindex(dev);
  if (!idx || inet_pton(AF_INET6, addr, k.want) != 1)
    return false;
  struct nl_req r;
  struct ndmsg *nd = nl_req_init(&r, RTM_GETNEIGH, 0, sizeof(*nd));
  nd->ndm_family = AF_INET6;
  nd->ndm_ifindex = (int)idx;
  return nl_dump(nl, &r, seek_cb, &k) == 0 && k.found;
}
