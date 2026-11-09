/* Host-side asserts for the engine's pure logic, run by `make check`.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "../src/cli/internal.h"
#include "../src/mode/internal.h"
#include "check.h"
#include "cli/cli.h"
#include "core/core.h"
#include "mode/mode.h"

int failed;

void tmp_path(char *out, size_t size) {
  const char *dir = getenv("TMPDIR");
  int n = snprintf(out, size, "%s/vapcheck.XXXXXX", dir && *dir ? dir : "/tmp");
  CHECK(n > 0 && (size_t)n < size);
}

static void test_ip4(void) {
  CHECK(valid_ip4("192.168.42.1"));
  CHECK(valid_ip4("0.0.0.0"));
  CHECK(!valid_ip4("1.2.3"));
  CHECK(!valid_ip4("1.2.3.4.5"));
  CHECK(!valid_ip4("01.2.3.4"));
  CHECK(!valid_ip4("256.1.1.1"));
  CHECK(!valid_ip4("1.2.3.4 "));
  CHECK(!valid_ip4(""));

  struct subnet s;
  CHECK(subnet_from_gw("10.0.7.1", &s));
  CHECK(!strcmp(s.gw, "10.0.7.1") && !strcmp(s.net, "10.0.7.0/24"));
  CHECK(!strcmp(s.begin, "10.0.7.10") && !strcmp(s.end, "10.0.7.50"));
  CHECK(s.third == 7);
  struct v6 v;
  nat66_prefix(&s, &v);
  CHECK(!strcmp(v.net, "fd76:6170:7::/64") && !strcmp(v.gw, "fd76:6170:7::1"));
  CHECK(!subnet_from_gw("10.0.7.0", &s));
  CHECK(!subnet_from_gw("10.0.7.255", &s));
  CHECK(!subnet_from_gw("10.0.7", &s));

  unsigned char bin[16];
  int len;
  CHECK(prefix_parse(AF_INET6, "2001:db8::/64", bin, &len) && len == 64);
  CHECK(prefix_parse(AF_INET, "10.0.0.1", bin, &len) && len == 32);
  CHECK(!prefix_parse(AF_INET6, "2001:db8::/", bin, &len));
  CHECK(!prefix_parse(AF_INET6, "2001:db8::/x", bin, &len));
  CHECK(!prefix_parse(AF_INET, "10.0.0.0/33", bin, &len));
}

/* Each value as the shell engine's sq() wrote it (generated with the real sq,
 * the sed that turns ' into '\''), so a downgrade reads our file and we read
 * its. */
static const struct {
  const char *value, *line;
} sq_fixture[] = {
    {"it's", "SSID='it'\\''s'"},
    {"a\"b", "SSID='a\"b'"},
    {"$HOME `x`", "SSID='$HOME `x`'"},
    {"back\\slash", "SSID='back\\slash'"},
    {"sp ace", "SSID='sp ace'"},
    {"\xe0\xb7\x81\xe0\xb7\x8a\xe2\x80\x8d\xe0\xb6\xbb\xe0\xb7\x93",
     "SSID='\xe0\xb7\x81\xe0\xb7\x8a\xe2\x80\x8d\xe0\xb6\xbb\xe0\xb7\x93'"},
    {"", "SSID=''"},
    {"''", "SSID=''\\'''\\'''"},
};

static void test_conf(void) {
  char path[256];
  tmp_path(path, sizeof(path));
  int fd = mkstemp(path);
  CHECK(fd >= 0);
  close(fd);
  for (size_t i = 0; i < sizeof(sq_fixture) / sizeof(sq_fixture[0]); i++) {
    struct vap_opts back;
    char file[256];
    snprintf(file, sizeof(file), "%s\n", sq_fixture[i].line);
    CHECK(write_file(path, file) == 0);
    opts_defaults(&back);
    strcpy(back.ssid, "stale");
    CHECK(conf_load(path, &back) == 0);
    CHECK(!strcmp(back.ssid, sq_fixture[i].value));
    CHECK(!strcmp(back.ip_gw, "192.168.42.1"));
  }
  unlink(path);

  /* A trailing backslash or an open quote must not walk off the end. */
  const char *s = "abc\\";
  char out[64];
  conf_unquote(&s, out, sizeof(out));
  CHECK(!strcmp(out, "abc\\"));
  s = "'abc";
  conf_unquote(&s, out, sizeof(out));
  CHECK(!strcmp(out, "abc"));
  /* # is a comment only where a word starts. */
  s = "Cafe#1 # note";
  conf_unquote(&s, out, sizeof(out));
  CHECK(!strcmp(out, "Cafe#1"));
  s = "#x";
  conf_unquote(&s, out, sizeof(out));
  CHECK(!strcmp(out, ""));

  /* An unterminated quote in a file never brings the line break with it. */
  char path2[256];
  tmp_path(path2, sizeof(path2));
  int fd2 = mkstemp(path2);
  CHECK(write(fd2, "PASSWORD='abc\r\nSSID='x\n", 23) == 23);
  close(fd2);
  struct vap_opts o;
  opts_defaults(&o);
  CHECK(conf_load(path2, &o) == 0);
  CHECK(!strpbrk(o.password, "\r\n") && !strpbrk(o.ssid, "\r\n"));
  CHECK(!strcmp(o.password, "abc") && !strcmp(o.ssid, "x"));
  unlink(path2);
}

