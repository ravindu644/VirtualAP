/* The hostapd and dnsmasq configs as the engine writes them.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "../src/mode/internal.h"
#include "check.h"
#include "core/core.h"
#include "mode/mode.h"

static void hostapd_conf(const char *band, int ch, int width,
                         const char *security, char *out, size_t size) {
  struct vap_opts o;
  opts_defaults(&o);
  strcpy(o.ssid, "home");
  strcpy(o.password, "secret123");
  strcpy(o.security, security);
  struct plan p = {!strcmp(band, "5") ? 5 : 2, ch, width};
  char path[256];
  tmp_path(path, sizeof(path));
  int fd = mkstemp(path);
  CHECK(fd >= 0);
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
  tmp_path(path, sizeof(path));
  int fd = mkstemp(path);
  CHECK(fd >= 0);
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

  /* A real OpenWrt file: other LANs share it, and a MAC can be on two. */
  static const char leases[] =
      "1793831268 02:7c:8a:96:37:58 192.168.41.175 pine ff:8a:96\n"
      "1793846000 fc:93:6b:c2:3c:20 192.168.41.9 stale 01:fc:93\n"
      "1793846097 fc:93:6b:c2:3c:20 192.168.100.100 Ravindu-s-A16 01:fc:93\n"
      "1793848377 96:5a:64:7a:d0:d7 192.168.100.200 * 01:96:5a\n"
      "1793848378 96:5a:64:7a:d0:d8 192.168.100.201 b\x1b"
      "d 01:96:5a\n"
      "1793848379 96:5a:64:7a:d0:d9 192.168.100.evil x 01:96:5a\n"
      "1793848400 96:5a:64:7a:d0:da 192.168.100.2000 x 01:96:5a\n"
      "1793848401 96:5a:64:7a:d0:db 192.168.100.77\n"
      "1793848402 96:5a:64:7a:d0:dc 192.168.100.30 older 01:aa\n"
      "1793848403 96:5a:64:7a:d0:dc 192.168.100.31 newer 01:bb\n"
      "1793848401 96:5a:64:7a:d0:dc 192.168.100.32 oldest 01:cc\n"
      "duid 00:01:00:01:32:79:a0:de\n";
  const char *gw = "192.168.100.1";
  char ip[16], name[64];
  lease_find(leases, "fc:93:6b:c2:3c:20", gw, ip, name);
  CHECK(!strcmp(ip, "192.168.100.100") && !strcmp(name, "Ravindu-s-A16"));
  lease_find(leases, "96:5a:64:7a:d0:d7", gw, ip, name);
  CHECK(!strcmp(ip, "192.168.100.200") && !strcmp(name, "-"));
  lease_find(leases, "96:5a:64:7a:d0:d8", gw, ip, name);
  CHECK(!strcmp(ip, "192.168.100.201") && !strcmp(name, "-"));
  lease_find(leases, "96:5a:64:7a:d0:d9", gw, ip, name);
  CHECK(!strcmp(ip, "-") && !strcmp(name, "-"));
  lease_find(leases, "02:7c:8a:96:37:58", gw, ip, name);
  CHECK(!strcmp(ip, "-") && !strcmp(name, "-"));
  lease_find(leases, "96:5a:64:7a:d0:da", gw, ip, name);
  CHECK(!strcmp(ip, "-") && !strcmp(name, "-"));
  lease_find(leases, "96:5a:64:7a:d0:db", gw, ip, name);
  CHECK(!strcmp(ip, "-") && !strcmp(name, "-"));
  lease_find(leases, "96:5a:64:7a:d0:dc", gw, ip, name);
  CHECK(!strcmp(ip, "192.168.100.31") && !strcmp(name, "newer"));
  lease_find("", "02:7c:8a:96:37:58", gw, ip, name);
  CHECK(!strcmp(ip, "-"));

  /* What the container may leave in place of the file. */
  char root[256], tmp[300], file[320], back[32];
  tmp_path(root, sizeof(root));
  CHECK(mkdtemp(root));
  struct statfs fs;
  CHECK(statfs(root, &fs) == 0);
  snprintf(tmp, sizeof(tmp), "%s/tmp", root);
  snprintf(file, sizeof(file), "%s/dhcp.leases", tmp);
  CHECK(leases_read_under(root, fs.f_type, back, sizeof(back)) == -1);
  CHECK(mkdir(tmp, 0755) == 0);
  CHECK(leases_read_under(root, fs.f_type, back, sizeof(back)) == -1);
  FILE *f = fopen(file, "w");
  CHECK(f && fputs("lease\n", f) >= 0 && fclose(f) == 0);
  CHECK(leases_read_under(root, fs.f_type, back, sizeof(back)) == 5 &&
        !strcmp(back, "lease"));
  CHECK(leases_read_under(root, fs.f_type + 1, back, sizeof(back)) == -1);
  CHECK(unlink(file) == 0 && symlink("/proc/version", file) == 0);
  CHECK(leases_read_under(root, fs.f_type, back, sizeof(back)) == -1);
  CHECK(unlink(file) == 0 && mkfifo(file, 0600) == 0);
  CHECK(leases_read_under(root, fs.f_type, back, sizeof(back)) == -1);
  CHECK(unlink(file) == 0 && rmdir(tmp) == 0);
  CHECK(symlink("/", tmp) == 0);
  CHECK(leases_read_under(root, fs.f_type, back, sizeof(back)) == -1);
  CHECK(unlink(tmp) == 0 && rmdir(root) == 0);
}
