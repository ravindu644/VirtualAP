/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "radio/genl.h"

#include <errno.h>
#include <string.h>

void genl_req(struct nl_req *r, int family_id, int cmd, int flags) {
  struct genlmsghdr *g = nl_req_init(r, family_id, flags, GENL_HDRLEN);
  g->cmd = (uint8_t)cmd;
  g->version = 1;
}

void genl_parse(struct rtattr *tb[], int max, const struct nlmsghdr *h) {
  nl_parse(tb, max,
           (struct rtattr *)((char *)NLMSG_DATA(h) + GENL_HDRLEN),
           (int)(h->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN)));
}

static void id_cb(const struct nlmsghdr *h, void *arg) {
  struct rtattr *tb[CTRL_ATTR_MAX + 1];
  genl_parse(tb, CTRL_ATTR_MAX, h);
  if (tb[CTRL_ATTR_FAMILY_ID] && RTA_PAYLOAD(tb[CTRL_ATTR_FAMILY_ID]) >= 2) {
    uint16_t id;
    memcpy(&id, RTA_DATA(tb[CTRL_ATTR_FAMILY_ID]), sizeof(id));
    *(int *)arg = id;
  }
}

int genl_open(struct nl *nl, const char *name, int *family_id) {
  int ret = nl_open_proto(nl, NETLINK_GENERIC);
  if (ret < 0)
    return ret;
  struct nl_req r;
  genl_req(&r, GENL_ID_CTRL, CTRL_CMD_GETFAMILY, 0);
  nl_attr_str(&r, CTRL_ATTR_FAMILY_NAME, name);
  int id = 0;
  ret = nl_get(nl, &r, id_cb, &id);
  if (ret == 0 && !id)
    ret = -ENOENT;
  if (ret < 0) {
    nl_close(nl);
    return ret;
  }
  *family_id = id;
  return 0;
}
