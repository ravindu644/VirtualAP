/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "cmd/cmd.h"
#include "net/ttl.h"
#include "radio/nl80211.h"
#include "radio/wifi_mac.h"

/* What the app greys out or offers: 5 GHz widths beyond 20 MHz, the TTL fix,
 * the factory MAC for Static MAC mode. */
int cmd_caps(struct vap_opts *o) {
  struct phy_info phy;
  char mac[18];
  nl80211_phy_info(o->phy_iface, &phy);
  wifi_mac(o->phy_iface, mac);
  printf("ht40=%d\n", phy.band[BAND_5].ht40);
  printf("vht=%d\n", phy.band[BAND_5].vht);
  printf("ttl=%d\n", ttl_supported());
  printf("mac=%s\n", mac);
  return 0;
}
