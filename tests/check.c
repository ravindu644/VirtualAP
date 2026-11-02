/* Host-side asserts for the engine's pure logic, run by `make check`.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cli/conf.h"
#include "cli/options.h"
#include "cli/state.h"
#include "core/fsutil.h"
#include "core/ipaddr.h"
#include "core/log.h"
#include "core/proc.h"
#include "net/addr.h"
#include "net/link.h"
#include "net/neigh.h"
#include "net/netd.h"
#include "net/route.h"
#include "net/rule.h"

#include <fcntl.h>
#include <net/if.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

static int failed;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
      failed++;                                                                \
    }                                                                          \
  } while (0)

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
  char path[] = "/tmp/vapcheck.XXXXXX";
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

  /* The pre-1.0 double-quoted form. */
  const char *s = "\"a\\\"b \\$x\\\\\" # comment";
  char out[64];
  conf_unquote(&s, out, sizeof(out));
  CHECK(!strcmp(out, "a\"b $x\\"));

  /* A trailing backslash inside quotes must not walk off the end. */
  s = "\"abc\\";
  conf_unquote(&s, out, sizeof(out));
  CHECK(!strcmp(out, "abc\\"));
  /* # is a comment only where a word starts. */
  s = "Cafe#1 # note";
  conf_unquote(&s, out, sizeof(out));
  CHECK(!strcmp(out, "Cafe#1"));
  s = "#x";
  conf_unquote(&s, out, sizeof(out));
  CHECK(!strcmp(out, ""));

  /* An unterminated quote in a file never brings the line break with it. */
  char path2[] = "/tmp/vapcheck.XXXXXX";
  int fd2 = mkstemp(path2);
  CHECK(write(fd2, "PASSWORD=\"abc\\\"\r\nSSID='x\n", 26) == 26);
  close(fd2);
  struct vap_opts o;
  opts_defaults(&o);
  CHECK(conf_load(path2, &o) == 0);
  CHECK(!strpbrk(o.password, "\r\n") && !strpbrk(o.ssid, "\r\n"));
  CHECK(!strcmp(o.ssid, "x"));
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
  char path[] = "/tmp/vapstate.XXXXXX";
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

/* The netlink layer against a real kernel: a throwaway user and network
 * namespace, so it needs neither root nor a phone. */
static int ifreq_get(const char *name, unsigned long req, struct ifreq *ifr) {
  int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  memset(ifr, 0, sizeof(*ifr));
  snprintf(ifr->ifr_name, IFNAMSIZ, "%s", name);
  int ret = ioctl(s, req, ifr);
  close(s);
  return ret;
}

static int map_root(void) {
  char buf[64];
  uid_t uid = getuid();
  gid_t gid = getgid();
  if (unshare(CLONE_NEWUSER | CLONE_NEWNET) < 0)
    return -1;
  int fd = open("/proc/self/setgroups", O_WRONLY);
  if (fd >= 0) {
    if (write(fd, "deny", 4) < 0)
      return -1;
    close(fd);
  }
  snprintf(buf, sizeof(buf), "0 %d 1", (int)uid);
  fd = open("/proc/self/uid_map", O_WRONLY);
  if (fd < 0 || write(fd, buf, strlen(buf)) < 0)
    return -1;
  close(fd);
  snprintf(buf, sizeof(buf), "0 %d 1", (int)gid);
  fd = open("/proc/self/gid_map", O_WRONLY);
  if (fd < 0 || write(fd, buf, strlen(buf)) < 0)
    return -1;
  close(fd);
  return 0;
}

/* iproute2's ip, for the netd rules the engine itself never writes; NULL
 * when the host has none, and those checks are skipped. */
static const char *ip_bin(void) {
  static const char *const paths[] = {"/usr/sbin/ip", "/usr/bin/ip", "/sbin/ip"};
  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    if (access(paths[i], X_OK) == 0)
      return paths[i];
  return NULL;
}

static int ip(const char *a, const char *b, const char *c, const char *d,
              const char *e, const char *f, const char *g, const char *h) {
  char *argv[] = {(char *)ip_bin(), (char *)a, (char *)b, (char *)c, (char *)d,
                  (char *)e, (char *)f, (char *)g, (char *)h, NULL};
  return run_quiet(argv);
}

