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

#endif
