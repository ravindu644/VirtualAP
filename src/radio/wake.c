/* Standalone mode's way out of "No Wi-Fi radio is registered": with the
 * phone's Wi-Fi switched off, some drivers drop the wiphy altogether. Each
 * lever below is what the framework does on one kind of chip, written
 * without it, so the phone's Wi-Fi setting stays off.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include <string.h>

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

/* Qualcomm phones of the pronto generation have the
 * framework insmod the Wi-Fi module when Wi-Fi goes on and rmmod it when it
 * goes off, so with Wi-Fi off there is no module, no wiphy and no node to
 * write. The module is loaded here the same way. */
static const char *const modules[] = {
    "/vendor/lib/modules/pronto/pronto_wlan.ko",
    "/system/vendor/lib/modules/pronto/pronto_wlan.ko",
    "/system/lib/modules/pronto/pronto_wlan.ko",
    "/vendor/lib/modules/wlan.ko",
    "/system/lib/modules/wlan.ko",
    "/system/lib/modules/pronto_wlan.ko",
    "/vendor/lib/modules/pronto_wlan.ko",
    "/vendor/lib/modules/qca_cld3_wlan.ko",
    "/vendor_dlkm/lib/modules/qca_cld3_wlan.ko",
};

/* 0 when the module is in the kernel afterwards, else -errno. */
static int load_module(const char *path) {
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -errno;
  long r = syscall(__NR_finit_module, fd, "", 0);
  int e = errno;
  close(fd);
  return r == 0 || e == EEXIST ? 0 : -e;
}

/* The nodes a driver exposes for switching the chip on, written the way its
 * vendor's Wi-Fi HAL or init script does. write_file never creates a file, so
 * a node this phone does not have is a plain miss. Qualcomm's qcacld loads
 * its driver, and registers the wiphy, when ON goes into /dev/wlan (OFF
 * removes it again, which is what switching Wi-Fi off does); older builds use
 * a boot node or the fwpath parameter. MediaTek only creates wlan0 and the
 * wiphy after the 1 goes into wmtWifi. */
static const struct {
  const char *path, *value;
} nodes[] = {
    {"/dev/wlan", "ON"},
    {"/dev/wmtWifi", "1"},
    {"/sys/kernel/boot_wlan/boot_wlan", "1"},
    {"/sys/module/wlan/parameters/fwpath", "sta"},
};

int radio_wake(const char *phy_iface) {
  if (nl80211_find_wiphy(phy_iface) >= 0)
    return 0;
  int tried = 0;
  for (size_t i = 0; i < sizeof(modules) / sizeof(modules[0]); i++) {
    if (access(modules[i], R_OK) < 0)
      continue;
    tried++;
    int r = load_module(modules[i]);
    vap_log("Wi-Fi radio is off - loading %s: %s", modules[i],
            r < 0 ? strerror(-r) : "ok");
    if (r < 0)
      continue;
    if (wiphy_within(phy_iface, 4000))
      return 0;
    break;
  }
  for (size_t i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++) {
    if (write_file(nodes[i].path, nodes[i].value) < 0)
      continue;
    tried++;
    vap_log("Wi-Fi radio is off - wrote %s to %s...", nodes[i].value,
            nodes[i].path);
    if (wiphy_within(phy_iface, 4000))
      return 0;
  }
  if (!tried)
    vap_log("No Wi-Fi module or power node found (pronto_wlan.ko, wlan.ko, "
            "/dev/wlan, /dev/wmtWifi, boot_wlan, fwpath).");
  return -ENODEV;
}
