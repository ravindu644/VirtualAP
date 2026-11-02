/* From Droidspaces-OSS src/net/netlink.c (its core), with the six copies of
 * the dump loop folded into nl_dump.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/nl.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define NL_BUFSIZE 16384

#define NLMSG_TAIL(n)                                                          \
  ((struct rtattr *)(((uint8_t *)(n)) + NLMSG_ALIGN((n)->nlmsg_len)))

int nl_open(struct nl *nl) {
  nl->seq = 1;
  nl->fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
  if (nl->fd < 0)
    return -errno;
  struct sockaddr_nl sa = {.nl_family = AF_NETLINK};
  if (bind(nl->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
    int e = -errno;
    close(nl->fd);
    nl->fd = -1;
    return e;
  }
  return 0;
}

void nl_close(struct nl *nl) {
  if (nl->fd >= 0)
    close(nl->fd);
  nl->fd = -1;
}

void *nl_req_init(struct nl_req *r, int type, int flags, size_t hdr_len) {
  memset(r, 0, sizeof(*r));
  r->n.nlmsg_len = NLMSG_LENGTH(hdr_len);
  r->n.nlmsg_type = (uint16_t)type;
  r->n.nlmsg_flags = (uint16_t)(NLM_F_REQUEST | flags);
  return NLMSG_DATA(&r->n);
}

__attribute__((noinline)) struct rtattr *nl_attr(struct nl_req *r, int type,
                                                 const void *data, int len) {
  int alen = RTA_LENGTH(len);
  if (NLMSG_ALIGN(r->n.nlmsg_len) + RTA_ALIGN(alen) > sizeof(*r))
    return NULL;
  struct rtattr *rta = NLMSG_TAIL(&r->n);
  rta->rta_type = (unsigned short)type;
  rta->rta_len = (unsigned short)alen;
  if (len && data)
    memcpy(RTA_DATA(rta), data, (size_t)len);
  r->n.nlmsg_len = NLMSG_ALIGN(r->n.nlmsg_len) + RTA_ALIGN(alen);
  return rta;
}

struct rtattr *nl_attr_u32(struct nl_req *r, int type, uint32_t v) {
  return nl_attr(r, type, &v, sizeof(v));
}

struct rtattr *nl_attr_str(struct nl_req *r, int type, const char *s) {
  return nl_attr(r, type, s, (int)strlen(s) + 1);
}

struct rtattr *nl_nest_begin(struct nl_req *r, int type) {
  return nl_attr(r, type, NULL, 0);
}

void nl_nest_end(struct nl_req *r, struct rtattr *nest) {
  if (nest)
    nest->rta_len =
        (unsigned short)((uint8_t *)NLMSG_TAIL(&r->n) - (uint8_t *)nest);
}

void *nl_reserve(struct nl_req *r, size_t len) {
  size_t off = NLMSG_ALIGN(r->n.nlmsg_len);
  if (off + len > sizeof(*r))
    return NULL;
  r->n.nlmsg_len = (uint32_t)(off + len);
  return (uint8_t *)&r->n + off;
}

void nl_parse(struct rtattr *tb[], int max, struct rtattr *rta, int len) {
  memset(tb, 0, sizeof(*tb) * (size_t)(max + 1));
  for (; RTA_OK(rta, len); rta = RTA_NEXT(rta, len))
    if (rta->rta_type <= max)
      tb[rta->rta_type] = rta;
}

uint32_t nl_u32(const struct rtattr *rta) {
  uint32_t v = 0;
  if (rta && RTA_PAYLOAD(rta) >= sizeof(v))
    memcpy(&v, RTA_DATA(rta), sizeof(v));
  return v;
}

static int nl_send(struct nl *nl, struct nl_req *r) {
  r->n.nlmsg_seq = ++nl->seq;
  struct sockaddr_nl sa = {.nl_family = AF_NETLINK};
  if (sendto(nl->fd, &r->n, r->n.nlmsg_len, 0, (struct sockaddr *)&sa,
             sizeof(sa)) < 0)
    return -errno;
  return 0;
}

/* Reads until the reply to r ends: an ACK or error, a dump's DONE, or a
 * single non-multipart reply. Messages for other sequence numbers (a late
 * ACK from an earlier request) are skipped, never taken for ours. */
static int nl_recv(struct nl *nl, struct nl_req *r,
                   void (*cb)(const struct nlmsghdr *, void *), void *arg) {
  uint8_t buf[NL_BUFSIZE];
  int ret = 0;
  for (;;) {
    ssize_t n = recv(nl->fd, buf, sizeof(buf), 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      return -errno;
    }
    struct nlmsghdr *h = (struct nlmsghdr *)buf;
    for (; NLMSG_OK(h, (uint32_t)n); h = NLMSG_NEXT(h, n)) {
      if (h->nlmsg_seq != r->n.nlmsg_seq)
        continue;
      if (h->nlmsg_type == NLMSG_ERROR) {
        const struct nlmsgerr *e = NLMSG_DATA(h);
        return e->error ? e->error : ret;
      }
      if (h->nlmsg_flags & NLM_F_DUMP_INTR)
        ret = -EINTR;
      if (h->nlmsg_type == NLMSG_DONE) {
        /* A dump the kernel had to cut short says why in DONE's payload. */
        int err = 0;
        if (h->nlmsg_len >= NLMSG_LENGTH(sizeof(err)))
          memcpy(&err, NLMSG_DATA(h), sizeof(err));
        return err < 0 ? err : ret;
      }
      if (cb)
        cb(h, arg);
      if (!(h->nlmsg_flags & NLM_F_MULTI))
        return ret;
    }
  }
}

int nl_talk(struct nl *nl, struct nl_req *r) {
  r->n.nlmsg_flags |= NLM_F_ACK;
  int ret = nl_send(nl, r);
  return ret < 0 ? ret : nl_recv(nl, r, NULL, NULL);
}

int nl_get(struct nl *nl, struct nl_req *r,
           void (*cb)(const struct nlmsghdr *, void *), void *arg) {
  int ret = nl_send(nl, r);
  return ret < 0 ? ret : nl_recv(nl, r, cb, arg);
}

int nl_dump(struct nl *nl, struct nl_req *r,
            void (*cb)(const struct nlmsghdr *, void *), void *arg) {
  r->n.nlmsg_flags |= NLM_F_DUMP;
  int ret = nl_send(nl, r);
  return ret < 0 ? ret : nl_recv(nl, r, cb, arg);
}
