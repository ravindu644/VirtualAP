/* What both modes share: ap0 itself and the daemons on it.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_AP_H
#define VAP_AP_H

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

#endif
