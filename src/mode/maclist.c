/* The devices the hotspot refuses and the ones it lets in, kept across starts.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "internal.h"
#include "mode/mode.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/core.h"
#include "radio/radio.h"

/* About 400 devices. Adding stops a line short of it, so a full list can still
 * be read, and so be taken from. */
#define LIST_SIZE 32768
#define LINE_SIZE 128

/* The whole list read before anything replaces it: a read that fails partway
 * must not become a shorter list. A list not there yet is empty. */
static int list_read(const char *path, char buf[LIST_SIZE]) {
  int n = read_file(path, buf, LIST_SIZE);
  if (n < 0 && errno == ENOENT) {
    buf[0] = '\0';
    return 0;
  }
  /* Only the file's own size can say it was cut: read_file also strips every
   * trailing newline, so a cut list can read short. */
  struct stat st;
  if (n < 0 || stat(path, &st) < 0 || st.st_size >= LIST_SIZE) {
    vap_error("Cannot read %s.", path);
    return -1;
  }
  return n;
}

bool maclist_mac(const char *s, char mac[18]) {
  /* s[2]: mac_take also reads twelve bare digits, which hostapd refuses. The
   * low bit of the first byte, an odd second digit, marks a group address: no
   * device sends from one, so it could only ever be a typo on a list. */
  return strcspn(s, " \t\n") == 17 && s[2] == ':' && mac_take(s, mac) &&
         !strchr("13579bdf", mac[1]);
}

/* Each entry but skip's. Only what is sure to be read passes: hostapd refuses to
 * start on a line it cannot parse. It is more forgiving than this, a MAC with
 * junk after it is a block to hostapd, so a rewrite says what it drops. shown is
 * the blocked command's form, otherwise the file's: hostapd takes what follows
 * the MAC as a VLAN id through atoi, which reads the # as 0, no VLAN, so it
 * reads this same file. */
static void entries(FILE *f, const char *list, const char *skip, bool shown) {
  for (const char *l = list, *nl; l; l = nl ? nl + 1 : NULL) {
    nl = strchr(l, '\n');
    char line[LINE_SIZE], mac[18], name[64] = "";
    size_t len = strcspn(l, "\n");
    size_t kept = len < sizeof(line) ? len : sizeof(line) - 1;
    memcpy(line, l, kept);
    line[kept] = '\0';
    if (kept < len || !maclist_mac(line, mac)) {
      if (!shown && *line)
        vap_warn("Dropped from a device list: %s", line);
      continue;
    }
    if (skip && !strcmp(mac, skip))
      continue;
    if (sscanf(line + 17, " #%63s", name) != 1 || !name_printable(name))
      name[0] = '\0';
    if (shown)
      fprintf(f, "%s %s\n", mac, *name ? name : "-");
    else if (*name)
      fprintf(f, "%s #%s\n", mac, name);
    else
      fprintf(f, "%s\n", mac);
  }
}

struct edit {
  const char *list, *mac, *name;
};

static void edit_body(FILE *f, const void *arg) {
  const struct edit *e = arg;
  entries(f, e->list, e->mac, false);
  if (e->name)
    fprintf(f, "%s%s%s\n", e->mac, *e->name ? " #" : "", e->name);
}

static int edit(const char *path, const char *mac, const char *name) {
  char list[LIST_SIZE];
  int n = list_read(path, list);
  if (n < 0)
    return -1;
  if (name && n > LIST_SIZE - 2 - LINE_SIZE) {
    vap_error("%s is full.", path);
    return -1;
  }
  struct edit e = {list, mac, name};
  if (write_atomic(path, 0600, edit_body, &e) < 0) {
    vap_error("Cannot write %s.", path);
    return -1;
  }
  return 0;
}

int maclist_add(const char *path, const char *mac, const char *name) {
  /* The name is only a label: one that cannot be shown is left out. */
  bool ok = name && strlen(name) < 64 && name_printable(name);
  return edit(path, mac, ok ? name : "");
}

int maclist_del(const char *path, const char *mac) {
  return edit(path, mac, NULL);
}

int maclist_print(const char *path) {
  char list[LIST_SIZE];
  if (list_read(path, list) < 0)
    return -1;
  entries(stdout, list, NULL, true);
  return 0;
}

static int hostapd_say(const char *cmd) {
  char reply[16];
  int n = wpa_ctrl_request(HOSTAPD_CTRL "/" AP_IFACE, cmd, reply, sizeof(reply),
                           1000);
  return n >= 2 && !strncmp(reply, "OK", 2) ? 0 : -1;
}

int hostapd_maclist(const char *path, const char *mac, bool add) {
  /* Off is fine: the next start reads the file. */
  if (!pidfile_ours(HOSTAPD_PID, "hostapd"))
    return 0;
  char cmd[64];
  /* DENY_ACL ADD_MAC drops the device if it is connected, and so does
   * ACCEPT_ACL DEL_MAC once only allowed devices can join. */
  snprintf(cmd, sizeof(cmd), "%s %s %s",
           !strcmp(path, BLOCKLIST) ? "DENY_ACL" : "ACCEPT_ACL",
           add ? "ADD_MAC" : "DEL_MAC", mac);
  return hostapd_say(cmd);
}

/* The order matters when turning it on. CLEAR drops everyone while macaddr_acl is
 * 1, so it goes to 0 first, for a repeat or a retry that finds it on already. CLEAR
 * is there because the file read adds to what is loaded. The file comes last: that
 * read is what drops everyone not on it, which needs macaddr_acl=1 by then. */
static int hostapd_allow_only(bool on) {
  if (!pidfile_ours(HOSTAPD_PID, "hostapd"))
    return 0;
  if (!on)
    return hostapd_say("SET macaddr_acl 0");
  return hostapd_say("SET macaddr_acl 0") < 0 ||
                 hostapd_say("ACCEPT_ACL CLEAR") < 0 ||
                 hostapd_say("SET macaddr_acl 1") < 0 ||
                 hostapd_say("SET accept_mac_file " ALLOWLIST) < 0
             ? -1
             : 0;
}

/* A file beside the lists and not a setting in ap.conf: like them it changes one
 * command at a time, hotspot on or off, and is not something a start passes. It
 * goes ahead of tightening and behind loosening, so a start in between is strict.
 * Turning on that a hotspot does not take is undone: the app must not say only
 * allowed devices can join while everyone still can, and a tap can try again. */
int allow_only_set(bool on) {
  if (on) {
    int fd = open(ALLOW_ONLY, O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) {
      vap_error("Cannot write %s.", ALLOW_ONLY);
      return -1;
    }
    close(fd);
  }
  if (hostapd_allow_only(on) < 0) {
    if (on) {
      /* It may have got as far as 1 with an empty list, which refuses everyone. */
      hostapd_say("SET macaddr_acl 0");
      unlink(ALLOW_ONLY);
    }
    vap_error("The running hotspot did not take it, it stays %s.",
              on ? "off" : "on");
    return -1;
  }
  if (!on && unlink(ALLOW_ONLY) < 0 && errno != ENOENT) {
    vap_error("Cannot remove %s.", ALLOW_ONLY);
    return -1;
  }
  return 0;
}
