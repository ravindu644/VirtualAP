/* Getting the driver's regulatory domain to allow an AP on 5 GHz.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_REGDOM_H
#define VAP_REGDOM_H

#include "radio/channel.h"

/* True if phy marks the 5 GHz channel no-IR (beaconing forbidden). */
bool chan_no_ir(const struct phy_info *phy, int ch);

/* Samsung's Broadcom FullMAC firmware boots in world country "99", and the
 * framework sometimes never pushes a real code (a race with telephony at
 * boot): every 5 GHz channel stays NO-IR and hostapd dies ~5 s in, after the
 * liveness check. The wiphy is self-managed, so `iw reg set` and hostapd's
 * country_code are ignored; the one lever is the driver's private COUNTRY
 * command through the supplicant's control socket. When the channel is
 * still NO-IR after it, the plan falls back to 2.4 GHz (the station's
 * channel, else 6). phy is refreshed in place. */
void ensure_5g_ir(struct plan *p, const char *phy_iface, const struct sta *s,
                  struct phy_info *phy);

#endif
