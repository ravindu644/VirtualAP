/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "radio/channel.h"

#include <stdlib.h>
#include <string.h>

#include "core/log.h"

void sta_from_freq(int freq, struct sta *out) {
  out->band = out->chan = 0;
  if (freq == 2484) {
    out->band = 2;
    out->chan = 14;
  } else if (freq >= 2412 && freq <= 2472) {
    out->band = 2;
    out->chan = (freq - 2407) / 5;
  } else if (freq >= 5180 && freq < 5925) {
    /* From channel 36: the few below it (32, Japan's 8-16) are nothing an
     * AP is planned on, so such a station counts as on no band at all. */
    out->band = 5;
    out->chan = (freq - 5000) / 5;
  } else if (freq == 5935) {
    out->band = 6;
    out->chan = 2;
  } else if (freq >= 5950 && freq <= 7125) {
    /* 6 GHz reuses the 5 GHz numbers for other frequencies, so the band
     * says which is meant. */
    out->band = 6;
    out->chan = (freq - 5950) / 5;
  }
}

int chan_freq5(int ch) { return 5000 + 5 * ch; }

void plan_init(struct plan *p, const char *band, const char *channel) {
  p->band = !strcmp(band, "5") ? 5 : 2;
  p->channel = *channel ? atoi(channel) : 0;
  p->width = 20;
}

bool is_dfs(int ch) { return ch >= 52 && ch <= 144; }

int vht_seg0(int c) {
  if (c >= 36 && c <= 48)
    return 42;
  if (c >= 52 && c <= 64)
    return 58;
  if (c >= 100 && c <= 112)
    return 106;
  if (c >= 116 && c <= 128)
    return 122;
  if (c >= 132 && c <= 144)
    return 138;
  if (c >= 149 && c <= 161)
    return 155;
  return 0;
}

/* seg0 - 6 is the block's first channel; channels step by 4, which also
 * holds for UNII-3's 149/153/157/161. */
char ht40_dir(int c, int seg0) {
  return ((c - (seg0 - 6)) / 4) % 2 == 0 ? '+' : '-';
}

static void follow_sta(struct plan *p, int sta_ch) {
  if (p->channel && p->channel != sta_ch)
    vap_warn("Requested channel %d but STA is on %d - using %d (same-channel "
             "concurrency).",
             p->channel, sta_ch, sta_ch);
  p->channel = sta_ch;
}

bool plan_channel(struct plan *p, const struct sta *s) {
  if (p->band == 5) {
    if (s->band == 5) {
      if (is_dfs(s->chan)) {
        vap_warn("STA is on 5GHz DFS channel %d - AP cannot beacon there. "
                 "Falling back to 2.4GHz.",
                 s->chan);
        /* A 5 GHz -c is overridden too: the station holds the radio on a
         * DFS channel, so any 5 GHz channel fails the same way. */
        p->channel = 6;
      } else {
        follow_sta(p, s->chan);
      }
    } else if (s->band) {
      vap_warn("STA is on %sGHz (channel %d) - 5GHz AP would need "
               "different-band concurrency.",
               s->band == 2 ? "2.4" : "6", s->chan);
      if (!p->channel) {
        vap_warn("Trying 5GHz channel 36; if the AP fails to start, use "
                 "band 2.");
        p->channel = 36;
      }
    } else if (!p->channel) {
      /* No STA connection (mobile-data upstream): the radio is free. */
      p->channel = 36;
    }
  } else if (s->band == 2) {
    follow_sta(p, s->chan);
  } else if (!p->channel) {
    p->channel = 6;
  }
  return p->channel >= 36;
}

void plan_band(struct plan *p) { p->band = p->channel >= 36 ? 5 : 2; }

void plan_width(struct plan *p, const char *want, const struct phy_info *phy) {
  if (p->band != 5) {
    /* A 40 MHz channel takes most of 2.4 GHz and crowds out the neighbours
     * and Bluetooth, so auto stays at 20 and 40 is only used when asked for
     * by name. Channel 14 has no room for a second channel. */
    p->width = !strcmp(want, "40") && p->channel <= 13 &&
                       phy->band[BAND_24].ht40
                   ? 40
                   : 20;
  } else {
    /* The widest the chip and the channel can do, 5 GHz's own capabilities
     * only: a chip can do HT40 on 2.4 GHz and not on 5. A channel in no
     * block (165) stays at 20. */
    int max = 20;
    if (vht_seg0(p->channel)) {
      if (phy->band[BAND_5].ht40)
        max = 40;
      if (phy->band[BAND_5].vht)
        max = 80;
    }
    if (!strcmp(want, "20"))
      p->width = 20;
    else if (!strcmp(want, "40"))
      p->width = max == 20 ? 20 : 40;
    else
      p->width = max;
  }
  if (strcmp(want, "auto") && atoi(want) != p->width)
    vap_warn("Requested %sMHz but using %dMHz (channel/chip limit).", want,
             p->width);
}
