/* What mode/'s own files share, and the tests check: not part of its
 * public header. Included as "internal.h", or by path from tests/.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MODE_INTERNAL_H
#define VAP_MODE_INTERNAL_H

#include "mode/mode.h"

/* ipv6.c: IPv6 for the clients in routed mode: NAT66 on a private prefix
 * where the kernel can masquerade IPv6, else a share of the upstream's own
 * /64, else none. Never fatal: on any problem the AP stays IPv4 only. */

/* The first address on iface the internet routes back to: global scope, not
 * ULA (fc00::/7), neither deprecated nor tentative; a /64 only when only64.
 * Wi-Fi routers without IPv6 internet hand out ULAs alone. */
bool v6_global_addr(struct nl *nl, const char *iface, bool only64,
                    struct in6_addr *out);

/* ipv6.c: NAT66 the Droidspaces way: a private /64 on ap0, masqueraded out
 * of whatever the upstream is. Survives an upstream or prefix change
 * untouched. */

/* ip6table_nat with MASQUERADE, and an IPv6 default route in the upstream's
 * table to masquerade out of. */
bool nat66_possible(struct nl *nl, const struct upstream *up);
/* v->net, prefix and gw, from the gateway's third octet. */
void nat66_prefix(const struct subnet *net, struct v6 *v);
/* 0, or -1 with the reason in why. */
int nat66_setup(struct nl *nl, const struct upstream *up, struct v6 *v,
                const char **why);

/* ipv6.c: The upstream's own /64 shared with the clients, what Android's
 * tethering does: ap0 advertises it off-link, every client address is routed
 * as a /128 to ap0 and answered for on the upstream with an NDP proxy entry.
 * No NAT, so no ip6table_nat, which stock GKI kernels lack. */

/* v->prefix and net: upaddr's /64. */
void shared_prefix(const struct in6_addr *upaddr, struct v6 *v);
/* 0, or -1 with the reason in why. */
int shared_setup(struct nl *nl, const struct upstream *up,
                 const struct in6_addr *upaddr, struct v6 *v, const char **why);
/* A client address found on ap0: its /128 to ap0 and its proxy entry on the
 * upstream, recorded in run/v6clients for the teardown when record (a
 * re-install of a known one is not). */
int shared_add_client(struct nl *nl, const char *addr, const char *up,
                      bool record);

#define V6CLIENTS VAP_RUN "/v6clients"

/* ap.c: hostapd: its config, its start, and what status asks it. */

/* hostapd.conf for the plan. 0 or -1. */
int hostapd_write_conf(const char *path, const struct vap_opts *o,
                       const struct plan *p);
/* Write the conf, launch hostapd and confirm it is still up 2 s later. A
 * 40 MHz 2.4 GHz plan the firmware refuses is retried at 20 MHz (p->width is
 * updated). 0, or -1 after logging why. */
int hostapd_start(const struct vap_opts *o, struct plan *p);

/* ap.c: dnsmasq: DHCP and DNS for the clients, and the IPv6 router
 * advertisements. */

/* dns is the comma separated upstream list, blanks and empties dropped.
 * 0 or -1. */
int dnsmasq_write_conf(const char *path, const struct subnet *net,
                       const char *dns, const struct ra *ra);
/* True when dns holds at least one address. */
bool dns_any(const char *dns);

/* ap.c: What both modes share: ap0 itself and the daemons on it. */

/* Create ap0 on the physical Wi-Fi interface, after removing a stale one.
 * 0, or -1 after logging why. */
int ap_iface_create(const char *phy_iface);
/* nl80211's delete, the link delete when the driver will not. */
void ap_iface_del(void);
/* Band, channel and width for -b/-c/-W, the station and the chip, the 5 GHz
 * NO-IR fix included. */
void ap_plan_radio(const struct vap_opts *o, struct plan *p);
/* run.state with the keys both modes share added to st (band, channel, ssid,
 * gateway, started). 0, or -1 after logging why. */
int ap_state_save(struct state *st, const struct vap_opts *o,
                  const struct plan *p, const struct subnet *net);
/* The TTL fix as -T asks, cleared first so turning it off takes effect on a
 * restart. src4 and src6 (or NULL) are the forwarded traffic's sources. */
void ap_ttl(const struct vap_opts *o, const char *src4, const char *src6);

/* SIGINT and SIGTERM are held for the whole start and looked at between
 * steps, so a start told to stop always tears down and never leaves
 * half an AP. */
void ap_hold_signals(void);
bool ap_interrupted(void);

/* routed.c: The policy rules of a routed AP, added by the setup and the
 * monitor and removed by the teardowns, all from here so the three never
 * disagree. */

/* Every rule of family at the priorities rules_add uses, whoever added it. */
void rules_del(struct nl *nl, int family);

/* managed.c: Droidspaces containers, as managed mode needs them. */

/* script on sh's stdin inside the container, args as its $1..: values never
 * pass through a shell's quoting. line(text) for each line it prints. The
 * container's own namespaces, cgroup and limits apply, as droidspaces run
 * sets them. Exit code, or -1. */
int ds_run_script(const char *name, const char *script,
                  const char *const args[],
                  void (*line)(const char *text, void *arg), void *arg);
/* Its /etc/os-release says ID=openwrt (quoted or not). */
bool ds_is_openwrt(const char *name);

/* managed.c: An OpenWrt container's LAN for the hotspot, provisioned
 * automatically. */

/* A LAN on the container's VAP_PORT at the gateway's address, its DHCP and
 * router advertisements, and NAT out of its WAN. Best effort: what fails is
 * logged as a warning. */
void openwrt_provision(const char *container, const struct subnet *net);

/* managed.c: Receive packet steering on the managed-mode veth. */

/* The mask written, every CPU but CPU0, 32-bit words most significant
 * first: "fe" for 0-7, "ff,fffffffe" for 0-39, "" for one CPU or a buffer too
 * short. RPS_MASK_MAX holds the widest, 1024 CPUs. */
#define RPS_MASK_MAX 300
void rps_mask(char *out, size_t size);
/* The same from the text of /sys/devices/system/cpu/present, for the tests. */
void rps_mask_from(const char *present, char *out, size_t size);
/* mask on ifname in the host's namespace. */
bool rps_host(const char *ifname, const char *mask);
/* ifname in the namespace of netns_fd. Moving a link between namespaces
 * drops its map, and the host's /sys shows only the host's links, so a
 * child steps in and mounts a sysfs of its own for that namespace; never
 * the container's /proc/<pid>/root/sys, whose symlinks the container picks. */
bool rps_in_netns(int netns_fd, const char *ifname, const char *mask);

#endif
