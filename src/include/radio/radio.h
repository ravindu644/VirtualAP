/* The radio: nl80211, the control sockets, the factory MAC and the channel
 * policy.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_RADIO_H
#define VAP_RADIO_H

#include <linux/genetlink.h>
#include <stdbool.h>
#include <stddef.h>

#include "net/net.h"

/* driver.c: The nl80211 questions and the two commands the engine needs,
 * over raw generic netlink: what `iw` was spawned for. */

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

/* The same questions asked of a wiphy by index instead of through a netdev,
 * for standalone mode: with the phone's Wi-Fi off, wlan0 is down or gone
 * while the wiphy is still registered, and `iw phy phyN interface add` works
 * without it. */

/* The wiphy index to use: the one behind prefer_ifname when that interface
 * exists, else the first wiphy that lists AP among its interface types.
 * -ENODEV when the kernel has no Wi-Fi radio registered at all (driver not
 * loaded), -errno on a netlink error. */
int nl80211_find_wiphy(const char *prefer_ifname);
/* Standalone mode with the Wi-Fi switched off: write the driver's power node
 * (MediaTek wmtWifi, Qualcomm boot_wlan or fwpath) until a wiphy is
 * registered. 0 when one is, -ENODEV when no node woke it. */
int radio_wake(const char *phy_iface);
/* nl80211_phy_info for a wiphy index. 0 or -errno, *out zeroed either way. */
int nl80211_phy_info_idx(int wiphy, struct phy_info *out);
/* `iw phy phyN interface add name type __ap`. 0 or -errno. */
int nl80211_add_ap_phy(int wiphy, const char *name);
/* `iw dev name del`. 0 or -errno (-ENODEV when it is not there). */
int nl80211_del(const char *name);

/* driver.c: The hostapd / wpa_supplicant control socket, what hostapd_cli
 * speaks. */

/* cmd to the daemon listening at path, its reply in reply (NUL terminated)
 * within timeout_ms. Returns the reply's length, or -errno (-ETIMEDOUT for
 * a daemon that never answers, so a hung hostapd cannot stall a status;
 * -EMSGSIZE for a reply that does not fit). */
int wpa_ctrl_request(const char *path, const char *cmd, char *reply,
                     size_t size, int timeout_ms);

/* driver.c: The Wi-Fi's factory MAC, what the app's Static MAC mode starts
 * from. */

/* aa:bb:cc:dd:ee:ff in lower case, or "" when no source has it:
 *  1. Android's Wi-Fi service (11 and up) saves the factory MAC it read from
 *     the HAL in its settings store, whatever the vendor or driver.
 *  2. ifname's address, but only while the kernel says nobody changed it
 *     (addr_assign_type 0): Android gives wlan0 a random MAC per network.
 *  3. The kernel's permanent address.
 *  4. Samsung's copy in efs, with no trailing newline: aa:bb:.. on Broadcom
 *     phones, twelve bare hex digits on MediaTek ones. */
void wifi_mac(const char *ifname, char out[18]);

/* The MAC at the start of s, aa:bb:cc:dd:ee:ff or twelve bare hex digits,
 * as aa:bb:.. in lower case. False for anything else, or all zeros. */
bool mac_take(const char *s, char out[18]);

/* channel.c: Band, channel and width policy: the only place a channel number
 * is compared. Most phone chips beacon only on the channel their own Wi-Fi
 * is on (same-channel concurrency), so the station's channel leads. */

/* The phone's own Wi-Fi connection: band 0 when it has none. */
struct sta {
  int band; /* 2, 5 or 6 */
  int chan;
};
/* The one place a frequency becomes a band and channel. */
void sta_from_freq(int freq, struct sta *out);
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

/* The 80 MHz block's centre channel for a 5 GHz primary, 0 when the channel
 * is in no 80 MHz block (165 and the like). */
int vht_seg0(int ch);
/* HT40 secondary direction inside the block: the lower channel of each
 * 40 MHz pair is '+', the upper '-'. */
char ht40_dir(int ch, int seg0);

/* channel.c: Getting the driver's regulatory domain to allow an AP on 5 GHz. */

/* Samsung's Broadcom FullMAC firmware boots in world country "99", and the
 * framework sometimes never pushes a real code (a race with telephony at
 * boot): every 5 GHz channel stays NO-IR and hostapd dies ~5 s in, after the
 * liveness check. The wiphy is self-managed, so `iw reg set` and hostapd's
 * country_code are ignored; the one lever is the driver's private COUNTRY
 * command through the supplicant's control socket. When the channel is
 * still NO-IR after it, the plan falls back to 2.4 GHz (the station's
 * channel, else 6). phy is refreshed in place. wiphy is the radio's index
 * when phy_iface may not exist (standalone mode), -1 to go through
 * phy_iface. */
void ensure_5g_ir(struct plan *p, const char *phy_iface, int wiphy,
                  const struct sta *s, struct phy_info *phy);

#endif
