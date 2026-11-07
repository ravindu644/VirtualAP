/* The key table is USB-Arsenal's config.c pattern.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "cli/cli.h"
#include "internal.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "core/core.h"
#include "virtualap.h"

#define F(field) offsetof(struct vap_opts, field)

const struct vap_key vap_keys[] = {
    {'w', "PHY_IFACE", F(phy_iface), "wlan0", "<iface>",
     "Physical WiFi interface (default: wlan0)", 0},
    {'o', "UPSTREAM", F(upstream), "auto", "<iface,...>",
     "Upstream interfaces in priority order, or 'auto' (default: auto)", 0},
    {'s', "SSID", F(ssid), "", "<SSID>", "AP name", 0},
    {'p', "PASSWORD", F(password), "", "<password>", "AP password (8-63 chars)",
     0},
    {'b', "BAND", F(band), "2", "<band>", "2 = 2.4GHz (default), 5 = 5GHz", 0},
    {'c', "CHANNEL", F(channel), "", "<channel>",
     "Channel (auto-picked / STA-followed when omitted)", 0},
    {'W', "WIDTH", F(width), "auto", "<width>",
     "Channel width: auto (default), 20, 40, or 80 (80 is 5GHz\n"
     "only; on 2.4GHz auto means 20)",
     0},
    {'g', "IP_GW", F(ip_gw), GW_DEFAULT, "<gateway>",
     "Gateway IP (default: " GW_DEFAULT ")", 1},
    {'d', "DNS_SERVERS", F(dns_servers), "", "<dns>",
     "Comma-separated upstream DNS servers (default: " DNS_DEFAULT ")", 0},
    {'H', "HIDDEN", F(hidden), "0", "<0|1>",
     "Hide SSID (0 = visible [default], 1 = hidden)", 0},
    {'A', "SECURITY", F(security), "wpa2", "<mode>",
     "Security: open, wpa2 (default), wpa2wpa3, wpa3", 0},
    {'M', "PMF", F(pmf), "0", "<0|1>",
     "Protected Management Frames for wpa2 (0=off [default],\n"
     "1=on); forced on for wpa2wpa3, required for wpa3",
     0},
    {'T', "TTL_FIX", F(ttl_fix), "0", "<0|1>",
     "TTL fix: forwarded packets leave with TTL 64, so they\n"
     "look like the phone's own (0=off [default], 1=on).\n"
     "Needs the kernel's TTL target; skipped when missing.",
     0},
    {'6', "IPV6", F(ipv6), "1", "<0|1>",
     "IPv6 for clients (0=off, 1=on [default]): NAT66 on a\n"
     "private prefix when the kernel can, else the upstream's\n"
     "own /64 with an NDP proxy. Routed mode only.",
     0},
    {'K', "CONTAINER", F(container), "", "<container>",
     "Managed mode: hand the AP's LAN to a Droidspaces\n"
     "container (it owns DHCP/DNS/NAT). Disables -o/-g/-d.",
     0},
    {'m', "GW_MAC", F(gw_mac), "", "<mac>",
     "Managed mode: MAC of the container's LAN port, the\n"
     "clients' gateway (empty = a new random one on every\n"
     "start [default]).",
     0},
    {'S', "STANDALONE", F(standalone), "0", "<0|1>",
     "Standalone radio: create ap0 straight on the Wi-Fi chip\n"
     "without needing wlan0 up or connected, so the hotspot\n"
     "runs with the phone's Wi-Fi off (0=off [default], 1=on).",
     0},
};
const size_t vap_nkeys = sizeof(vap_keys) / sizeof(vap_keys[0]);

static char *opt_field(struct vap_opts *o, const struct vap_key *k) {
  return (char *)o + k->off;
}

const struct vap_key *opt_by_conf(const char *name) {
  for (size_t i = 0; i < vap_nkeys; i++)
    if (!strcmp(vap_keys[i].conf, name))
      return &vap_keys[i];
  return NULL;
}

static const struct vap_key *by_flag(int c) {
  for (size_t i = 0; i < vap_nkeys; i++)
    if (vap_keys[i].flag == c)
      return &vap_keys[i];
  return NULL;
}

int opt_set(struct vap_opts *o, const struct vap_key *k, const char *val) {
  if (strpbrk(val, "\r\n")) {
    vap_error("-%c cannot contain a line break.", k->flag);
    return -1;
  }
  if (strlen(val) >= OPT_LEN) {
    vap_error("-%c is longer than %d bytes.", k->flag, OPT_LEN - 1);
    return -1;
  }
  snprintf(opt_field(o, k), OPT_LEN, "%s",
           !*val && k->empty_is_default ? k->def : val);
  return 0;
}

void opts_defaults(struct vap_opts *o) {
  memset(o, 0, sizeof(*o));
  for (size_t i = 0; i < vap_nkeys; i++)
    snprintf(opt_field(o, &vap_keys[i]), OPT_LEN, "%s", vap_keys[i].def);
}

int opts_parse(struct vap_opts *o, int argc, char **argv) {
  char optstring[3 + 2 * sizeof(vap_keys) / sizeof(vap_keys[0])];
  size_t n = 0;
  /* "+": the first operand ends the options, the POSIX rule, on glibc too;
   * ":" so a missing value is told apart from an unknown flag. */
  optstring[n++] = '+';
  optstring[n++] = ':';
  for (size_t i = 0; i < vap_nkeys; i++) {
    optstring[n++] = vap_keys[i].flag;
    optstring[n++] = ':';
  }
  optstring[n] = '\0';

  int c;
  opterr = 0;
  optind = 1;
  while ((c = getopt(argc, argv, optstring)) != -1) {
    const struct vap_key *k = by_flag(c);
    if (c == ':' || c == '?' || !k) {
      if (c == ':')
        vap_error("-%c needs a value.", optopt);
      else
        vap_error("Unknown option -%c.", optopt);
      print_usage("virtualap");
      return -1;
    }
    if (opt_set(o, k, optarg) < 0)
      return -1;
  }
  o->args = argv + optind;
  return 0;
}

