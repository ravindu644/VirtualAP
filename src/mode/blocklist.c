/* The devices the hotspot refuses, kept across starts.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "internal.h"
#include "mode/mode.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "core/core.h"
#include "radio/radio.h"

/* About 400 devices. Adding stops a line short of it, so a full list can still
 * be read, and so be unblocked from. */
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

bool blocklist_mac(const char *s, char mac[18]) {
  /* s[2]: mac_take also reads twelve bare digits, which hostapd refuses. */
  return strcspn(s, " \t\n") == 17 && s[2] == ':' && mac_take(s, mac);
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
    if (kept < len || !blocklist_mac(line, mac)) {
      if (!shown && *line)
        vap_warn("Dropped from the blocklist: %s", line);
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
    vap_error("The blocklist is full.");
    return -1;
  }
  struct edit e = {list, mac, name};
  if (write_atomic(path, 0600, edit_body, &e) < 0) {
    vap_error("Cannot write %s.", path);
    return -1;
  }
  return 0;
}

int blocklist_add(const char *path, const char *mac, const char *name) {
  /* The name is only a label: one that cannot be shown is left out. */
  bool ok = name && strlen(name) < 64 && name_printable(name);
  return edit(path, mac, ok ? name : "");
}

int blocklist_del(const char *path, const char *mac) {
  return edit(path, mac, NULL);
}

int blocklist_print(const char *path) {
  char list[LIST_SIZE];
  if (list_read(path, list) < 0)
    return -1;
  entries(stdout, list, NULL, true);
  return 0;
}

int hostapd_deny(const char *mac, bool deny) {
  /* Off is fine: the next start reads the file. */
  if (!pidfile_ours(HOSTAPD_PID, "hostapd"))
    return 0;
  char cmd[64], reply[16];
  snprintf(cmd, sizeof(cmd), "DENY_ACL %s %s", deny ? "ADD_MAC" : "DEL_MAC",
           mac);
  /* ADD_MAC also drops the device if it is connected. */
  int n = wpa_ctrl_request(HOSTAPD_CTRL "/" AP_IFACE, cmd, reply, sizeof(reply),
                           1000);
  return n >= 2 && !strncmp(reply, "OK", 2) ? 0 : -1;
}
