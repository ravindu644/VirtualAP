/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "internal.h"
#include "mode/mode.h"

#include <ctype.h>
#include <limits.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "cli/cli.h"
#include "core/core.h"
#include "net/ipt/ipt.h"
#include "net/net.h"
#include "radio/radio.h"
#include "virtualap.h"

static int server_line(const char *item, void *f) {
  fprintf(f, "server=%s\n", item);
  return 0;
}

static int any(const char *item, void *arg) {
  (void)item;
  (void)arg;
  return 1;
}

bool dns_any(const char *dns) { return csv_each(dns, any, NULL) != 0; }

struct dnsmasq_conf {
  const struct subnet *net;
  const char *dns;
  const struct ra *ra;
};

static void dnsmasq_body(FILE *f, const void *arg) {
  const struct dnsmasq_conf *c = arg;
  fprintf(f,
          "interface=" AP_IFACE "\nbind-interfaces\nexcept-interface=lo\n"
          "listen-address=%s\ndhcp-range=%s,%s,12h\n"
          "dhcp-option=option:router,%s\ndhcp-option=option:dns-server,%s\n",
          c->net->gw, c->net->begin, c->net->end, c->net->gw, c->net->gw);
  if (c->ra)
    /* off-link: clients send everything, the upstream LAN's own addresses
     * too, to us instead of resolving them on ap0. */
    fprintf(f,
            "enable-ra\ndhcp-range=%s,ra-only,%s64,12h\n"
            "dhcp-option=option6:dns-server,[%s]\n",
            c->ra->prefix, c->ra->off_link ? "off-link," : "", c->ra->dns);
  fprintf(f, "dhcp-leasefile=%s\nno-resolv\n", DNSMASQ_LEASES);
  csv_each(c->dns, server_line, f);
  fprintf(f, "no-hosts\npid-file=%s\nuser=root\nlog-facility=%s\nlog-dhcp\n",
          DNSMASQ_PID, VAP_DNSMASQ_LOG);
}

int dnsmasq_write_conf(const char *path, const struct subnet *net,
                       const char *dns, const struct ra *ra) {
  struct dnsmasq_conf c = {net, dns, ra};
  return write_atomic(path, 0644, dnsmasq_body, &c);
}

int dnsmasq_start(const struct subnet *net, const char *dns,
                  const struct ra *ra) {
  if (dnsmasq_write_conf(DNSMASQ_CONF, net, dns, ra) < 0) {
    vap_error("Cannot write %s.", DNSMASQ_CONF);
    return -1;
  }
  vap_log("Starting dnsmasq...");
  /* Its log-facility creates it when there is none. */
  write_file(VAP_DNSMASQ_LOG, "");
  /* stderr stays ours: what dnsmasq says before it daemonizes ("Address in
   * use") is the reason the start failed, and belongs in the app's log. */
  const char *const argv[] = {VAP_BIN "/dnsmasq", "-C", DNSMASQ_CONF, NULL};
  if (run_shown(argv) != 0) {
    vap_error("dnsmasq failed to start");
    return -1;
  }
  vap_log("dnsmasq running (PID %d)",
          (int)pidfile_ours(DNSMASQ_PID, "dnsmasq"));
  return 0;
}

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
      fprintf(f,
              "no_pri_sec_switch=1\nht_capab=[HT40%c]\nvht_oper_chwidth=%d\n",
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
  /* run_logged creates it when there is none. */
  write_file(VAP_HOSTAPD_LOG, "");
  /* -t -f: timestamped output to its own log, since -B daemonizes it away;
   * stderr goes there too for errors printed before that. */
  const char *const argv[] = {
      VAP_BIN "/hostapd", "-B",         "-t", "-f", VAP_HOSTAPD_LOG, "-P",
      HOSTAPD_PID,        HOSTAPD_CONF, NULL};
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
  /* Not STATUS's num_sta, which counts a device with the wrong password too.
   * ponytail: a station leaving in the middle of this walk ends it short, for
   * the one poll until the next; restart the walk on FAIL if that ever shows.
   */
  char mac[18];
  int n = 0;
  for (bool more = hostapd_station(NULL, mac); more;
       more = hostapd_station(mac, mac))
    n++;
  return n;
}

