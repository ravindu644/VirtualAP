/* Band, channel and width policy: the only place a channel number is
 * compared. Most phone chips beacon only on the channel their own Wi-Fi is
 * on (same-channel concurrency), so the station's channel leads.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CHANNEL_H
#define VAP_CHANNEL_H

#include <stdbool.h>

#include "radio/nl80211.h"

/* The phone's own Wi-Fi connection: band 0 when it has none. */
struct sta {
  int band; /* 2, 5 or 6 */
  int chan;
};
/* The one place a frequency becomes a band and channel. */
void sta_from_freq(int freq, struct sta *out);
/* A 5 GHz channel's centre frequency. */
int chan_freq5(int ch);

struct plan {
  int band;    /* 2 or 5 */
  int channel; /* 0 = auto until plan_channel */
  int width;   /* 20, 40 or 80 once plan_width ran */
};
/* From the -b and -c values as start validated them. */
void plan_init(struct plan *p, const char *band, const char *channel);

/* Pick the channel: follow the station when it fits the band, fall back when
 * it cannot (DFS, another band), else the defaults (6, or 36). Returns true
 * when the result is a 5 GHz channel that ensure_5g_ir must clear for
 * beaconing; plan_band() settles the band afterwards. */
bool plan_channel(struct plan *p, const struct sta *s);
/* The band follows the final channel. */
void plan_band(struct plan *p);
/* The width asked for (-W) cut down to what the band, the channel and the
 * chip can do, with a warning when that differs from a width asked by name. */
void plan_width(struct plan *p, const char *want, const struct phy_info *phy);

/* 5 GHz DFS range, 52-144: beaconing there needs radar detection, which
 * phone chips refuse. */
bool is_dfs(int ch);
/* The 80 MHz block's centre channel for a 5 GHz primary, 0 when the channel
 * is in no 80 MHz block (165 and the like). */
int vht_seg0(int ch);
/* HT40 secondary direction inside the block: the lower channel of each
 * 40 MHz pair is '+', the upper '-'. */
char ht40_dir(int ch, int seg0);

#endif
