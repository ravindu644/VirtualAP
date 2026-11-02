/* Host-side asserts for the engine's pure logic, run by `make check`.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "check.h"
#include "cli/conf.h"
#include "cli/options.h"
#include "cli/state.h"
#include "core/fsutil.h"
#include "core/ipaddr.h"
#include "core/log.h"
#include "mode/nat66.h"

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

/* Each value as the sh engine's sq() wrote it (generated with the real sq,
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
    struct vap_opts o, back;
    opts_defaults(&o);
    snprintf(o.ssid, sizeof(o.ssid), "%s", sq_fixture[i].value);
    CHECK(conf_save(path, &o) == 0);
    char file[4096], want[256];
    CHECK(read_file(path, file, sizeof(file)) > 0);
    snprintf(want, sizeof(want), "\n%s\n", sq_fixture[i].line);
    CHECK(strstr(file, want) != NULL);
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

int main(void) {
  log_set_file(NULL);
  /* opts_parse reports to stdout; keep the run quiet unless a check fails. */
  if (!freopen("/dev/null", "w", stdout))
    return 1;
  test_ip4();
  test_conf();
  test_opts();
  test_state();
  test_ipt();
  test_radio();
  test_services();
  test_netlink_in_netns();
  fprintf(stderr, failed ? "check: %d failed\n" : "check: ok\n", failed);
  return failed != 0;
}
