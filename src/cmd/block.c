/* The commands behind the app's blocked and allowed devices.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cmd/cmd.h"
#include "core/core.h"
#include "mode/mode.h"
#include "virtualap.h"

/* args[0] as a MAC, with the lock held: it keeps a start from reading a file
 * between our write and our word to hostapd, and two of these from losing each
 * other's entry. */
static bool take_mac(const struct vap_opts *o, char mac[18]) {
  const char *arg = o->args[0];
  if (!arg || strlen(arg) != 17 || !maclist_mac(arg, mac)) {
    vap_error("Expected a device's MAC address, like aa:bb:cc:dd:ee:ff.");
    return false;
  }
  return vap_lock() == 0;
}

static int refused(const char *mac) {
  vap_error("The running hotspot did not take %s, it stays on the list.", mac);
  return 1;
}

static bool allowlist_empty(void) {
  struct stat st;
  return stat(ALLOWLIST, &st) < 0 || st.st_size == 0;
}

/* The last device off the allowed list turns off only allowed devices: left on,
 * nobody could join, the owner included. Last, so a device being blocked is on
 * the blocked list before everyone else can get in. */
static int allow_only_follow(void) {
  if (access(ALLOW_ONLY, F_OK) < 0 || !allowlist_empty())
    return 0;
  if (allow_only_set(false) < 0)
    return -1;
  vap_log("Nobody is left on the allowed devices, everyone can join again.");
  return 0;
}

/* On one list only: hostapd reads the allowlist first, so a device on both
 * would get in though blocked. So the allowlist changes first, shrinking ahead
 * of a block and growing ahead of an unblock, in the file and in hostapd:
 * nothing that fails after it can let in a device still meant to be kept out. A
 * hotspot that does not answer does not stop the file edits, so the next start
 * has it right. */
static int add(const struct vap_opts *o, const char *list, const char *other,
               const char *done) {
  char mac[18];
  if (!take_mac(o, mac))
    return 1;
  bool allowing = !strcmp(list, ALLOWLIST), told = true;
  if (!allowing) {
    if (maclist_del(other, mac) < 0)
      return 1;
    told = hostapd_maclist(other, mac, false) == 0;
  }
  if (maclist_add(list, mac, o->args[1]) < 0)
    return 1;
  told = hostapd_maclist(list, mac, true) == 0 && told;
  if (allowing) {
    if (maclist_del(other, mac) < 0)
      return 1;
    told = hostapd_maclist(other, mac, false) == 0 && told;
  }
  if (allow_only_follow() < 0)
    return 1;
  if (!told)
    return refused(mac);
  vap_log("%s %s.", done, mac);
  return 0;
}

/* The file changes behind hostapd, so a hotspot that does not answer leaves the
 * device listed, where the app can show it and try again. */
static int del(const struct vap_opts *o, const char *list, const char *done) {
  char mac[18];
  if (!take_mac(o, mac))
    return 1;
  if (hostapd_maclist(list, mac, false) < 0)
    return refused(mac);
  if (maclist_del(list, mac) < 0 || allow_only_follow() < 0)
    return 1;
  vap_log("%s %s.", done, mac);
  return 0;
}

int cmd_block(struct vap_opts *o) {
  return add(o, BLOCKLIST, ALLOWLIST, "Blocked");
}

int cmd_unblock(struct vap_opts *o) { return del(o, BLOCKLIST, "Unblocked"); }

int cmd_allow(struct vap_opts *o) {
  return add(o, ALLOWLIST, BLOCKLIST, "Allowed");
}

int cmd_disallow(struct vap_opts *o) {
  return del(o, ALLOWLIST, "Removed from the allowed devices:");
}

/* One "mac name" line per device, "-" for no name. */
int cmd_blocked(struct vap_opts *o) {
  (void)o;
  return maclist_print(BLOCKLIST) < 0 ? 1 : 0;
}

int cmd_allowed(struct vap_opts *o) {
  (void)o;
  return maclist_print(ALLOWLIST) < 0 ? 1 : 0;
}

/* on or off to set it, nothing to ask. */
int cmd_allow_only(struct vap_opts *o) {
  const char *arg = o->args[0];
  if (!arg) {
    puts(access(ALLOW_ONLY, F_OK) == 0 ? "on" : "off");
    return 0;
  }
  bool on = !strcmp(arg, "on");
  if (!on && strcmp(arg, "off")) {
    vap_error("Expected on or off.");
    return 1;
  }
  if (vap_lock() < 0)
    return 1;
  /* With nobody on the list it would lock everyone out, the owner included. The
   * rewrite first: hostapd reads this file next, and a line it cannot parse
   * would fail that read after the list it has loaded is gone. It leaves only
   * entries, so a file with anything in it has a device. */
  if (on) {
    if (maclist_del(ALLOWLIST, "") < 0)
      return 1;
    if (allowlist_empty()) {
      vap_error("Add a device to the allowed devices first.");
      return 1;
    }
  }
  return allow_only_set(on) < 0 ? 1 : 0;
}
