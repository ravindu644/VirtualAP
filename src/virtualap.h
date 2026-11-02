/* VirtualAP engine: paths, names and numbers every layer shares.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VIRTUALAP_H
#define VIRTUALAP_H

#define VAP_BASE "/data/local/virtualap"
#define VAP_BIN VAP_BASE "/bin"
#define VAP_RUN VAP_BASE "/run"
#define VAP_CONF VAP_BASE "/ap.conf"
#define VAP_STATE VAP_BASE "/run.state"
#define VAP_LOG VAP_BASE "/logs/ap.log"
/* Per-daemon logs, so a crash leaves a trace: both daemonize and would
 * otherwise drop their output into the void. */
#define VAP_HOSTAPD_LOG VAP_BASE "/logs/hostapd.log"
#define VAP_DNSMASQ_LOG VAP_BASE "/logs/dnsmasq.log"

#define AP_IFACE "ap0"
#define GW_DEFAULT "192.168.42.1"
/* What dnsmasq forwards to when no -d is given. Android has no resolv.conf,
 * and the phone's own resolvers may sit behind a VPN or on another interface
 * that dnsmasq's queries never take, so public ones are the safe default. */
#define DNS_DEFAULT "1.1.1.1,8.8.8.8"

/* Pinned rule priorities: above netd's range (10000+), clear of Droidspaces'
 * 6090-6100. */
#define PRIO_TO_SUBNET 7000 /* to <subnet> lookup main (return path) */
#define PRIO_FROM_AP 7010   /* iif ap0 lookup <upstream table> */
/* IPv6 shared /64 only: a packet from the upstream for an address in the
 * shared /64 that no client holds has no /128 in main. Without this it takes
 * the default route back out and, on cellular where the PGW routes the whole
 * /64 to us, bounces until its hop limit dies. */
#define PRIO_V6_SINK 7001 /* iif <upstream> to <our /64> blackhole */
/* Android files an interface's own routes, fe80::/64 included, in table
 * 1000+ifindex, which no rule reads. The kernel's neighbour discovery bypasses
 * routing, dnsmasq's unicast RA to a soliciting client does not. */
#define PRIO_V6_LINK 7002 /* oif ap0 lookup main */

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

#define DROIDSPACES_BIN "/data/local/Droidspaces/bin/droidspaces"

#endif
