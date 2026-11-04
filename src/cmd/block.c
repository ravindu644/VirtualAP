/* The commands behind the app's blocked devices.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <string.h>

#include "cmd/cmd.h"
#include "core/core.h"
#include "mode/mode.h"
#include "virtualap.h"

/* The lock keeps a start from reading the file between our write and our word
 * to hostapd, and two of these from losing each other's entry. The list changes
 * ahead of a block and behind an unblock, so a hotspot that does not answer
 * leaves the device listed, where the app can show it and try again. */
static int apply(struct vap_opts *o, bool block) {
  char mac[18];
  const char *arg = o->args[0];
  if (!arg || strlen(arg) != 17 || !blocklist_mac(arg, mac)) {
    vap_error("Expected a MAC address like aa:bb:cc:dd:ee:ff.");
    return 1;
  }
  if (vap_lock() < 0)
    return 1;
  if (block && blocklist_add(BLOCKLIST, mac, o->args[1]) < 0)
    return 1;
  if (hostapd_deny(mac, block) < 0) {
    vap_error("The running hotspot did not take %s, it stays on the list.",
              mac);
    return 1;
  }
  if (!block && blocklist_del(BLOCKLIST, mac) < 0)
    return 1;
  vap_log("%s %s.", block ? "Blocked" : "Unblocked", mac);
  return 0;
}

int cmd_block(struct vap_opts *o) { return apply(o, true); }

int cmd_unblock(struct vap_opts *o) { return apply(o, false); }

/* One "mac name" line per device, "-" for no name. */
int cmd_blocked(struct vap_opts *o) {
  (void)o;
  return blocklist_print(BLOCKLIST) < 0 ? 1 : 0;
}
