/* The channel, width and MAC policy, pure, no radio needed.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>

#include "check.h"
#include "radio/channel.h"
#include "radio/regdom.h"
#include "radio/wifi_mac.h"

static struct plan plan(const char *band, const char *ch, int sta_freq,
                        bool *needs_ir) {
  struct plan p;
  struct sta s;
  plan_init(&p, band, ch);
  sta_from_freq(sta_freq, &s);
  *needs_ir = plan_channel(&p, &s);
  plan_band(&p);
  return p;
}

void test_radio(void) {
  struct sta s;
  sta_from_freq(2437, &s);
  CHECK(s.band == 2 && s.chan == 6);
  sta_from_freq(2484, &s);
  CHECK(s.band == 2 && s.chan == 14);
  sta_from_freq(5180, &s);
  CHECK(s.band == 5 && s.chan == 36);
  sta_from_freq(5955, &s);
  CHECK(s.band == 6 && s.chan == 1);
  sta_from_freq(0, &s);
  CHECK(s.band == 0);
  sta_from_freq(5935, &s);
  CHECK(s.band == 6 && s.chan == 2);
  /* 5 GHz below 36 is not followed onto a 2.4 GHz plan. */
  sta_from_freq(5160, &s);
  CHECK(s.band == 0);

  bool ir;
  struct plan p = plan("2", "", 0, &ir);
  CHECK(p.band == 2 && p.channel == 6 && !ir);
  p = plan("2", "11", 2437, &ir);
  CHECK(p.channel == 6);
  p = plan("5", "", 0, &ir);
  CHECK(p.band == 5 && p.channel == 36 && ir);
  p = plan("5", "", 5745, &ir);
  CHECK(p.channel == 149 && p.band == 5);
  /* DFS: a real fall back to 2.4, whatever -c said. */
  p = plan("5", "40", 5500, &ir);
  CHECK(p.band == 2 && p.channel == 6 && !ir);
  p = plan("5", "", 2412, &ir);
  CHECK(p.channel == 36 && p.band == 5);
  p = plan("5", "44", 2412, &ir);
  CHECK(p.channel == 44);
  /* A 6 GHz station is another band, not channel 191. */
  p = plan("5", "", 5955, &ir);
  CHECK(p.channel == 36 && p.band == 5);
  p = plan("2", "", 5955, &ir);
  CHECK(p.channel == 6 && p.band == 2);
  p = plan("5", "", 5160, &ir);
  CHECK(p.channel == 36 && p.band == 5);

  CHECK(vht_seg0(36) == 42 && vht_seg0(48) == 42 && vht_seg0(149) == 155);
  CHECK(vht_seg0(165) == 0 && vht_seg0(6) == 0);
  CHECK(ht40_dir(36, 42) == '+' && ht40_dir(40, 42) == '-');
  CHECK(ht40_dir(44, 42) == '+' && ht40_dir(48, 42) == '-');
  CHECK(ht40_dir(149, 155) == '+' && ht40_dir(161, 155) == '-');
  CHECK(is_dfs(52) && is_dfs(144) && !is_dfs(48) && !is_dfs(149));

  struct phy_info phy;
  memset(&phy, 0, sizeof(phy));
  phy.band[BAND_5].ht40 = phy.band[BAND_5].vht = true;
  p.band = 5;
  p.channel = 36;
  plan_width(&p, "auto", &phy);
  CHECK(p.width == 80);
  plan_width(&p, "40", &phy);
  CHECK(p.width == 40);
  p.channel = 165;
  plan_width(&p, "auto", &phy);
  CHECK(p.width == 20);
  /* 2.4 GHz: 40 only by name, and only with 2.4 GHz's own HT40. */
  p.band = 2;
  p.channel = 6;
  plan_width(&p, "40", &phy);
  CHECK(p.width == 20);
  phy.band[BAND_24].ht40 = true;
  plan_width(&p, "auto", &phy);
  CHECK(p.width == 20);
  plan_width(&p, "40", &phy);
  CHECK(p.width == 40);
  /* 5 GHz must not take 2.4 GHz's HT40 for its own. */
  memset(&phy, 0, sizeof(phy));
  phy.band[BAND_24].ht40 = true;
  p.band = 5;
  p.channel = 36;
  plan_width(&p, "auto", &phy);
  CHECK(p.width == 20);

  phy.nfreqs = 2;
  phy.freqs[0].freq = 5180;
  phy.freqs[0].no_ir = true;
  phy.freqs[1].freq = 6135; /* 6 GHz channel 37's neighbour, not 5 GHz's */
  phy.freqs[1].no_ir = true;
  CHECK(chan_no_ir(&phy, 36) && !chan_no_ir(&phy, 40));
  phy.freqs[0].disabled = true;
  CHECK(!chan_no_ir(&phy, 36));

  char mac[18];
  CHECK(mac_take("FC:93:6B:C2:3C:20\n", mac) && !strcmp(mac, "fc:93:6b:c2:3c:20"));
  CHECK(mac_take("FC936BC23C20", mac) && !strcmp(mac, "fc:93:6b:c2:3c:20"));
  CHECK(!mac_take("FC936BC23C20FC936BC23C20", mac));
  CHECK(!mac_take("00:00:00:00:00:00", mac));
  CHECK(!mac_take("fc:93:6b", mac));
}
