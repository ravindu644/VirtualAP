/* Generic netlink on top of the nl transport: family lookup and requests.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_GENL_H
#define VAP_GENL_H

#include <linux/genetlink.h>

#include "net/net.h"

/* A NETLINK_GENERIC socket with the id of family `name` resolved. 0, or
 * -errno (-ENOENT when the kernel has no such family). */
int genl_open(struct nl *nl, const char *name, int *family_id);
/* Start a request: nlmsghdr + genlmsghdr for cmd. */
void genl_req(struct nl_req *r, int family_id, int cmd, int flags);
/* The attributes of a generic netlink reply. */
void genl_parse(struct rtattr *tb[], int max, const struct nlmsghdr *h);

#endif
