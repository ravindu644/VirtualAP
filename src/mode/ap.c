/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/ap.h"

#include <net/if.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli/state.h"
#include "core/ipaddr.h"
#include "core/log.h"
#include "core/pidfile.h"
#include "net/link.h"
#include "net/ttl.h"
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

int ap_state_save(struct state *st, const struct vap_opts *o,
                  const struct plan *p, const struct subnet *net) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", p->band);
  state_set(st, "band", buf);
  snprintf(buf, sizeof(buf), "%d", p->channel);
  state_set(st, "channel", buf);
  state_set(st, "ssid", o->ssid);
  state_set(st, "gateway", net->gw);
  log_stamp(buf);
  state_set(st, "started", buf);
  if (state_save(VAP_STATE, st) < 0) {
    vap_error("Cannot write %s", VAP_STATE);
    return -1;
  }
  return 0;
}

void ap_ttl(const struct vap_opts *o, const char *src4, const char *src6) {
  /* ttl_apply clears first itself. */
  if (strcmp(o->ttl_fix, "1")) {
    ttl_clear();
    return;
  }
  if (ttl_apply(src4, src6) == 0)
    vap_log("  TTL fix : forwarded packets leave with TTL 64");
  else
    vap_warn("TTL fix is not available: the kernel or iptables lacks the "
             "TTL target. Continuing without it.");
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
