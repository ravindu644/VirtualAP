/* What mode/'s own files share, and the tests check: not part of its
 * public header. Included as "internal.h", or by path from tests/.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MODE_INTERNAL_H
#define VAP_MODE_INTERNAL_H

#include "mode/mode.h"

/* ap.c: Printable ASCII with no blanks, all a name may be before it reaches the
 * app or a file of ours. */
bool name_printable(const char *s);

/* Android's local_network table, constant since Android 5. The AP subnet is
 * mirrored there so container replies (from Droidspaces' 172.28.0.0/16) hit
 * its rule 6095 instead of leaking out the WAN via 6100. */
#define ANDROID_LOCAL_NETWORK_TABLE 97

/* Droidspaces' NAT subnets: in managed mode the clients' traffic reaches the
 * host from the container's address in these. */
#define DS_NAT_NET "172.28.0.0/16"
#define DS_NAT_NET6 "fd64:7370::/48"

/* Managed mode names, distinct from Droidspaces' own ds-br0/ds-lan. */
#define VAP_BRIDGE "vap-br0"
#define VAP_VETH_HOST "vap-v0"
#define VAP_PORT "vaplan0"
/* Broadcom's FullMAC firmware hands up about a quarter of received frames with
 * the 8-byte CCMP MIC still on, so a full-size client frame is 1522 bytes. IP
 * trims that, a bridge and a veth silently drop anything over MTU+18. The veth
 * takes 1508 so the MIC fits; clients still see 1500. */
#define VAP_MTU 1508

/* Droidspaces is only ever run from here, never looked up in PATH. The app's
 * copy is Constants.DROIDSPACES. */
#define DROIDSPACES_BIN "/data/local/Droidspaces/bin/droidspaces"

/* managed.c: ds_read_leases with the container's root as a path, and the
 * filesystem the file must be on as statfs names it. */
int leases_read_under(const char *root, long fs_type, char *buf, size_t size);

/* ipv6.c: NAT66 the Droidspaces way: a private /64 on ap0, masqueraded out
 * of whatever the upstream is. Survives an upstream or prefix change
 * untouched. */

/* v->net, prefix and gw, from the gateway's third octet. */
void nat66_prefix(const struct subnet *net, struct v6 *v);

/* ipv6.c: The upstream's own /64 shared with the clients, what Android's
 * tethering does: ap0 advertises it off-link, every client address is routed
 * as a /128 to ap0 and answered for on the upstream with an NDP proxy entry.
 * No NAT, so no ip6table_nat, which stock GKI kernels lack. */

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

/* managed.c: Receive packet steering on the managed-mode veth. */

/* The mask written, every CPU but CPU0, 32-bit words most significant
 * first: "fe" for 0-7, "ff,fffffffe" for 0-39, "" for one CPU or a buffer too
 * short. RPS_MASK_MAX holds the widest, 1024 CPUs. */
#define RPS_MASK_MAX 300
/* The same from the text of /sys/devices/system/cpu/present, for the tests. */
void rps_mask_from(const char *present, char *out, size_t size);

#endif
