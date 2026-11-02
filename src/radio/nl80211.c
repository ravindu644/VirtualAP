/* What externals/iw does for `dev info`, `dev link`, `phy info` and
 * `interface add/del`, as the attributes it reads (info.c, link.c,
 * interface.c), without libnl.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "radio/nl80211.h"

#include <errno.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <string.h>

#include "radio/genl.h"

/* Old kernel headers name the flag after what it was. */
#ifndef NL80211_FREQUENCY_ATTR_NO_IR
#define NL80211_FREQUENCY_ATTR_NO_IR NL80211_FREQUENCY_ATTR_PASSIVE_SCAN
#endif

struct session {
  struct nl nl;
  int id;
};

static int open_nl80211(struct session *s) {
  return genl_open(&s->nl, "nl80211", &s->id);
}

/* A request about ifname; -ENODEV when there is no such interface. */
static int req_for(struct session *s, struct nl_req *r, int cmd,
                   const char *ifname) {
  unsigned idx = if_nametoindex(ifname);
  if (!idx)
    return -ENODEV;
  genl_req(r, s->id, cmd, 0);
  nl_attr_u32(r, NL80211_ATTR_IFINDEX, idx);
  return 0;
}

static void add_freqs(struct phy_info *p, struct rtattr *freqs) {
  int rem = (int)RTA_PAYLOAD(freqs);
  for (struct rtattr *f = RTA_DATA(freqs); RTA_OK(f, rem);
       f = RTA_NEXT(f, rem)) {
    struct rtattr *tb[NL80211_FREQUENCY_ATTR_MAX + 1];
    nl_parse(tb, NL80211_FREQUENCY_ATTR_MAX, RTA_DATA(f), (int)RTA_PAYLOAD(f));
    if (!tb[NL80211_FREQUENCY_ATTR_FREQ] || p->nfreqs >= PHY_MAX_FREQS)
      continue;
    p->freqs[p->nfreqs].freq = (int)nl_u32(tb[NL80211_FREQUENCY_ATTR_FREQ]);
    p->freqs[p->nfreqs].disabled = tb[NL80211_FREQUENCY_ATTR_DISABLED] != NULL;
    /* "no IR" in iw on new kernels, "passive scan" on old ones: either way
     * the radio may not beacon there. */
    p->freqs[p->nfreqs].no_ir = tb[NL80211_FREQUENCY_ATTR_NO_IR] != NULL;
    p->nfreqs++;
  }
}

static void wiphy_cb(const struct nlmsghdr *h, void *arg) {
  struct phy_info *p = arg;
  struct rtattr *tb[NL80211_ATTR_MAX + 1];
  genl_parse(tb, NL80211_ATTR_MAX, h);
  if (!tb[NL80211_ATTR_WIPHY_BANDS])
    return;
  int rem = (int)RTA_PAYLOAD(tb[NL80211_ATTR_WIPHY_BANDS]);
  for (struct rtattr *b = RTA_DATA(tb[NL80211_ATTR_WIPHY_BANDS]);
       RTA_OK(b, rem); b = RTA_NEXT(b, rem)) {
    /* 0 is 2.4 GHz and 1 is 5 GHz on every kernel. */
    int idx = b->rta_type == 0 ? BAND_24 : b->rta_type == 1 ? BAND_5 : -1;
    struct rtattr *bt[NL80211_BAND_ATTR_MAX + 1];
    nl_parse(bt, NL80211_BAND_ATTR_MAX, RTA_DATA(b), (int)RTA_PAYLOAD(b));
    if (bt[NL80211_BAND_ATTR_FREQS])
      add_freqs(p, bt[NL80211_BAND_ATTR_FREQS]);
    if (idx < 0)
      continue;
    p->band[idx].present = true;
    /* iw: "HT20/HT40" is bit 1 of the HT capabilities; "VHT Capabilities"
     * needs both the capabilities and the MCS set. */
    if (bt[NL80211_BAND_ATTR_HT_CAPA] &&
        RTA_PAYLOAD(bt[NL80211_BAND_ATTR_HT_CAPA]) >= 2) {
      uint16_t capa;
      memcpy(&capa, RTA_DATA(bt[NL80211_BAND_ATTR_HT_CAPA]), sizeof(capa));
      if (capa & 0x2)
        p->band[idx].ht40 = true;
    }
    if (bt[NL80211_BAND_ATTR_VHT_CAPA] && bt[NL80211_BAND_ATTR_VHT_MCS_SET])
      p->band[idx].vht = true;
  }
}