bool hostapd_station(const char *after, char mac[18]) {
  char cmd[32] = "STA-FIRST", reply[4096];
  if (after)
    snprintf(cmd, sizeof(cmd), "STA-NEXT %s", after);
  /* The MAC is the reply's first line. An empty reply is the end of the list.
   * FAIL is a station that left between two calls, and it ends the walk early;
   * the count changed with it, so the app asks again. */
  while (wpa_ctrl_request(HOSTAPD_CTRL "/" AP_IFACE, cmd, reply, sizeof(reply),
                          1000) > 0 &&
         mac_take(reply, mac)) {
    /* hostapd lists a station from its association, so one with the wrong
     * password is here too, with no [AUTHORIZED] and no keys. */
    if (strstr(reply, "[AUTHORIZED]"))
      return true;
    snprintf(cmd, sizeof(cmd), "STA-NEXT %s", mac);
  }
  return false;
}

/* A /24 always, so the prefix up to the last dot is the subnet. */
static bool on_gw_lan(const char *ip, const char *gw) {
  const char *dot = strrchr(gw, '.');
  return dot && valid_ip4(ip) && !strncmp(ip, gw, (size_t)(dot - gw) + 1);
}

void lease_find(const char *leases, const char *mac, const char *gw,
                char ip[16], char name[64]) {
  strcpy(ip, "-");
  strcpy(name, "-");
  long long newest = -1;
  for (const char *l = leases, *nl; l; l = nl ? nl + 1 : NULL) {
    nl = strchr(l, '\n');
    /* One line at a time, or a short line borrows its fields from the next.
     * The fields sit first, so a long client id cut off here costs nothing. */
    char line[128], m[19], a[17], n[64];
    long long expiry;
    size_t len = strcspn(l, "\n");
    if (len >= sizeof(line))
      len = sizeof(line) - 1;
    memcpy(line, l, len);
    line[len] = '\0';
    /* Each width is one past the longest good value, so a longer one fails the
     * checks below instead of splitting into the next field. A lease this MAC
     * still holds on another of the router's LANs must not show the wrong
     * address. A client that changed its client id holds two here, the newer
     * one is its own. */
    if (sscanf(line, "%lld %18s %16s %63s", &expiry, m, a, n) != 4)
      continue;
    /* dnsmasq writes 0 for a lease that never expires. */
    if (!expiry)
      expiry = LLONG_MAX;
    if (strcasecmp(m, mac) || !on_gw_lan(a, gw) || expiry <= newest)
      continue;
    newest = expiry;
    strcpy(ip, a);
    /* dnsmasq writes * for no hostname. In managed mode the container wrote
     * this file, so nothing but printable ASCII reaches the app. */
    bool ok = strcmp(n, "*") != 0;
    for (const char *c = n; *c; c++)
      ok = ok && isgraph((unsigned char)*c);
    strcpy(name, ok ? n : "-");
  }
}

void arp_find(const char *arp, const char *mac, const char *gw, char ip[16]) {
  /* ponytail: /proc/net/arp marks a stale entry complete too, so a device can
   * still show an address it just left; netlink's NUD state tells them apart,
   * at the price of entering the container's namespace. */
  char other[17] = "";
  for (const char *l = arp, *nl; l; l = nl ? nl + 1 : NULL) {
    nl = strchr(l, '\n');
    char a[17], m[19];
    unsigned flags;
    if (sscanf(l, "%16s %*s %x %18s", a, &flags, m) != 3 ||
        !(flags & ATF_COM) || strcasecmp(m, mac) || !on_gw_lan(a, gw))
      continue;
    if (!strcmp(a, ip))
      return;
    if (!*other)
      strcpy(other, a);
  }
  if (*other)
    strcpy(ip, other);
}

