/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/neigh.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/neighbour.h>
#include <net/if.h>

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
