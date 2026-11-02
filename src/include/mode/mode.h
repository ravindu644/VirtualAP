/* The two kinds of AP and what they share: ap0, the daemons, routed mode,
 * the clients' IPv6 and managed mode.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MODE_H
#define VAP_MODE_H

#include <arpa/inet.h>
#include <net/if.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#include "cli/cli.h"
#include "core/core.h"
#include "net/net.h"
#include "radio/radio.h"
#include "virtualap.h"

/* ipv6.c: IPv6 for the clients in routed mode: NAT66 on a private prefix
 * where the kernel can masquerade IPv6, else a share of the upstream's own
 * /64, else none. Never fatal: on any problem the AP stays IPv4 only. */

enum { V6_OFF, V6_NAT66, V6_SHARED };

struct v6 {
  int mode;
  char net[INET6_ADDRSTRLEN + 8]; /* "x::/64" */
  char prefix[INET6_ADDRSTRLEN];  /* "x::", what dnsmasq advertises */
  char gw[INET6_ADDRSTRLEN + 2];  /* ap0's own address in it */
  char up[IF_NAMESIZE];           /* the IPv6 upstream interface */
  char prev_fwd[8], prev_proxy[8];
};

/* What the clients should get now, touching nothing: v->mode, v->up and the
 * prefix filled, upaddr the upstream's global address. Returns why IPv6 is
 * off when it is (NULL when -6 0 asked for that). */
const char *ipv6_decide(struct nl *nl, const struct vap_opts *o,
                        const struct upstream *up, const struct subnet *net,
                        struct v6 *v, struct in6_addr *upaddr);
void ipv6_setup(struct nl *nl, const struct vap_opts *o,
                const struct upstream *up, const struct subnet *net,
                struct v6 *v);
void ipv6_save(const struct v6 *v, struct state *st);
/* Undo whatever st says was set up, tolerant of half a setup, and of a
 * run.state with no IPv6 keys at all (v2.1.0's shell engine had none). */
void ipv6_teardown(struct nl *nl, const struct state *st);

/* The IPv6 upstream's accept_ra at 2, which it needs once forwarding is on
 * to go on taking RAs, and with them its address and default route. */
void ipv6_keep_ra(const char *iface);

/* /proc/sys/net/ipv6/conf/<iface>/<key>. */
int conf6_set(const char *iface, const char *key, const char *val);
void conf6_get(const char *iface, const char *key, char *out, size_t size);

/* ipv6.c: Finding the clients' global addresses in a shared /64. */

/* The source is an echo to all-nodes from ap0's global address: every
 * client answers from its own global one, and nothing else reveals it. The
 * prefix is off-link, so a client solicits only the router's link-local and
 * ap0's neighbour table never sees the address. */

/* A raw ICMPv6 socket on ap0, from v's address, echo replies only. -1 when
 * it cannot be had. */
int v6disc_open(const struct v6 *v);
void v6disc_ping(int fd);
/* Every reply waiting: a new address in v's /64 gets its /128 and proxy
 * entry (shared_add_client). Routes and proxy entries stay for the AP's
 * lifetime: a /128 for a client that left only drops traffic to it, while
 * dropping the entry of a dozing client that missed a round would cut it
 * off. Only replies to our own echo id count, and never for an address that
 * is the phone's own or a neighbour on the upstream. */
void v6disc_read(int fd, struct nl *nl, const struct v6 *v);
/* Forget every address, for a new /64. */
void v6disc_reset(void);
/* Once a minute: drop the addresses that did not answer since the last
 * refresh (rotated privacy addresses, clients gone), and have the rest
 * re-installed on their next answer, since an ap0 that went down and up took
 * every /128 with it. */
void v6disc_refresh(void);

/* ap.c: hostapd: its config, its start, and what status asks it. */

#define HOSTAPD_CONF VAP_RUN "/hostapd.conf"
#define HOSTAPD_PID VAP_RUN "/hostapd.pid"
#define HOSTAPD_CTRL VAP_RUN "/hostapd"

/* hostapd's own station list (MediaTek's gen4m has no dump_station, so the
 * driver's is always empty there). 0 when hostapd does not answer within 1 s:
 * a hung hostapd must not stall the status the app polls every 3 s. */
int hostapd_clients(void);

/* ap.c: dnsmasq: DHCP and DNS for the clients, and the IPv6 router
 * advertisements. */

