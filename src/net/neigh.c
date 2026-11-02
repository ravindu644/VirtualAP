/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/neigh.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/neighbour.h>
#include <net/if.h>
#include <string.h>

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
  nl_parse(tb, NDA_MAX, (struct rtattr *)((char *)nd + NLMSG_ALIGN(sizeof(*nd))),
           (int)(h->nlmsg_len - NLMSG_LENGTH(sizeof(*nd))));
  if (!(nd->ndm_flags & NTF_PROXY) && tb[NDA_DST] &&
      RTA_PAYLOAD(tb[NDA_DST]) == 16 && !memcmp(RTA_DATA(tb[NDA_DST]), k->want, 16))
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
