/* start-ap's write_hostapd_conf, hostapd_try and start_hostapd, and the
 * client count of cmd_status.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "services/hostapd.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "core/fsutil.h"
#include "core/log.h"
#include "core/pidfile.h"
#include "core/proc.h"
#include "radio/wpa_ctrl.h"
#include "virtualap.h"

static void wpa_block(FILE *f, const char *pass, const char *mgmt, int pmf) {
  fprintf(f,
          "wpa=2\nwpa_passphrase=%s\nwpa_key_mgmt=%s\nrsn_pairwise=CCMP\n"
          "ieee80211w=%d\n",
          pass, mgmt, pmf);
}

struct conf {
  const struct vap_opts *o;
  const struct plan *p;
};

static void body(FILE *f, const void *arg) {
  const struct vap_opts *o = ((const struct conf *)arg)->o;
  const struct plan *p = ((const struct conf *)arg)->p;
  fprintf(f, "interface=" AP_IFACE "\ndriver=nl80211\nctrl_interface=%s\n",
          HOSTAPD_CTRL);
  fprintf(f, "ssid=%s\nhw_mode=%s\nchannel=%d\n", o->ssid,
          p->band == 5 ? "a" : "g", p->channel);
  if (p->band == 5) {
    /* 802.11n + ac. The width lines are what open up 40/80 MHz; ieee80211ac
     * alone leaves hostapd at 20. vht_capab stays unset for the widest chip
     * compatibility. */
    fputs("ieee80211n=1\nieee80211ac=1\n", f);
    if (p->width != 20) {
      int seg0 = vht_seg0(p->channel);
      /* no_pri_sec_switch skips hostapd's 20/40 coexistence scan, which
       * FullMAC chips (Broadcom on the Galaxy S10) cannot run on an AP and
       * answer with -95, aborting hostapd. Our hostapd fork wires it into
       * the config parser. */
      fprintf(f, "no_pri_sec_switch=1\nht_capab=[HT40%c]\nvht_oper_chwidth=%d\n",
              ht40_dir(p->channel, seg0), p->width == 80);
      if (p->width == 80)
        fprintf(f, "vht_oper_centr_freq_seg0_idx=%d\n", seg0);
    }
  } else {
    fputs("ieee80211n=1\n", f);
    /* The second channel sits four above the primary for 1-7 and four below
     * for 8-13, so the pair stays inside the band. */
    if (p->width == 40)
      fprintf(f, "no_pri_sec_switch=1\nht_capab=[HT40%c]\n",
              p->channel <= 7 ? '+' : '-');
  }
  fputs("country_code=US\nieee80211d=1\n", f);
  /* PMF: wpa2 honours -M (0/1), the WPA2/WPA3 transition needs 1 (capable),
   * WPA3 needs 2 (required). SAE needs hostapd built with CONFIG_SAE. */
  if (!strcmp(o->security, "wpa3"))
    wpa_block(f, o->password, "SAE", 2);
  else if (!strcmp(o->security, "wpa2wpa3"))
    wpa_block(f, o->password, "WPA-PSK SAE", 1);
  else if (strcmp(o->security, "open"))
    wpa_block(f, o->password, "WPA-PSK", !strcmp(o->pmf, "1"));
  fprintf(f, "ignore_broadcast_ssid=%s\n", o->hidden);
}

int hostapd_write_conf(const char *path, const struct vap_opts *o,
                       const struct plan *p) {
  struct conf c = {o, p};
  /* 0600: it holds the passphrase. */
  return write_atomic(path, 0600, body, &c);
}

/* Liveness is a fixed 2 s and a pid check, not a poll: with ieee80211d=1
 * hostapd can sit in COUNTRY_UPDATE for ~5 s, so a death after the check
 * (a late channel rejection by the firmware) shows in status and hostapd.log
 * instead. Polling slowed the common start for no real-world gain. */
static int try_once(void) {
  int fd = open(VAP_HOSTAPD_LOG, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                0644);
  if (fd >= 0)
    close(fd);
  /* -t -f: timestamped output to its own log, since -B daemonizes it away;
   * stderr goes there too for errors printed before that. */
  const char *const argv[] = {VAP_BIN "/hostapd", "-B", "-t", "-f",
                              VAP_HOSTAPD_LOG,    "-P", HOSTAPD_PID,
                              HOSTAPD_CONF,       NULL};
  if (run_logged(argv, VAP_HOSTAPD_LOG) != 0)
    return -1;
  struct timespec two = {2, 0};
  nanosleep(&two, NULL);
  return pidfile_ours(HOSTAPD_PID, "hostapd") ? 0 : -1;
}

int hostapd_start(const struct vap_opts *o, struct plan *p) {
  for (;;) {
    if (hostapd_write_conf(HOSTAPD_CONF, o, p) < 0) {
      vap_error("Cannot write %s.", HOSTAPD_CONF);
      return -1;
    }
    vap_log("Starting hostapd (%dGHz ch %d, %dMHz)...", p->band, p->channel,
            p->width);
    if (try_once() == 0) {
      vap_log("hostapd running (PID %d, %dMHz)",
              (int)pidfile_ours(HOSTAPD_PID, "hostapd"), p->width);
      return 0;
    }
    pidfile_kill(HOSTAPD_PID, "hostapd");
    /* 40 MHz on 2.4 GHz is the one width the radio can advertise and the
     * firmware still refuse, for one while the phone's own Wi-Fi sits on a
     * 20 MHz network on the same channel. A hotspot at 20 beats none. */
    if (p->band != 5 && p->width == 40) {
      vap_warn("hostapd refused 40MHz on 2.4GHz, retrying at 20MHz.");
      p->width = 20;
      continue;
    }
    vap_error("hostapd failed to start (check band/channel - try band 2).");
    return -1;
  }
}

int hostapd_clients(void) {
  char reply[4096];
  if (wpa_ctrl_request(HOSTAPD_CTRL "/" AP_IFACE, "STATUS", reply,
                       sizeof(reply), 1000) <= 0)
    return 0;
  /* num_sta[0]= counts hostapd's sta_list, the list list_sta walks. */
  const char *n = strstr(reply, "\nnum_sta[0]=");
  return n ? atoi(n + 12) : 0;
}