#define DNSMASQ_CONF VAP_RUN "/dnsmasq.conf"
#define DNSMASQ_PID VAP_RUN "/dnsmasq.pid"

/* The router advertisement for the clients' IPv6, NULL for IPv4 only. */
struct ra {
  const char *prefix; /* "x:y:z:w::", the /64 advertised */
  const char *dns;    /* our own address in it, the RDNSS */
  bool off_link;      /* shared /64: clients send everything to us */
};

/* Write the conf and start dnsmasq. Its parent exits only once the daemon has
 * written its pid file and is serving, so a zero exit is the liveness check.
 * 0, or -1 after logging why. */
int dnsmasq_start(const struct subnet *net, const char *dns,
                  const struct ra *ra);

/* ap.c: What both modes share: ap0 itself and the daemons on it. */

struct state;
struct subnet;

/* hostapd and dnsmasq, each only if the pid in its file is really it, and
 * their generated configs. */
void ap_daemons_stop(void);
/* The static binaries the engine runs. 0, or -1 after naming the missing. */
int ap_prerequisites(void);

/* routed.c: The policy rules of a routed AP, added by the setup and the
 * monitor and removed by the teardowns, all from here so the three never
 * disagree. */

/* IPv4's when net is set: replies to net via main (7000), everything from
 * ap0 via the upstream's table (7010). IPv6's when v has a mode: the same for
 * v's /64, ap0's link-local routes via main (7002), and for a shared /64 the
 * sink of what no client holds (7001). Each added unless already there;
 * with verbose a failure is logged with its priority (the monitor, which
 * retries every beat, asks quietly). 0, or -1 when any failed (every one is
 * still tried). */
int rules_add(struct nl *nl, const struct upstream *up,
              const struct subnet *net, const struct v6 *v, bool verbose);
/* Only the rules that name the upstream's table (PRIO_FROM_AP), both
 * families, a failure logged: an upstream switch moves those and leaves the
 * clients' return path in place. */
void rules_del_upstream(struct nl *nl);

/* routed.c: Routed mode: ap0 is the clients' gateway, the phone NATs them
 * out of the upstream netd picked (or -o named). */

/* What a running routed AP was set up with, for the monitor to keep right. */
struct routed {
  struct upstream up;
  struct subnet net;
  struct v6 v6;
};

/* The whole start after the settings were validated and saved, lan the
 * subnet of the gateway start checked; r filled on success. 0, or -1 after
 * logging why and tearing down what was set up. */
int routed_start(struct vap_opts *o, const struct subnet *lan,
                 struct routed *r);
/* Undo a routed AP from run.state (or what the settings say, when there is
 * none), every step tolerant of what is already gone. */
void routed_teardown(const struct vap_opts *o);

/* managed.c: Droidspaces containers, as managed mode needs them. */

/* The running container's init as `droidspaces -n name pid` reports it, 0
 * when it is not running. */
pid_t ds_container_pid(const char *name);

/* managed.c: Receive packet steering on the managed-mode veth. */

/* A veth queues each packet on the CPU that sent it, and one flow arrives
 * from several: the Wi-Fi driver spreads its receive work, the container's
 * TCP sends from whichever core took the last ACK. The far end then sees the
 * flow out of order and retransmits what was never lost. RPS hashes each
 * flow to one CPU: every CPU but CPU0, the mask Droidspaces uses on its own
 * veths. Best effort; true when both ends took it. */

/* managed.c: Managed mode: ap0 is a port on a bridge whose other port is a
 * veth into a Droidspaces container, which owns all L3 (DHCP, DNS, NAT). */

/* The whole managed start after the settings were validated and saved, lan
 * the subnet of the gateway start checked. 0, or -1 after logging why and
 * tearing down what was set up. */
int managed_start(const struct vap_opts *o, const struct subnet *lan);
/* Whether the port is still in the container: the host end is there and the
 * process it was plugged into is still in that namespace. */
bool managed_plugged(void);
/* The container restarted as pid: a new veth goes into its namespace,
 * replacing a host end left in the old one. Its OpenWrt LAN config survived
 * the restart and picks the port up again. */
int managed_replug(const struct vap_opts *o, pid_t pid);
/* Undo a managed AP, tolerant of what is already gone. Deleting the veth
 * takes the container's end with it; its OpenWrt LAN config stays. */
void managed_teardown(const char *container);

#endif