static void test_netlink(void) {
  struct nl nl;
  CHECK(nl_open(&nl) == 0);
  CHECK(link_set_up(&nl, "lo", true) == 0);

  /* Links: a bridge, a veth at the managed-mode MTU, a MAC, a master. */
  static const uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  struct ifreq ifr;
  CHECK(link_add_bridge(&nl, "vt-br0") == 0);
  CHECK(link_add_veth(&nl, "vt-h", "vt-p", -1, mac, 1508) == 0);
  CHECK(ifreq_get("vt-h", SIOCGIFMTU, &ifr) == 0 && ifr.ifr_mtu == 1508);
  CHECK(ifreq_get("vt-p", SIOCGIFMTU, &ifr) == 0 && ifr.ifr_mtu == 1508);
  CHECK(ifreq_get("vt-p", SIOCGIFHWADDR, &ifr) == 0 &&
        !memcmp(ifr.ifr_hwaddr.sa_data, mac, 6));
  CHECK(link_set_master(&nl, "vt-h", "vt-br0") == 0);
  CHECK(link_set_up(&nl, "vt-h", true) == 0);
  CHECK(link_set_up(&nl, "vt-p", true) == 0);
  CHECK(ifreq_get("vt-h", SIOCGIFFLAGS, &ifr) == 0 && (ifr.ifr_flags & IFF_UP));
  static const uint8_t mac2[6] = {0x02, 0, 0, 0, 0, 0x09};
  CHECK(link_set_mac(&nl, "vt-p", mac2) == 0);
  CHECK(link_set_master(&nl, "vt-h", NULL) == 0);

  /* A veth whose far end is born in another namespace, named and with its
   * MAC: here is "the container", the namespace we step into is the host. */
  int here = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
  CHECK(unshare(CLONE_NEWNET) == 0);
  struct nl host;
  CHECK(nl_open(&host) == 0);
  CHECK(link_add_veth(&host, "vt-h2", "vt-lan", here, mac, 1508) == 0);
  CHECK(if_nametoindex("vt-h2") > 0 && if_nametoindex("vt-lan") == 0);
  nl_close(&host);
  CHECK(setns(here, CLONE_NEWNET) == 0);
  close(here);
  CHECK(if_nametoindex("vt-lan") > 0);
  CHECK(ifreq_get("vt-lan", SIOCGIFHWADDR, &ifr) == 0 &&
        !memcmp(ifr.ifr_hwaddr.sa_data, mac, 6));

  /* Addresses. */
  struct addr_info a[16];
  int idx = (int)if_nametoindex("vt-p");
  CHECK(addr_add(&nl, "vt-p", AF_INET, "192.168.42.1", 24, false) == 0);
  CHECK(addr_add(&nl, "vt-p", AF_INET6, "2001:db8:1:2::1", 64, true) == 0);
  int n = addr_list(&nl, AF_INET, idx, a, 16);
  CHECK(n == 1 && !strcmp(a[0].addr, "192.168.42.1") && a[0].prefix == 24);
  n = addr_list(&nl, AF_INET6, idx, a, 16);
  CHECK(n >= 1);
  int global = 0;
  for (int i = 0; i < n; i++)
    global += a[i].scope == RT_SCOPE_UNIVERSE;
  CHECK(global == 1);
  CHECK(addr_flush(&nl, "vt-p", AF_INET6, true) == 0);
  n = addr_list(&nl, AF_INET6, idx, a, 16);
  for (int i = 0; i < n; i++)
    CHECK(a[i].scope != RT_SCOPE_UNIVERSE);
  CHECK(addr_flush(&nl, "vt-p", AF_UNSPEC, false) == 0);
  CHECK(addr_list(&nl, AF_INET, idx, a, 16) == 0);

  /* Routes: the AP's connected route in main and in local_network (97),
   * the lookups the upstream is resolved with. */
  CHECK(addr_add(&nl, "vt-p", AF_INET, "192.168.42.1", 24, false) == 0);
  struct route rt = {AF_INET, "192.168.42.0/24", "vt-p", "192.168.42.1", 97,
                     RTPROT_STATIC, RT_SCOPE_LINK};
  CHECK(route_replace(&nl, &rt) == 0);
  CHECK(route_replace(&nl, &rt) == 0);
  CHECK(route_del(&nl, &rt) == 0);
  CHECK(route_del(&nl, &rt) == 0);
  struct route def = {AF_INET, NULL, "vt-p", NULL, 1031, 0, RT_SCOPE_LINK};
  CHECK(route_replace(&nl, &def) == 0);
  char oif[IF_NAMESIZE];
  CHECK(route_default_oif(&nl, AF_INET, 1031, oif) == 0 && !strcmp(oif, "vt-p"));
  CHECK(route_default_oif(&nl, AF_INET, 1032, oif) == -ENOENT);
  int table = 0;
  CHECK(route_iface_table(&nl, AF_INET, "vt-p", &table) == 0 && table == 1031);
  CHECK(route_iface_table(&nl, AF_INET, "vt-h", &table) == -ENOENT);
  struct route r6 = {AF_INET6, "2001:db8:1:2::5/128", "vt-p", NULL, 0, 0, 0};
  CHECK(route_replace(&nl, &r6) == 0);

  /* Rules in every shape start-ap writes, each idempotent. */
  struct rule to = {AF_INET, NULL, "192.168.42.0/24", NULL, NULL, 254, 7000,
                    false};
  struct rule from_ap = {AF_INET, NULL, NULL, "vt-p", NULL, 1031, 7010, false};
  struct rule sink = {AF_INET6, NULL, "2001:db8:1:2::/64", "vt-h", NULL, 0,
                      7001, true};
  struct rule link = {AF_INET6, NULL, NULL, NULL, "vt-p", 254, 7002, false};
  CHECK(rule_add(&nl, &to) == 0 && rule_add(&nl, &to) == 0);
  CHECK(rule_add(&nl, &from_ap) == 0 && rule_add(&nl, &from_ap) == 0);
  CHECK(rule_add(&nl, &sink) == 0 && rule_add(&nl, &sink) == 0);
  CHECK(rule_add(&nl, &link) == 0);
  CHECK(rule_exists(&nl, &to) && rule_exists(&nl, &from_ap));
  CHECK(rule_exists(&nl, &sink) && rule_exists(&nl, &link));
  struct rule other_iif = from_ap;
  other_iif.iif = "vt-h";
  CHECK(!rule_exists(&nl, &other_iif));
  CHECK(rule_add(&nl, &other_iif) == 0);
  CHECK(rule_del_prio(&nl, AF_INET, 7010) == 0);
  CHECK(!rule_exists(&nl, &from_ap) && !rule_exists(&nl, &other_iif));
  CHECK(rule_exists(&nl, &to));
  CHECK(rule_del_prio(&nl, AF_INET, 7010) == 0);

  /* netd's default-network rule, bare, then behind a per-app uid range at a
   * higher precedence that must be skipped. Only uid 0 is mapped in this
   * namespace, so the kernel refuses Android 15's 0-2147483647 range here;
   * that one is checked on a phone. */
  CHECK(netd_default_table(&nl, AF_INET, &table) == -ENOENT);
  if (!ip_bin()) {
    fprintf(stderr, "check: no iproute2 ip, netd rules skipped\n");
    goto proxy;
  }
  CHECK(ip("rule", "add", "fwmark", "0x0/0xffff", "iif", "lo", "lookup",
           "1031") == 0);
  CHECK(netd_default_table(&nl, AF_INET, &table) == 0 && table == 1031);
  CHECK(ip("rule", "flush", NULL, NULL, NULL, NULL, NULL, NULL) == 0);
  char *bare[] = {(char *)ip_bin(), "rule", "add", "pref", "22000", "fwmark",
                  "0x0/0xffff", "iif", "lo", "lookup", "1031", NULL};
  char *per_app[] = {(char *)ip_bin(), "rule", "add", "pref", "21000",
                     "fwmark", "0x0/0xffff", "iif", "lo", "uidrange", "0-0",
                     "lookup", "1099", NULL};
  CHECK(run_quiet(bare) == 0 && run_quiet(per_app) == 0);
  CHECK(netd_default_table(&nl, AF_INET, &table) == 0 && table == 1031);

  struct upstream up;
  CHECK(upstream_resolve(&nl, "auto", &up) == 0 && up.table == 1031 &&
        !strcmp(up.iface, "vt-p"));
  CHECK(upstream_resolve(&nl, "vt-p", &up) == 0 && up.table == 1031);
  /* No default anywhere: the table of whatever route it has. */
  struct route lan = {AF_INET, "10.9.0.0/16", "vt-p", NULL, 1040, 0,
                      RT_SCOPE_LINK};
  CHECK(route_del(&nl, &def) == 0 && route_replace(&nl, &lan) == 0);
  CHECK(route_iface_table(&nl, AF_INET, "vt-p", &table) == 0 &&
        (table == 1040 || table == RT_TABLE_MAIN));
  CHECK(upstream_resolve(&nl, "nope0", &up) < 0);
  snprintf(up.iface, sizeof(up.iface), "v4-rmnet_data1");
  CHECK(!strcmp(upstream_v6_iface(&up), "rmnet_data1"));

proxy:
  /* NDP proxy entries. */
  CHECK(neigh_proxy(&nl, true, "2001:db8:1:2::5", "vt-p") == 0);
  CHECK(neigh_proxy(&nl, true, "2001:db8:1:2::5", "vt-p") == 0);
  CHECK(neigh_proxy(&nl, false, "2001:db8:1:2::5", "vt-p") == 0);
  CHECK(neigh_proxy(&nl, false, "2001:db8:1:2::5", "vt-p") == 0);

  CHECK(link_del(&nl, "vt-h") == 0 && link_del(&nl, "vt-h") == 0);
  CHECK(if_nametoindex("vt-p") == 0);
  CHECK(link_del(&nl, "vt-br0") == 0);
  nl_close(&nl);
}

/* In a child: the namespaces are its own, and its failures come back as its
 * exit code. Skipped where unprivileged namespaces are not allowed. */
static void test_netlink_in_netns(void) {
  fflush(NULL);
  pid_t pid = fork();
  if (pid == 0) {
    if (map_root() < 0)
      _exit(77);
    test_netlink();
    _exit(failed ? 1 : 0);
  }
  int st;
  waitpid(pid, &st, 0);
  if (WIFEXITED(st) && WEXITSTATUS(st) == 77)
    fprintf(stderr, "check: no unprivileged namespaces, netlink skipped\n");
  else if (!WIFEXITED(st) || WEXITSTATUS(st) != 0)
    failed++;
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
  test_netlink_in_netns();
  fprintf(stderr, failed ? "check: %d failed\n" : "check: ok\n", failed);
  return failed != 0;
}
