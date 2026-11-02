/* The hostapd and dnsmasq configs as the engine writes them.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "check.h"
#include "core/fsutil.h"
#include "services/dnsmasq.h"
#include "services/hostapd.h"

static void hostapd_conf(const char *band, int ch, int width,
                         const char *security, char *out, size_t size) {
  struct vap_opts o;
  opts_defaults(&o);
  strcpy(o.ssid, "home");
  strcpy(o.password, "secret123");
  strcpy(o.security, security);
  struct plan p = {!strcmp(band, "5") ? 5 : 2, ch, width};
  char path[256];
  const char *dir = getenv("TMPDIR");
  snprintf(path, sizeof(path), "%s/vapconf.XXXXXX", dir && *dir ? dir : "/tmp");
  int fd = mkstemp(path);
  close(fd);
  CHECK(hostapd_write_conf(path, &o, &p) == 0);
  CHECK(read_file(path, out, size) > 0);
  unlink(path);
}

void test_services(void) {
  char c[2048];
  hostapd_conf("5", 36, 80, "wpa2", c, sizeof(c));
  CHECK(strstr(c, "\nhw_mode=a\nchannel=36\nieee80211n=1\nieee80211ac=1\n"
                  "no_pri_sec_switch=1\nht_capab=[HT40+]\nvht_oper_chwidth=1\n"
                  "vht_oper_centr_freq_seg0_idx=42\n"));
  CHECK(strstr(c, "\nwpa=2\nwpa_passphrase=secret123\nwpa_key_mgmt=WPA-PSK\n"
                  "rsn_pairwise=CCMP\nieee80211w=0\nignore_broadcast_ssid=0"));
  hostapd_conf("5", 40, 40, "wpa3", c, sizeof(c));
  CHECK(strstr(c, "ht_capab=[HT40-]\nvht_oper_chwidth=0\ncountry_code=US\n"));
  CHECK(strstr(c, "wpa_key_mgmt=SAE\nrsn_pairwise=CCMP\nieee80211w=2\n"));
  hostapd_conf("5", 149, 20, "wpa2wpa3", c, sizeof(c));
  CHECK(!strstr(c, "ht_capab") && strstr(c, "wpa_key_mgmt=WPA-PSK SAE\n"));
  CHECK(strstr(c, "ieee80211w=1\n"));
  hostapd_conf("2", 11, 40, "open", c, sizeof(c));
  CHECK(strstr(c, "hw_mode=g\nchannel=11\nieee80211n=1\nno_pri_sec_switch=1\n"
                  "ht_capab=[HT40-]\n"));
  CHECK(!strstr(c, "wpa"));

  struct subnet net;
  subnet_from_gw("192.168.42.1", &net);
  struct ra ra = {"2001:db8:1:2::", "2001:db8:1:2::1", true};
  char path[256];
  const char *dir = getenv("TMPDIR");
  snprintf(path, sizeof(path), "%s/vapconf.XXXXXX", dir && *dir ? dir : "/tmp");
  int fd = mkstemp(path);
  close(fd);
  CHECK(dnsmasq_write_conf(path, &net, " 9.9.9.9, ,1.1.1.1 ", &ra) == 0);
  CHECK(read_file(path, c, sizeof(c)) > 0);
  CHECK(strstr(c, "dhcp-range=192.168.42.10,192.168.42.50,12h\n"));
  CHECK(strstr(c, "enable-ra\ndhcp-range=2001:db8:1:2::,ra-only,off-link,64,"
                  "12h\ndhcp-option=option6:dns-server,[2001:db8:1:2::1]\n"));
  CHECK(strstr(c, "\nserver=9.9.9.9\nserver=1.1.1.1\nno-hosts\n"));
  CHECK(dnsmasq_write_conf(path, &net, "1.1.1.1", NULL) == 0);
  CHECK(read_file(path, c, sizeof(c)) > 0 && !strstr(c, "enable-ra"));
  unlink(path);
  CHECK(!dns_any(" , ,") && dns_any("8.8.8.8"));
}
