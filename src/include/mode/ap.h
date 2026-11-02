/* What both modes share: ap0 itself and the daemons on it.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_AP_H
#define VAP_AP_H

#include <stdbool.h>

#include "cli/options.h"
#include "radio/channel.h"

/* Create ap0 on the physical Wi-Fi interface, after removing a stale one.
 * 0, or -1 after logging why. */
int ap_iface_create(const char *phy_iface);
/* nl80211's delete, the link delete when the driver will not. */
void ap_iface_del(void);
/* hostapd and dnsmasq, each only if the pid in its file is really it, and
 * their generated configs. */
void ap_daemons_stop(void);
/* The static binaries the engine runs. 0, or -1 after naming the missing. */
int ap_prerequisites(void);

/* Band, channel and width for -b/-c/-W, the station and the chip, the 5 GHz
 * NO-IR fix included. */
void ap_plan_radio(const struct vap_opts *o, struct plan *p);
/* SIGINT and SIGTERM are held for the whole start and looked at between
 * steps, so a start told to stop always tears down (start-ap's trap) and
 * never leaves half an AP. */
void ap_hold_signals(void);
bool ap_interrupted(void);

#endif