void print_usage(const char *prog) {
  /* Droidspaces' banner (print_usage), same layout. __DATE__ space-pads
   * single-digit days ("Nov  3 2026"), so the pad is skipped. */
  printf("\033[1mVirtualAP v" VAP_VERSION
         " - Wi-Fi access point engine for rooted Android\033[0m\n");
  printf("by \033[1;36mravindu644\033[0m\n");
  printf("\n\033[1;34mhttps://github.com/ravindu644/VirtualAP\033[0m\n");
  printf("\033[2mBuilt on: %.4s%s %s\033[0m\n\n", __DATE__,
         __DATE__ + 4 + (__DATE__[4] == ' '), __TIME__);
  printf("Usage: %s <start|stop|status|clients|interfaces|caps> [options]\n"
         "       %s block <mac> [name]\n"
         "       %s unblock <mac>\n"
         "       %s blocked\n"
         "       %s allow <mac> [name]\n"
         "       %s disallow <mac>\n"
         "       %s allowed\n"
         "       %s allow-only [on|off]\n"
         "\nstart options:\n",
         prog, prog, prog, prog, prog, prog, prog, prog);
  /* Start order, the one people read, not ap.conf's. */
  static const char order[] = "spobcWgdwHAMKmT6S";
  for (const char *f = order; *f; f++) {
    const struct vap_key *k = by_flag(*f);
    if (!k)
      continue;
    char head[32];
    snprintf(head, sizeof(head), "-%c %s", k->flag, k->arg);
    printf("  %-14s ", head);
    for (const char *h = k->help; *h; h++) {
      putchar(*h);
      if (*h == '\n')
        printf("%17s", "");
    }
    putchar('\n');
  }
  printf("\nSaved settings in %s are reused when flags are omitted.\n",
         VAP_CONF);
}
