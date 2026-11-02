/* What radio/'s own files share, and the tests check: not part of its
 * public header. Included as "internal.h", or by path from tests/.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_RADIO_INTERNAL_H
#define VAP_RADIO_INTERNAL_H

#include "radio/radio.h"

/* driver.c: The hostapd / wpa_supplicant control socket, what hostapd_cli
 * speaks. */

/* cmd without waiting for the reply: wpa_supplicant runs a driver command
 * before it answers, and on Samsung's firmware the answer never comes back
 * (SELinux). 0 or -errno. */
int wpa_ctrl_send(const char *path, const char *cmd);

/* channel.c: Band, channel and width policy: the only place a channel number
 * is compared. Most phone chips beacon only on the channel their own Wi-Fi
 * is on (same-channel concurrency), so the station's channel leads. */

/* 5 GHz DFS range, 52-144: beaconing there needs radar detection, which
 * phone chips refuse. */
bool is_dfs(int ch);

/* channel.c: Getting the driver's regulatory domain to allow an AP on 5 GHz. */

/* True if phy marks the 5 GHz channel no-IR (beaconing forbidden). */
bool chan_no_ir(const struct phy_info *phy, int ch);

#endif
