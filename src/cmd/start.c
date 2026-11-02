/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cli/conf.h"
#include "core/csv.h"
#include "cmd/cmd.h"
#include "core/ipaddr.h"
#include "core/lock.h"
#include "core/log.h"
#include "mode/ap.h"
#include "mode/routed.h"
#include "monitor/monitor.h"
#include "monitor/upstream.h"
#include "virtualap.h"

/* The first DNS entry that is no IPv4 address, into arg. */
static int bad_dns(const char *item, void *arg) {
  if (valid_ip4(item))
    return 0;
  snprintf(arg, OPT_LEN, "%s", item);
  return 1;
}

static int all_digits(const char *s) {
  for (; *s; s++)
    if (!isdigit((unsigned char)*s))
      return 0;
  return 1;
}

/* An out of range channel is not fatal: it goes back to auto, with a word. */
static void check_channel(struct vap_opts *o) {
  if (!*o->channel)
    return;
  if (!all_digits(o->channel)) {
    vap_warn("Channel '%s' is not a valid number. Resetting to auto.",
             o->channel);
    o->channel[0] = '\0';
    return;
  }
  long ch = strtol(o->channel, NULL, 10);
  if (!strcmp(o->band, "5")) {
    if (ch < 36) {
      vap_warn("Channel %ld is invalid for 5GHz band. Resetting to auto.", ch);
      o->channel[0] = '\0';
    }
  } else if (ch < 1 || ch > 14) {
    vap_warn("Channel %ld is invalid for 2.4GHz band. Resetting to auto.", ch);
    o->channel[0] = '\0';
  }
}

int cmd_start(struct vap_opts *o) {
  if (vap_lock() < 0)
    return 1;
  log_truncate();

  if (!*o->ssid) {
    vap_error("SSID not set (use -s or save a config first)");
    return 1;
  }
  struct subnet net;
  if (!subnet_from_gw(o->ip_gw, &net)) {
    vap_error("Invalid gateway IP address: '%s'", o->ip_gw);
    return 1;
  }
  /* Checked before ap.conf is written: dnsmasq would only refuse a bad entry
   * after hostapd is up, and the value would already be saved. */
  char entry[OPT_LEN];
  if (csv_each(o->dns_servers, bad_dns, entry)) {
    vap_error("Invalid DNS server: '%s'", entry);
    return 1;
  }
  if (strcmp(o->hidden, "1"))
    strcpy(o->hidden, "0");
  /* WPA-PSK and SAE passphrases are 8-63 bytes; open networks carry none. */
  size_t plen = strlen(o->password);
  if (strcmp(o->security, "open") && (plen < 8 || plen > 63)) {
    vap_error("Password must be 8-63 characters.");
    return 1;
  }
  check_channel(o);
  /* Refused before ap.conf is saved or a running AP is touched. */
  if (*o->container) {
    vap_error("Managed mode (-K) is not in the C engine yet.");
    return 1;
  }

  /* Saved now, before the channel policy rewrites band and channel and
   * before anything can fail: a failed attempt must not lose the config. */
  if (conf_save(VAP_CONF, o) < 0)
    vap_warn("Could not save %s.", VAP_CONF);

  /* Tear down any previous AP, and daemons orphaned by one that left no
   * state. */
  if (access(VAP_STATE, F_OK) == 0) {
    vap_log("Existing AP state found - restarting...");
    stop_ap(o);
  }
  ap_daemons_stop();
  if (ap_prerequisites() < 0)
    return 1;
  struct routed r;
  if (routed_start(o, &r) < 0)
    return 1;
  /* A shared /64 lives on the monitor's client discovery; without the
   * monitor the prefix must not stay advertised. */
  if (monitor_spawn(o, &r) < 0 && r.v6.mode == V6_SHARED) {
    struct nl nl;
    if (nl_open(&nl) == 0) {
      ipv6_off(&nl, o, &r, "no monitor for the client discovery");
      nl_close(&nl);
    }
  }
  return 0;
}
