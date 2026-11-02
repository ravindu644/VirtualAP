/* The nl80211 questions and the two commands the engine needs, over raw
 * generic netlink: what `iw` was spawned for.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NL80211_H
#define VAP_NL80211_H

#include <stdbool.h>

#define PHY_MAX_FREQS 128

/* What a radio can do, per band: 0 is 2.4 GHz, 1 is 5 GHz. */
enum { BAND_24 = 0, BAND_5 = 1, BANDS = 2 };

struct phy_info {
  struct {
    bool present, ht40, vht;
  } band[BANDS];
  int nfreqs;
  struct {
    int freq;
    bool disabled, no_ir;
  } freqs[PHY_MAX_FREQS];
};

/* The wiphy behind ifname, from one split GET_WIPHY dump filtered by the
 * interface. Band data comes split across messages and is merged. 0 or
 * -errno, *out zeroed either way. */
int nl80211_phy_info(const char *ifname, struct phy_info *out);
/* The frequency of the BSS ifname is associated with, from the scan results
 * as `iw link` reads it: Samsung's Broadcom driver leaves the channel out of
 * the interface info. 0 when not connected, -errno on error. */
int nl80211_sta_freq(const char *ifname);
/* The width the radio actually runs ifname at, in MHz, 0 when the driver
 * does not say (Broadcom FullMAC often does not). */
int nl80211_width(const char *ifname);
/* `iw dev phy_iface interface add name type __ap`. 0 or -errno. */
int nl80211_add_ap(const char *phy_iface, const char *name);
/* `iw dev name del`. 0 or -errno (-ENODEV when it is not there). */
int nl80211_del(const char *name);

#endif
