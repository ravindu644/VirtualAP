/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cli/state.h"
#include "cmd/cmd.h"
#include "core/fsutil.h"
#include "core/pidfile.h"
#include "radio/nl80211.h"
#include "services/dnsmasq.h"
#include "services/hostapd.h"
#include "virtualap.h"

/* The run.state keys the app reads, printed in the file's own order. */
static bool shown(const char *key) {
  static const char *const keys[] = {
      "ssid",           "band",           "channel", "upstream",
      "upstream_table", "upstream_iface", "gateway", "gw_mac",
      "dns_servers",    "container",      "started"};
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    if (!strcmp(key, keys[i]))
      return true;
  return false;
}

/* key=value lines for the app, every 3 s while its screen is open, so
 * nothing here may block for long. */
int cmd_status(struct vap_opts *o) {
  struct state st;
  bool up = state_load(VAP_STATE, &st);
  bool bridged = up && !strcmp(state_get(&st, "mode"), "bridged");
  bool running = up && pidfile_ours(HOSTAPD_PID, "hostapd") > 0;
  const char *dnsmasq = "dead";
  /* DHCP and DNS are the container's in bridged mode, not ours. */
  if (bridged)
    dnsmasq = "container";
  else if (up && pidfile_ours(DNSMASQ_PID, "dnsmasq") > 0)
    dnsmasq = "running";
  printf("running=%d\nmode=%s\ndnsmasq=%s\nclients=%d\n", running,
         bridged ? "bridged" : "routed", dnsmasq, up ? hostapd_clients() : 0);

  if (running) {
    char conf[4096];
    if (read_file(HOSTAPD_CONF, conf, sizeof(conf)) < 0)
      conf[0] = '\0';
    /* The width the radio really runs, which FullMAC same-channel
     * concurrency can force down whatever the conf asked; the conf when the
     * driver does not say. */
    int width = nl80211_width(AP_IFACE);
    if (!width)
      width = strstr(conf, "\nvht_oper_chwidth=1") ? 80
              : strstr(conf, "\nht_capab=[HT40")   ? 40
                                                   : 20;
    printf("width=%d\n", width);
    const char *sec = !strstr(conf, "\nwpa=") ? "open"
                      : strstr(conf, "\nwpa_key_mgmt=WPA-PSK SAE\n")
                          ? "wpa2wpa3"
                      : strstr(conf, "\nwpa_key_mgmt=SAE\n") ? "wpa3"
                                                             : "wpa2";
    printf("security=%s\n", sec);
  }
  int printed = 0;
  for (size_t i = 0; up && i < st.n; i++)
    if (shown(st.kv[i].key)) {
      printf("%s=%s\n", st.kv[i].key, st.kv[i].val);
      printed++;
    }
  if (!printed)
    printf("gateway=%s\n", o->ip_gw);
  return 0;
}
