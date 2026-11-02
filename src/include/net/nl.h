/* rtnetlink transport: the socket, requests, dumps and attribute helpers that
 * link, addr, route, rule and neigh build their messages with.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NL_H
#define VAP_NL_H

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stddef.h>
#include <stdint.h>

/* A NETLINK_ROUTE socket. It stays bound to the network namespace it was
 * opened in, but the helpers resolve interface names with if_nametoindex,
 * which asks the calling thread's namespace: a socket for a container is
 * opened and used while the thread is inside it, then closed before the
 * thread steps back. */
struct nl {
  int fd;
  uint32_t seq;
};

/* 0, or -errno. */
int nl_open(struct nl *nl);
void nl_close(struct nl *nl);

/* A request with room for its attributes. */
struct nl_req {
  struct nlmsghdr n;
  char body[1024];
};
/* Header and the family header (ifinfomsg, rtmsg...) of hdr_len bytes,
 * zeroed. Returns that family header. */
void *nl_req_init(struct nl_req *r, int type, int flags, size_t hdr_len);

/* Out of line on purpose: once LTO inlines it into a builder whose sizes fold
 * to constants, GCC 14 takes n for the 16-byte header rather than the request
 * and reports every memcpy as an overflow (Droidspaces' note). NULL when the
 * request is full; the kernel then rejects the short message. */
struct rtattr *nl_attr(struct nl_req *r, int type, const void *data, int len);
struct rtattr *nl_attr_u32(struct nl_req *r, int type, uint32_t v);
struct rtattr *nl_attr_str(struct nl_req *r, int type, const char *s);
struct rtattr *nl_nest_begin(struct nl_req *r, int type);
void nl_nest_end(struct nl_req *r, struct rtattr *nest);
/* The raw bytes after the current end, for VETH_INFO_PEER's embedded
 * ifinfomsg. NULL when it does not fit. */
void *nl_reserve(struct nl_req *r, size_t len);

/* tb[type] = the attribute, for every type <= max in [rta, rta+len). */
void nl_parse(struct rtattr *tb[], int max, struct rtattr *rta, int len);
/* A 4-byte payload read safely: 0 when the attribute is short. */
uint32_t nl_u32(const struct rtattr *rta);

/* Send a request that wants an ACK. 0 or the kernel's -errno. */
int nl_talk(struct nl *nl, struct nl_req *r);
/* Send a dump request and call cb on every reply message, to the end of the
 * dump, so the next request never reads a leftover of this one. cb must not
 * send on the same socket: collect, then act. 0, the error the kernel ended
 * the dump with, or -EINTR when the tables changed under it (netd rewrites
 * them on every network switch) and what cb saw may be inconsistent. */
int nl_dump(struct nl *nl, struct nl_req *r,
            void (*cb)(const struct nlmsghdr *h, void *arg), void *arg);
/* A single GET (one link by index): cb sees the one reply. A missing object
 * comes back as the kernel's -errno. */
int nl_get(struct nl *nl, struct nl_req *r,
           void (*cb)(const struct nlmsghdr *h, void *arg), void *arg);

#endif