static void test_opts(void) {
  struct vap_opts o;
  opts_defaults(&o);
  char *argv[] = {"start", "-s", "home", "-g", "", "-c", "", "-6", "0", NULL};
  CHECK(opts_parse(&o, 9, argv) == 0);
  CHECK(!strcmp(o.ssid, "home"));
  CHECK(!strcmp(o.ip_gw, "192.168.42.1"));
  CHECK(!strcmp(o.channel, ""));
  CHECK(!strcmp(o.ipv6, "0"));

  char *bad[] = {"start", "-s", "two\nlines", NULL};
  CHECK(opts_parse(&o, 3, bad) < 0);
  char *unknown[] = {"start", "-z", "1", NULL};
  CHECK(opts_parse(&o, 3, unknown) < 0);
  char *gw6[] = {"start", "-G", "2001:db8::1", NULL};
  CHECK(opts_parse(&o, 3, gw6) == 0 && !strcmp(o.ip6_gw, "2001:db8::1"));
}

static void test_state(void) {
  char path[256];
  tmp_path(path, sizeof(path));
  int fd = mkstemp(path);
  close(fd);
  struct state s = {0}, back;
  state_set(&s, "gateway", "192.168.42.1");
  state_set(&s, "ssid", "a=b");
  state_set(&s, "gateway", "10.0.0.1");
  CHECK(state_save(path, &s) == 0);
  CHECK(state_load(path, &back));
  CHECK(back.n == 2);
  CHECK(!strcmp(state_get(&back, "gateway"), "10.0.0.1"));
  CHECK(!strcmp(state_get(&back, "ssid"), "a=b"));
  CHECK(!strcmp(state_get(&back, "missing"), ""));
  unlink(path);
  CHECK(!state_load(path, &back));
}

static void test_rps(void) {
  char m[RPS_MASK_MAX], tiny[4];
  rps_mask_from("0-7\n", m, sizeof(m));
  CHECK(!strcmp(m, "fe"));
  rps_mask_from("0,2-5", m, sizeof(m));
  CHECK(!strcmp(m, "3e"));
  rps_mask_from("0-39", m, sizeof(m));
  CHECK(!strcmp(m, "ff,fffffffe"));
  rps_mask_from("0", m, sizeof(m));
  CHECK(!strcmp(m, ""));
  rps_mask_from("0-1023", m, sizeof(m));
  CHECK(strlen(m) == 32 * 8 + 31 && !strncmp(m, "ffffffff,", 9));
  rps_mask_from("0-39", tiny, sizeof(tiny));
  CHECK(!strcmp(tiny, ""));
}

static void test_ula(void) {
  char a[INET6_ADDRSTRLEN + 4], tiny[8];
  CHECK(ula_lan("fd12:3456:789a::/48", 42, a, sizeof(a)) &&
        !strcmp(a, "fd12:3456:789a:42::1/64"));
  /* 255 is 0x255, as ip6hint reads it; leading zeros and capitals parse. */
  CHECK(ula_lan("FD00:0bad:0001::/48", 255, a, sizeof(a)) &&
        !strcmp(a, "fd00:bad:1:255::1/64"));
  CHECK(ula_lan("fd12:3456:789a::/48", 0, a, sizeof(a)) &&
        !strcmp(a, "fd12:3456:789a::1/64"));
  CHECK(!ula_lan("fd12:3456:789a::", 42, a, sizeof(a)));
  CHECK(!ula_lan("fd12:3456:789a::/64", 42, a, sizeof(a)));
  CHECK(!ula_lan("fd12:3456:789a:1::/48", 42, a, sizeof(a)));
  CHECK(!ula_lan("fc12:3456:789a::/48", 42, a, sizeof(a)));
  CHECK(!ula_lan("2001:db8:1::/48", 42, a, sizeof(a)));
  CHECK(!ula_lan("fd64:7370::/48", 42, a, sizeof(a)));
  CHECK(!ula_lan("fd12:3456:789a::/48x", 42, a, sizeof(a)));
  CHECK(!ula_lan("", 42, a, sizeof(a)));
  CHECK(!ula_lan("fd12:3456:789a::/48", 42, tiny, sizeof(tiny)));
}