int nl80211_phy_info(const char *ifname, struct phy_info *out) {
  struct session s;
  memset(out, 0, sizeof(*out));
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  struct nl_req r;
  ret = req_for(&s, &r, NL80211_CMD_GET_WIPHY, ifname);
  if (ret == 0) {
    nl_attr(&r, NL80211_ATTR_SPLIT_WIPHY_DUMP, NULL, 0);
    ret = nl_dump(&s.nl, &r, wiphy_cb, out);
  }
  nl_close(&s.nl);
  return ret;
}

static void scan_cb(const struct nlmsghdr *h, void *arg) {
  int *freq = arg;
  struct rtattr *tb[NL80211_ATTR_MAX + 1];
  genl_parse(tb, NL80211_ATTR_MAX, h);
  if (!tb[NL80211_ATTR_BSS])
    return;
  struct rtattr *bss[NL80211_BSS_MAX + 1];
  nl_parse(bss, NL80211_BSS_MAX, RTA_DATA(tb[NL80211_ATTR_BSS]),
           (int)RTA_PAYLOAD(tb[NL80211_ATTR_BSS]));
  if (!bss[NL80211_BSS_STATUS] || !bss[NL80211_BSS_FREQUENCY])
    return;
  uint32_t st = nl_u32(bss[NL80211_BSS_STATUS]);
  if (st == NL80211_BSS_STATUS_ASSOCIATED || st == NL80211_BSS_STATUS_IBSS_JOINED)
    *freq = (int)nl_u32(bss[NL80211_BSS_FREQUENCY]);
}

int nl80211_sta_freq(const char *ifname) {
  struct session s;
  int ret = open_nl80211(&s), freq = 0;
  if (ret < 0)
    return ret;
  /* Android scans in the background, and a scan dump that spans a new scan
   * comes back interrupted; the associated BSS is still the one found. */
  for (int attempt = 0; attempt < 3 && !freq; attempt++) {
    struct nl_req r;
    ret = req_for(&s, &r, NL80211_CMD_GET_SCAN, ifname);
    if (ret == 0)
      ret = nl_dump(&s.nl, &r, scan_cb, &freq);
    if (ret != -EINTR)
      break;
  }
  nl_close(&s.nl);
  return freq ? freq : ret < 0 ? ret : 0;
}

static void width_cb(const struct nlmsghdr *h, void *arg) {
  struct rtattr *tb[NL80211_ATTR_MAX + 1];
  genl_parse(tb, NL80211_ATTR_MAX, h);
  if (!tb[NL80211_ATTR_CHANNEL_WIDTH])
    return;
  /* The widths iw prints as a 2 or 3 digit number of MHz, which is what
   * start-ap's status scraped; 80+80 and the narrow ones it never matched. */
  static const int mhz[] = {[NL80211_CHAN_WIDTH_20_NOHT] = 20,
                            [NL80211_CHAN_WIDTH_20] = 20,
                            [NL80211_CHAN_WIDTH_40] = 40,
                            [NL80211_CHAN_WIDTH_80] = 80,
                            [NL80211_CHAN_WIDTH_160] = 160,
                            [NL80211_CHAN_WIDTH_10] = 10,
                            [13] = 320};
  uint32_t w = nl_u32(tb[NL80211_ATTR_CHANNEL_WIDTH]);
  if (w < sizeof(mhz) / sizeof(mhz[0]))
    *(int *)arg = mhz[w];
}

int nl80211_width(const char *ifname) {
  struct session s;
  int width = 0;
  if (open_nl80211(&s) < 0)
    return 0;
  struct nl_req r;
  if (req_for(&s, &r, NL80211_CMD_GET_INTERFACE, ifname) == 0)
    nl_get(&s.nl, &r, width_cb, &width);
  nl_close(&s.nl);
  return width;
}

int nl80211_add_ap(const char *phy_iface, const char *name) {
  struct session s;
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  struct nl_req r;
  ret = req_for(&s, &r, NL80211_CMD_NEW_INTERFACE, phy_iface);
  if (ret == 0) {
    nl_attr_str(&r, NL80211_ATTR_IFNAME, name);
    nl_attr_u32(&r, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_AP);
    ret = nl_talk(&s.nl, &r);
  }
  nl_close(&s.nl);
  return ret;
}

int nl80211_del(const char *name) {
  struct session s;
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  struct nl_req r;
  ret = req_for(&s, &r, NL80211_CMD_DEL_INTERFACE, name);
  if (ret == 0)
    ret = nl_talk(&s.nl, &r);
  nl_close(&s.nl);
  return ret;
}