void ap_iface_del(void) {
  if (nl80211_del(AP_IFACE) == 0)
    return;
  struct nl nl;
  if (nl_open(&nl) == 0) {
    link_del(&nl, AP_IFACE);
    nl_close(&nl);
  }
}

int ap_iface_create(const char *phy_iface) {
  vap_log("Creating virtual AP interface '%s' on %s...", AP_IFACE, phy_iface);
  ap_iface_del();
  int ret = nl80211_add_ap(phy_iface, AP_IFACE);
  if (ret < 0) {
    /* What iw printed, which the app's log showed. */
    vap_log("command failed: %s (%d)", strerror(-ret), ret);
    vap_error("Failed to create %s - chip/driver may not support AP mode.",
              AP_IFACE);
    return -1;
  }
  if (!if_nametoindex(AP_IFACE)) {
    vap_error("%s was not created successfully", AP_IFACE);
    return -1;
  }
  return 0;
}

void ap_daemons_stop(void) {
  pidfile_kill(HOSTAPD_PID, "hostapd");
  pidfile_kill(DNSMASQ_PID, "dnsmasq");
  unlink(HOSTAPD_CONF);
  unlink(DNSMASQ_CONF);
}

int ap_prerequisites(void) {
  static const char *const bins[] = {VAP_BIN "/hostapd", VAP_BIN "/dnsmasq"};
  for (int i = 0; i < 2; i++)
    if (access(bins[i], X_OK) < 0) {
      vap_error("%s not found", bins[i]);
      return -1;
    }
  return 0;
}

void ap_plan_radio(const struct vap_opts *o, struct plan *p) {
  struct phy_info phy;
  struct sta sta;
  int freq = nl80211_sta_freq(o->phy_iface);
  sta_from_freq(freq > 0 ? freq : 0, &sta);
  nl80211_phy_info(o->phy_iface, &phy);
  plan_init(p, o->band, o->channel);
  /* 5 GHz needs the regdomain to allow beaconing there, and ensure_5g_ir may
   * itself fall back to 2.4 GHz, so the band follows the final channel. */
  if (plan_channel(p, &sta))
    ensure_5g_ir(p, o->phy_iface, &sta, &phy);
  plan_band(p);
  plan_width(p, o->width, &phy);
}

int ap_state_save(struct state *st, const struct vap_opts *o,
                  const struct plan *p, const struct subnet *net) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", p->band);
  state_set(st, "band", buf);
  snprintf(buf, sizeof(buf), "%d", p->channel);
  state_set(st, "channel", buf);
  state_set(st, "ssid", o->ssid);
  state_set(st, "gateway", net->gw);
  log_stamp(buf);
  state_set(st, "started", buf);
  if (state_save(VAP_STATE, st) < 0) {
    vap_error("Cannot write %s", VAP_STATE);
    return -1;
  }
  return 0;
}

void ap_ttl(const struct vap_opts *o, const char *src4, const char *src6) {
  /* ttl_apply clears first itself. */
  if (strcmp(o->ttl_fix, "1")) {
    ttl_clear();
    return;
  }
  if (ttl_apply(src4, src6) == 0)
    vap_log("  TTL fix : forwarded packets leave with TTL 64");
  else
    vap_warn("TTL fix is not available: the kernel or iptables lacks the "
             "TTL target. Continuing without it.");
}

void ap_hold_signals(void) {
  sigset_t hold;
  sigemptyset(&hold);
  sigaddset(&hold, SIGINT);
  sigaddset(&hold, SIGTERM);
  sigprocmask(SIG_BLOCK, &hold, NULL);
}

bool ap_interrupted(void) {
  sigset_t pending;
  sigpending(&pending);
  return sigismember(&pending, SIGINT) || sigismember(&pending, SIGTERM);
}