static void test_gw6(void) {
  char a[INET6_ADDRSTRLEN + 4], tiny[8];
  CHECK(gw6_lan("2001:db8::5", a, sizeof(a)) && !strcmp(a, "2001:db8::5/64"));
  CHECK(gw6_lan("2001:DB8::5/64", a, sizeof(a)) &&
        !strcmp(a, "2001:db8::5/64"));
  /* A bare /64 gets ::1, the gateway -u gives too. */
  CHECK(gw6_lan("3fff::", a, sizeof(a)) && !strcmp(a, "3fff::1/64"));
  CHECK(gw6_lan("2001:db8:0:5::/64", a, sizeof(a)) &&
        !strcmp(a, "2001:db8:0:5::1/64"));
  CHECK(gw6_lan("fd9b:eb8c:afef:42::1", a, sizeof(a)) &&
        !strcmp(a, "fd9b:eb8c:afef:42::1/64"));
  CHECK(gw6_lan("fc00::1", a, sizeof(a)) && !strcmp(a, "fc00::1/64"));
  /* Next to Droidspaces' /48, not in it. */
  CHECK(gw6_lan("fd64:7370:1::1", a, sizeof(a)));
  CHECK(!gw6_lan("fd64:7370::1", a, sizeof(a)));
  CHECK(!gw6_lan("fd64:7370:0:42::1", a, sizeof(a)));
  CHECK(!gw6_lan("2001:db8::1/48", a, sizeof(a)));
  CHECK(!gw6_lan("2001:db8::1/128", a, sizeof(a)));
  CHECK(!gw6_lan("2001:db8::1/64x", a, sizeof(a)));
  CHECK(!gw6_lan("fe80::1", a, sizeof(a)));
  CHECK(!gw6_lan("ff02::1", a, sizeof(a)));
  CHECK(!gw6_lan("::1", a, sizeof(a)));
  CHECK(!gw6_lan("::", a, sizeof(a)));
  CHECK(!gw6_lan("::ffff:192.0.2.1", a, sizeof(a)));
  CHECK(!gw6_lan("4000::1", a, sizeof(a)));
  CHECK(!gw6_lan("192.168.42.1", a, sizeof(a)));
  CHECK(!gw6_lan("", a, sizeof(a)));
  CHECK(!gw6_lan("2001:db8::5", tiny, sizeof(tiny)));
}

/* Android keeps sleep and echo in /system/bin, and has no /bin. */
static void test_capture_limit(void) {
  const char *dir = access("/bin/sleep", X_OK) == 0 ? "/bin" : "/system/bin";
  char sleep_bin[32], echo_bin[32], out[32];
  snprintf(sleep_bin, sizeof(sleep_bin), "%s/sleep", dir);
  snprintf(echo_bin, sizeof(echo_bin), "%s/echo", dir);
  const char *const slow[] = {sleep_bin, "30", NULL};
  const char *const quick[] = {echo_bin, "hello", NULL};
  time_t began = time(NULL);
  CHECK(run_capture_within(slow, out, sizeof(out), 100) == -1);
  CHECK(time(NULL) - began < 5);
  CHECK(run_capture_within(quick, out, sizeof(out), 5000) == 0 &&
        !strcmp(out, "hello"));
  CHECK(run_capture(quick, out, sizeof(out)) == 0 && !strcmp(out, "hello"));
}

int main(void) {
  log_set_file(NULL);
  /* opts_parse reports to stdout; keep the run quiet unless a check fails. */
  if (!freopen("/dev/null", "w", stdout))
    return 1;
  test_ip4();
  test_capture_limit();
  test_conf();
  test_opts();
  test_state();
  test_rps();
  test_ula();
  test_gw6();
  test_ipt();
  test_radio();
  test_services();
  test_netlink_in_netns();
  fprintf(stderr, failed ? "check: %d failed\n" : "check: ok\n", failed);
  return failed != 0;
}
