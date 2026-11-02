/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/conf.h"
#include "cmd/cmd.h"
#include "core/ipaddr.h"
#include "core/lock.h"
#include "core/log.h"
#include "virtualap.h"

/* Each entry of a comma separated list, blanks removed, empties skipped, as
 * the script's word splitting did. Returns the first entry that fails ok(),
 * NULL when all pass. buf holds the entry. */
static const char *bad_entry(const char *list, char *buf, size_t size,
                             bool (*ok)(const char *)) {
  size_t n = 0;
  for (const char *p = list;; p++) {
    if (*p == ',' || !*p) {
      buf[n] = '\0';
      if (n && !ok(buf))
        return buf;
      n = 0;
      if (!*p)
        return NULL;
    } else if (!isspace((unsigned char)*p) && n + 1 < size) {
      buf[n++] = *p;
    }
  }
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
  if (bad_entry(o->dns_servers, entry, sizeof(entry), valid_ip4)) {
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

  /* Saved now, before the channel policy rewrites band and channel and
   * before anything can fail: a failed attempt must not lose the config. */
  if (conf_save(VAP_CONF, o) < 0)
    vap_warn("Could not save %s.", VAP_CONF);

  vap_error("start: the rest of the engine is not ported to C yet.");
  return 1;
}
