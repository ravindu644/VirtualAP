/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "cli/cli.h"
#include "cmd/cmd.h"
#include "core/core.h"
#include "mode/mode.h"
#include "net/ipt/ipt.h"
#include "net/net.h"
#include "radio/radio.h"
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

/* One "mac ip name" line per connected client, "-" for what is not known.
 * hostapd says who is connected; the leases only name them. Not part of status,
 * which runs for as long as the screen is open: in managed mode this starts
 * droidspaces, so the app asks only while its list is open. */
int cmd_clients(struct vap_opts *o) {
  (void)o;
  struct state st;
  if (!state_load(VAP_STATE, &st))
    return 0;
  char mac[18];
  if (!hostapd_station(NULL, mac))
    return 0;
  char leases[16384] = "";
  /* Missing, or past this much, leaves a client with its MAC alone. */
  if (!strcmp(state_get(&st, "mode"), "bridged"))
    ds_read_leases(state_get(&st, "container"), leases, sizeof(leases));
  else
    read_file(DNSMASQ_LEASES, leases, sizeof(leases));
  do {
    char ip[16], name[64];
    lease_find(leases, mac, state_get(&st, "gateway"), ip, name);
    printf("%s %s %s\n", mac, ip, name);
  } while (hostapd_station(mac, mac));
  return 0;
}

#define MAX_IFACES 128

struct iface {
  char name[IF_NAMESIZE];
  char ip[INET_ADDRSTRLEN];
};

static int by_name(const void *a, const void *b) {
  return strcmp(((const struct iface *)a)->name,
                ((const struct iface *)b)->name);
}

/* What Droidspaces' and USB Arsenal's pickers leave out, and our own: the
 * links these three create to carry a hotspot, a tether or a container, and
 * netd's placeholder, which has a default route of its own. */
static bool hidden_iface(const char *name) {
  static const char *const own[] = {AP_IFACE, "vap-", "vaplan",
                                    "ds-",    "ua-",  "dummy"};
  for (size_t i = 0; i < sizeof(own) / sizeof(own[0]); i++)
    if (!strncmp(name, own[i], strlen(own[i])))
      return true;
  return false;
}

/* What -o would accept. Droidspaces lists default routes only and takes a typed
 * name for anything else; the app has no such field, so a split-tunnel VPN has
 * to be listed for it to be picked at all. */
static bool has_internet_route(struct nl *nl, const char *name) {
  int table;
  return route_iface_table(nl, AF_INET, name, &table) == 0 ||
         route_iface_table(nl, AF_INET6, name, &table) == 0;
}

/* Candidate upstreams for the app, "name:ip" or "name:no-ip" per line, in
 * name order: the links with a route to the internet, in any table. A phone
 * has dozens of others. getifaddrs lists every link (AF_PACKET) and then the
 * addresses, primary IPv4 address first. */
int cmd_interfaces(struct vap_opts *o) {
  (void)o;
  struct ifaddrs *all;
  struct nl nl;
  if (nl_open(&nl) < 0) {
    vap_error("Cannot open a netlink socket.");
    return 1;
  }
  if (getifaddrs(&all) < 0) {
    vap_error("Cannot list the network interfaces.");
    nl_close(&nl);
    return 1;
  }
  static struct iface ifs[MAX_IFACES];
  int n = 0;
  for (struct ifaddrs *a = all; a; a = a->ifa_next) {
    if (hidden_iface(a->ifa_name))
      continue;
    int i;
    for (i = 0; i < n && strcmp(ifs[i].name, a->ifa_name); i++)
      ;
    if (i == n) {
      if (n == MAX_IFACES)
        continue;
      snprintf(ifs[n].name, sizeof(ifs[n].name), "%s", a->ifa_name);
      ifs[n++].ip[0] = '\0';
    }
    if (!ifs[i].ip[0] && a->ifa_addr && a->ifa_addr->sa_family == AF_INET)
      inet_ntop(AF_INET, &((struct sockaddr_in *)(void *)a->ifa_addr)->sin_addr,
                ifs[i].ip, sizeof(ifs[i].ip));
  }
  freeifaddrs(all);
  qsort(ifs, (size_t)n, sizeof(ifs[0]), by_name);
  for (int i = 0; i < n; i++)
    if (has_internet_route(&nl, ifs[i].name))
      printf("%s:%s\n", ifs[i].name, ifs[i].ip[0] ? ifs[i].ip : "no-ip");
  nl_close(&nl);
  return 0;
}

/* What the app greys out or offers: 5 GHz widths beyond 20 MHz, the TTL fix,
 * the factory MAC for Static MAC mode. */
int cmd_caps(struct vap_opts *o) {
  struct phy_info phy;
  char mac[18];
  nl80211_phy_info(o->phy_iface, &phy);
  wifi_mac(o->phy_iface, mac);
  printf("ht40=%d\n", phy.band[BAND_5].ht40);
  printf("vht=%d\n", phy.band[BAND_5].vht);
  printf("ttl=%d\n", ttl_supported());
  printf("mac=%s\n", mac);
  return 0;
}
