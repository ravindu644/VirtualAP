/* The netlink layer against a real kernel, in a throwaway user and network
 * namespace, so it needs neither root nor a phone.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <sched.h>
#include <stdbool.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../src/net/internal.h"
#include "check.h"
#include "core/core.h"
#include "mode/rules.h"
#include "net/ipt/ipt.h"
#include "net/net.h"

static int ifreq_get(const char *name, unsigned long req, struct ifreq *ifr) {
  int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  memset(ifr, 0, sizeof(*ifr));
  snprintf(ifr->ifr_name, IFNAMSIZ, "%s", name);
  int ret = ioctl(s, req, ifr);
  close(s);
  return ret;
}

/* Root already (a phone): a network namespace is all it takes, and uids
 * outside 0 stay valid, which the Android 15 netd rule needs. */
static bool real_root;

static int map_root(void) {
  char buf[64];
  real_root = geteuid() == 0;
  if (real_root)
    return unshare(CLONE_NEWNET);
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
  static const char *const paths[] = {"/usr/sbin/ip", "/usr/bin/ip", "/sbin/ip",
                                      "/system/bin/ip"};
  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++)
    if (access(paths[i], X_OK) == 0)
      return paths[i];
  return NULL;
}

static int ip(const char *a, const char *b, const char *c, const char *d,
              const char *e, const char *f, const char *g, const char *h) {
  const char *argv[] = {ip_bin(), a, b, c, d, e, f, g, h, NULL};
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
  struct route rt = {AF_INET, "192.168.42.0/24", "vt-p",       "192.168.42.1",
                     97,      RTPROT_STATIC,     RT_SCOPE_LINK};
  CHECK(route_replace(&nl, &rt) == 0);
  CHECK(route_replace(&nl, &rt) == 0);
  CHECK(route_del(&nl, &rt) == 0);
  CHECK(route_del(&nl, &rt) == 0);
  struct route def = {AF_INET, NULL, "vt-p", NULL, 1031, 0, RT_SCOPE_LINK};
  CHECK(route_replace(&nl, &def) == 0);
  char oif[IF_NAMESIZE];
  CHECK(route_default_oif(&nl, AF_INET, 1031, oif) == 0 &&
        !strcmp(oif, "vt-p"));
  CHECK(route_default_oif(&nl, AF_INET, 1032, oif) == -ENOENT);
  int table = 0;
  CHECK(route_iface_table(&nl, AF_INET, "vt-p", &table) == 0 && table == 1031);
  CHECK(route_iface_table(&nl, AF_INET, "vt-h", &table) == -ENOENT);
  struct route r6 = {AF_INET6, "2001:db8:1:2::5/128", "vt-p", NULL, 0, 0, 0};
  CHECK(route_replace(&nl, &r6) == 0);

  /* Rules in every shape the AP writes, each idempotent. */
  struct rule to = {AF_INET, NULL, "192.168.42.0/24", NULL, NULL, 254,
                    7000,    false};
  struct rule from_ap = {AF_INET, NULL, NULL, "vt-p", NULL, 1031, 7010, false};
  struct rule sink = {AF_INET6, NULL, "2001:db8:1:2::/64", "vt-h", NULL, 0,
                      7001,     true};
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

  /* A routed AP's sets: IPv4 7000 and 7010, NAT66 those and 7002, a shared
   * /64 the 7001 sink on top; rules_del takes a family's back out. */
  rules_del(&nl, AF_INET);
  rules_del(&nl, AF_INET6);
  struct upstream ups = {.table = 1031};
  struct subnet sn;
  CHECK(subnet_from_gw("192.168.42.1", &sn));
  struct v6 v6 = {.mode = V6_NAT66, .net = "2001:db8:1:2::/64", .up = "vt-h"};
  CHECK(rules_add(&nl, &ups, &sn, &v6, true) == 0);
  struct rule want[] = {
      {AF_INET, NULL, "192.168.42.0/24", NULL, NULL, 254, 7000, false},
      {AF_INET, NULL, NULL, "ap0", NULL, 1031, 7010, false},
      {AF_INET6, NULL, "2001:db8:1:2::/64", NULL, NULL, 254, 7000, false},
      {AF_INET6, NULL, NULL, "ap0", NULL, 1031, 7010, false},
      {AF_INET6, NULL, NULL, NULL, "ap0", 254, 7002, false},
  };
  for (int i = 0; i < 5; i++)
    CHECK(rule_exists(&nl, &want[i]));
  /* An upstream switch takes 7010 alone, the return path stays. */
  rules_del_upstream(&nl);
  CHECK(!rule_exists(&nl, &want[1]) && !rule_exists(&nl, &want[3]));
  CHECK(rule_exists(&nl, &want[0]) && rule_exists(&nl, &want[2]) &&
        rule_exists(&nl, &want[4]));
  ups.table = 1032;
  CHECK(rules_add(&nl, &ups, &sn, &v6, true) == 0);
  struct rule moved4 = want[1], moved6 = want[3];
  moved4.table = moved6.table = 1032;
  CHECK(rule_exists(&nl, &moved4) && rule_exists(&nl, &moved6));
  CHECK(!rule_exists(&nl, &want[1]) && !rule_exists(&nl, &want[3]));
  rules_del_upstream(&nl);
  ups.table = 1031;
  CHECK(rules_add(&nl, &ups, &sn, &v6, true) == 0);
  CHECK(!rule_exists(&nl, &sink));
  v6.mode = V6_SHARED;
  CHECK(rules_add(&nl, &ups, NULL, &v6, true) == 0);
  CHECK(rule_exists(&nl, &sink));
  rules_del(&nl, AF_INET6);
  for (int i = 2; i < 5; i++)
    CHECK(!rule_exists(&nl, &want[i]));
  CHECK(!rule_exists(&nl, &sink) && rule_exists(&nl, &want[0]));
  rules_del(&nl, AF_INET);
  CHECK(!rule_exists(&nl, &want[0]) && !rule_exists(&nl, &want[1]));

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
  const char *bare[] = {ip_bin(), "rule",   "add",        "pref",
                        "22000",  "fwmark", "0x0/0xffff", "iif",
                        "lo",     "lookup", "1031",       NULL};
  const char *per_app[] = {ip_bin(), "rule",       "add",  "pref", "21000",
                           "fwmark", "0x0/0xffff", "iif",  "lo",   "uidrange",
                           "0-0",    "lookup",     "1099", NULL};
  CHECK(run_quiet(bare) == 0 && run_quiet(per_app) == 0);
  CHECK(netd_default_table(&nl, AF_INET, &table) == 0 && table == 1031);
  if (real_root) {
    /* Android 15+: the default network itself as a range over every uid. */
    const char *all_uids[] = {ip_bin(), "rule",     "add",          "pref",
                              "21500",  "fwmark",   "0x0/0xffff",   "iif",
                              "lo",     "uidrange", "0-2147483647", "lookup",
                              "1033",   NULL};
    CHECK(run_quiet(all_uids) == 0);
    CHECK(netd_default_table(&nl, AF_INET, &table) == 0 && table == 1033);
    const char *del[] = {ip_bin(), "rule", "del", "pref", "21500", NULL};
    CHECK(run_quiet(del) == 0);
  }

  struct upstream up;
  CHECK(upstream_resolve(&nl, "auto", &up, false) == 0 && up.table == 1031 &&
        !strcmp(up.iface, "vt-p"));
  CHECK(upstream_resolve(&nl, "vt-p", &up, false) == 0 && up.table == 1031);
  /* No default anywhere: the table of whatever route it has. */
  struct route lan = {AF_INET, "10.9.0.0/16", "vt-p", NULL, 1040,
                      0,       RT_SCOPE_LINK};
  CHECK(route_del(&nl, &def) == 0 && route_replace(&nl, &lan) == 0);
  CHECK(route_iface_table(&nl, AF_INET, "vt-p", &table) == 0 &&
        (table == 1040 || table == RT_TABLE_MAIN));
  CHECK(upstream_resolve(&nl, "nope0", &up, false) < 0);
  snprintf(up.iface, sizeof(up.iface), "v4-rmnet_data1");
  CHECK(!strcmp(upstream_v6_iface(&up), "rmnet_data1"));

proxy:
  /* The firewall rules against the kernel's own ip_tables, where there is
   * one (a phone; desktop kernels are nftables only). This namespace's
   * tables are its own, the phone's firewall is never touched. */
  if (ipt_rules_present(AF_INET, "vt-p", NULL) < 0) {
    fprintf(stderr, "check: no legacy ip_tables, iptables live test skipped\n");
  } else {
    for (int fam = AF_INET; fam; fam = fam == AF_INET ? AF_INET6 : 0) {
      const char *net =
          fam == AF_INET ? "192.168.42.0/24" : "fd76:6170:2a::/64";
      CHECK(ipt_rules_present(fam, "vt-p", NULL) == 0);
      CHECK(ipt_forward_accept(fam, "vt-p", true) == 0);
      CHECK(ipt_forward_accept(fam, "vt-p", true) == 0);
      CHECK(ipt_rules_present(fam, "vt-p", NULL) == 1);
      if (ipt_nat_available(fam)) {
        CHECK(ipt_masquerade(fam, net, true) == 0);
        CHECK(ipt_masquerade(fam, net, true) == 0);
        CHECK(ipt_rules_present(fam, "vt-p", net) == 1);
        CHECK(ipt_masquerade(fam, net, false) == 0);
        CHECK(ipt_rules_present(fam, "vt-p", net) == 0);
      } else {
        fprintf(stderr, "check: no %s nat table here\n",
                fam == AF_INET ? "IPv4" : "IPv6");
      }
      CHECK(ipt_forward_accept(fam, "vt-p", false) == 0);
      CHECK(ipt_rules_present(fam, "vt-p", NULL) == 0);
    }
  }

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
void test_netlink_in_netns(void) {
  fflush(NULL);
  pid_t pid = fork();
  if (pid == 0) {
    /* Its own count: failures before the fork are the parent's to report. */
    failed = 0;
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
