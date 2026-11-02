/* start-ap's create_ap_iface, del_ap_iface, stop_daemons and
 * check_prerequisites.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/ap.h"

#include <net/if.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "core/log.h"
#include "core/pidfile.h"
#include "net/link.h"
#include "radio/nl80211.h"
#include "radio/regdom.h"
#include "services/dnsmasq.h"
#include "services/hostapd.h"
#include "virtualap.h"

void ap_iface_del(void) {
  if (nl80211_del(AP_IFACE) == 0)
    return;
  struct nl nl;
  if (nl_open(&nl) == 0) {
    link_del(&nl, AP_IFACE);
    nl_close(&nl);
  }
}

int ap_iface_create(const char *phy_iface) {
  vap_log("Creating virtual AP interface '%s' on %s...", AP_IFACE, phy_iface);
  ap_iface_del();
  int ret = nl80211_add_ap(phy_iface, AP_IFACE);
  if (ret < 0) {
    /* What iw printed, which the app's log showed. */
    vap_log("command failed: %s (%d)", strerror(-ret), ret);
    vap_error("Failed to create %s - chip/driver may not support AP mode.",
              AP_IFACE);
    return -1;
  }
  if (!if_nametoindex(AP_IFACE)) {
    vap_error("%s was not created successfully", AP_IFACE);
    return -1;
  }
  return 0;
}

void ap_daemons_stop(void) {
  pidfile_kill(HOSTAPD_PID, "hostapd");
  pidfile_kill(DNSMASQ_PID, "dnsmasq");
  /* The sh engine's IPv6 client loop (`sh start-ap v6sync ...`): killed, not
   * only told by its pid file going, or one iteration already past that check
   * could add a proxy entry after the teardown removed them. */
  pid_t v6sync = pidfile_cmdline(VAP_RUN "/v6sync.pid", " v6sync ");
  if (v6sync > 0)
    kill(v6sync, SIGTERM);
  unlink(VAP_RUN "/v6sync.pid");
  unlink(HOSTAPD_CONF);
  unlink(DNSMASQ_CONF);
}

int ap_prerequisites(void) {
  static const char *const bins[] = {VAP_BIN "/hostapd", VAP_BIN "/dnsmasq"};
  for (int i = 0; i < 2; i++)
    if (access(bins[i], X_OK) < 0) {
      vap_error("%s not found", bins[i]);
      return -1;
    }
  return 0;
}

void ap_plan_radio(const struct vap_opts *o, struct plan *p) {
  struct phy_info phy;
  struct sta sta;
  int freq = nl80211_sta_freq(o->phy_iface);
  sta_from_freq(freq > 0 ? freq : 0, &sta);
  nl80211_phy_info(o->phy_iface, &phy);
  plan_init(p, o->band, o->channel);
  /* 5 GHz needs the regdomain to allow beaconing there, and ensure_5g_ir may
   * itself fall back to 2.4 GHz, so the band follows the final channel. */
  if (plan_channel(p, &sta))
    ensure_5g_ir(p, o->phy_iface, &sta, &phy);
  plan_band(p);
  plan_width(p, o->width, &phy);
}

void ap_hold_signals(void) {
  sigset_t hold;
  sigemptyset(&hold);
  sigaddset(&hold, SIGINT);
  sigaddset(&hold, SIGTERM);
  sigprocmask(SIG_BLOCK, &hold, NULL);
}

bool ap_interrupted(void) {
  sigset_t pending;
  sigpending(&pending);
  return sigismember(&pending, SIGINT) || sigismember(&pending, SIGTERM);
}
