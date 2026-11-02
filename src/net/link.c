/* From Droidspaces-OSS src/net/netlink.c (links, bridge, veth).
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/link.h"

#include <errno.h>
#include <linux/if_link.h>
#include <linux/veth.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>

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
