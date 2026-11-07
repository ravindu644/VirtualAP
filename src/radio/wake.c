/* Standalone mode's way out of "No Wi-Fi radio is registered": with the
 * phone's Wi-Fi switched off, some drivers drop the wiphy altogether. Each
 * lever below is what the framework does on one kind of chip, written
 * without it, so the phone's Wi-Fi setting stays off.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "core/core.h"
#include "radio/radio.h"

static void sleep_ms(int ms) {
  struct timespec t = {ms / 1000, (ms % 1000) * 1000L * 1000L};
  nanosleep(&t, NULL);
}

/* True when the wiphy shows up within ms. */
static bool wiphy_within(const char *phy_iface, int ms) {
  for (int waited = 0;; waited += 250) {
    if (nl80211_find_wiphy(phy_iface) >= 0)
      return true;
    if (waited >= ms)
      return false;
    sleep_ms(250);
  }
}

/* The nodes a driver exposes for switching the chip on, written the way its
 * vendor's init script does. write_file never creates a file, so a node this
 * phone does not have is a plain miss. MediaTek first: wlan0 and the wiphy
 * only exist after the 1 goes into wmtWifi. Qualcomm's qcacld has two
 * variants, a boot node and the fwpath parameter. */
static const struct {
  const char *path, *value;
} nodes[] = {
    {"/dev/wmtWifi", "1"},
    {"/sys/kernel/boot_wlan/boot_wlan", "1"},
    {"/sys/module/wlan/parameters/fwpath", "sta"},
};

int radio_wake(const char *phy_iface) {
  if (nl80211_find_wiphy(phy_iface) >= 0)
    return 0;
  for (size_t i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++) {
    if (write_file(nodes[i].path, nodes[i].value) < 0)
      continue;
    vap_log("Wi-Fi radio is off - wrote %s to %s...", nodes[i].value,
            nodes[i].path);
    if (wiphy_within(phy_iface, 4000))
      return 0;
  }
  return -ENODEV;
}
