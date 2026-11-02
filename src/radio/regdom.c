/* start-ap's chan_no_ir and ensure_5g_ir.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "radio/regdom.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "core/log.h"
#include "core/proc.h"
#include "radio/wpa_ctrl.h"

#define WPA_CTRL_DIR "/data/vendor/wifi/wpa/sockets"

bool chan_no_ir(const struct phy_info *phy, int ch) {
  /* By frequency: 6 GHz numbers its channels from 1 again, and start-ap's
   * grep for "[36] ... no IR" could match a 6 GHz line. */
  int freq = chan_freq5(ch);
  for (int i = 0; i < phy->nfreqs; i++)
    if (phy->freqs[i].freq == freq)
      return phy->freqs[i].no_ir && !phy->freqs[i].disabled;
  return false;
}

/* The framework's country, upper case, "US" when it has none to give. `cmd
 * wifi get-country-code` is Android 11+ and prints null before telephony has
 * settled. */
static void country(char cc[3]) {
  static const char *const argv[] = {"/system/bin/cmd", "wifi",
                                     "get-country-code", NULL};
  char out[128];
  strcpy(cc, "US");
  if (run_capture(argv, out, sizeof(out)) != 0)
    return;
  char *eq = strrchr(out, '=');
  if (!eq)
    return;
  for (eq++; *eq == ' '; eq++)
    ;
  if (isalpha((unsigned char)eq[0]) && isalpha((unsigned char)eq[1]) &&
      (!eq[2] || isspace((unsigned char)eq[2]))) {
    cc[0] = (char)toupper((unsigned char)eq[0]);
    cc[1] = (char)toupper((unsigned char)eq[1]);
  }
}

void ensure_5g_ir(struct plan *p, const char *phy_iface, const struct sta *s,
                  struct phy_info *phy) {
  if (!chan_no_ir(phy, p->channel))
    return;
  char cc[3], sock[128], cmd[32];
  country(cc);
  vap_warn("5GHz channel %d is NO-IR (driver in world regdomain) - setting "
           "driver country to %s.",
           p->channel, cc);
  /* No wpa_cli on phones; the supplicant runs the command before it tries
   * to answer, and the answer never makes it back (SELinux). */
  snprintf(sock, sizeof(sock), "%s/%s", WPA_CTRL_DIR, phy_iface);
  snprintf(cmd, sizeof(cmd), "DRIVER COUNTRY %s", cc);
  wpa_ctrl_send(sock, cmd);
  struct timespec one = {1, 0};
  nanosleep(&one, NULL);
  /* A refresh that failed proves nothing: the channel stays NO-IR, and the
   * capabilities the width is planned from stay what they were. */
  struct phy_info fresh;
  bool ok = nl80211_phy_info(phy_iface, &fresh) == 0;
  if (ok)
    *phy = fresh;
  if (!ok || chan_no_ir(phy, p->channel)) {
    p->channel = s->band == 2 ? s->chan : 6;
    vap_warn("Channel still NO-IR after setting country - falling back to "
             "2.4GHz channel %d.",
             p->channel);
  }
}
