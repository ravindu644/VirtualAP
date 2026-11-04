/* The kernel's networking over rtnetlink, and namespaces. The ip_tables
 * engine has net/ipt/ipt.h.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NET_H
#define VAP_NET_H

#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* nl.c: rtnetlink transport: the socket, requests, dumps and attribute
 * helpers that link, addr, route, rule and neigh build their messages with. */

/* A NETLINK_ROUTE socket. It stays bound to the network namespace it was
 * opened in, but the helpers resolve interface names with if_nametoindex,
 * which asks the calling thread's namespace: a socket for a container is
 * opened and used while the thread is inside it, then closed before the
 * thread steps back. */
struct nl {
  int fd;
  uint32_t seq;
};

/* A NETLINK_ROUTE socket. 0, or -errno. */
int nl_open(struct nl *nl);
/* The same for another netlink protocol (NETLINK_GENERIC for nl80211). */
int nl_open_proto(struct nl *nl, int proto);
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

/* nl.c: Working in a container's network namespace from the host. */

/* /proc/<pid>/ns/net, opened once and used for every step, so a pid reused
 * in between can never redirect one. -EINVAL when it is our own namespace
 * (a container on the host's network has no veth to give), -errno else. */
int netns_open(pid_t pid);
/* The inode of pid's network namespace, 0 when pid is gone. It names the
 * namespace while that lives; a freed one's inode can be handed out again. */
ino_t netns_ino(pid_t pid);
/* fn(arg) with this thread inside fd's namespace, then back. setns moves the
 * calling thread only, so the engine stays single threaded. fn's return, or
 * -errno when the namespace could not be entered. Not coming back is fatal:
 * the rest of the run would act on the container. */
int in_netns(int fd, int (*fn)(void *arg), void *arg);

/* link.c: Network interfaces: create, delete, configure. */

/* Every call returns 0 or -errno, as the kernel answered. */
int link_set_up(struct nl *nl, const char *ifname, bool up);
/* 0 also when it was already gone. */
int link_del(struct nl *nl, const char *ifname);
/* master NULL releases the link from its bridge. */
int link_set_master(struct nl *nl, const char *ifname, const char *master);
int link_add_bridge(struct nl *nl, const char *name);
/* A veth pair, both ends at mtu (0 = the default). With netns_fd >= 0 the peer
 * is born inside that namespace, already under its final name and with
 * peer_mac if given: a link created on the host and moved in afterwards shows
 * up there under its temporary name first on kernels before 5.x, and
 * OpenWrt's netifd does not look at it again after the rename. */
int link_add_veth(struct nl *nl, const char *host, const char *peer,
                  int netns_fd, const uint8_t *peer_mac, unsigned mtu);
/* The kernel's permanent address (IFLA_PERM_ADDRESS, 5.5+) as aa:bb:..,
 * -ENOENT when the driver reports none. */
int link_permaddr(struct nl *nl, const char *ifname, char out[18]);

/* link.c: Interface addresses. */

/* Add (or refresh) addr/prefix on ifname. nodad skips duplicate address
 * detection, for our own router address on a link we own: waiting it out
 * would leave the address unusable for the first second. 0 or -errno. */
int addr_add(struct nl *nl, const char *ifname, int family, const char *addr,
             int prefix, bool nodad);

struct addr_info {
  int ifindex;
  int family;
  int prefix;
  int scope;      /* RT_SCOPE_UNIVERSE for global */
  uint32_t flags; /* IFA_F_DEPRECATED, IFA_F_TENTATIVE... */
  char addr[INET6_ADDRSTRLEN];
};
/* Every address of family (AF_UNSPEC: both) on ifindex (0: every interface),
 * in the kernel's order, which puts an interface's primary IPv4 address first.
 * Returns how many were stored in out, or -errno. */
int addr_list(struct nl *nl, int family, int ifindex, struct addr_info *out,
              int max);

/* Remove the addresses of family (AF_UNSPEC: all) on ifname, only the global
 * ones when global_only. 0 or -errno. */
int addr_flush(struct nl *nl, const char *ifname, int family, bool global_only);

/* link.c: NDP proxy entries: the upstream router finds a client behind the
 * phone. */

/* `ip -6 neigh replace|del proxy addr dev dev`. 0 or -errno; deleting one
 * that is not there is 0. */
int neigh_proxy(struct nl *nl, bool add, const char *addr, const char *dev);
/* Is addr in dev's neighbour table (a real, non-proxy entry)? */
bool neigh_known(struct nl *nl, const char *addr, const char *dev);

/* route.c: Routes and the routing-table lookups the upstream is resolved
 * with. */

struct route {
  int family;
  const char *dst; /* "a.b.c.d/len" or "x::/len"; NULL for the default route */
  const char *dev; /* the outgoing interface */
  const char *src; /* preferred source address, NULL for none */
  int table;
  int proto; /* RTPROT_*, 0 for RTPROT_BOOT as ip(8) uses */
  int scope; /* RT_SCOPE_LINK for a connected route */
};

/* `ip route replace`: adds or overwrites, so a restart never stacks
 * duplicates. 0 or -errno. */
int route_replace(struct nl *nl, const struct route *rt);
/* 0 also when there was no such route. */
int route_del(struct nl *nl, const struct route *rt);

/* The interface of the default route in table, the lowest metric when
 * several, the tie-break the kernel applies. On an IPv6-only mobile network
 * the IPv4 default in the default network's table is the CLAT interface
 * (v4-rmnet_dataX), which is what IPv4 forwarding needs. 0 or -ENOENT. */
int route_default_oif(struct nl *nl, int family, int table,
                      char ifname[IF_NAMESIZE]);

/* route.c: Policy routing rules. */

struct rule {
  int family;
  const char *from; /* "addr/len", NULL for all */
  const char *to;   /* "addr/len", NULL for all */
  const char *iif;  /* NULL for any */
  const char *oif;  /* NULL for any */
  int table;        /* the table looked up, unless blackhole */
  int prio;
  bool blackhole;
};

/* Adds the rule unless an identical one is already there. Kernels before
 * 4.12 ignore NLM_F_EXCL on rules and would stack a second copy, so the dump
 * is asked first. 0 or -errno. */
int rule_add(struct nl *nl, const struct rule *ru);
bool rule_exists(struct nl *nl, const struct rule *ru);
/* Every rule of family at prio, whoever added it. 0 or -errno. */
int rule_del_prio(struct nl *nl, int family, int prio);

/* route.c: Which network the hotspot's traffic leaves through, as Android's
 * netd sees it. */

struct upstream {
  int table; /* 0: none, the network has not arrived */
  char iface[IF_NAMESIZE];
};
/* want is "auto" or -o's comma-separated list. 0, or -1 with up zeroed when
 * nothing qualifies, which is a normal state: the start goes on without one and
 * the monitor wires it when one appears. Logs a change, and warns once about
 * having none. */
int upstream_resolve(struct nl *nl, const char *want, struct upstream *up);

/* The interface IPv6 leaves through: on an IPv6-only network the IPv4
 * upstream is the CLAT interface v4-<iface>, which carries no IPv6. */
const char *upstream_v6_iface(const struct upstream *up);

#endif
