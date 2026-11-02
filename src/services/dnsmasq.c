/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "services/dnsmasq.h"

#include <stdio.h>
#include <unistd.h>

#include "core/csv.h"
#include "core/fsutil.h"
#include "core/log.h"
#include "core/pidfile.h"
#include "core/proc.h"

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

struct conf {
  const struct subnet *net;
  const char *dns;
  const struct ra *ra;
};

static void body(FILE *f, const void *arg) {
  const struct conf *c = arg;
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
  fprintf(f, "dhcp-leasefile=%s/dnsmasq.leases\nno-resolv\n", VAP_RUN);
  csv_each(c->dns, server_line, f);
  fprintf(f, "no-hosts\npid-file=%s\nuser=root\nlog-facility=%s\nlog-dhcp\n",
          DNSMASQ_PID, VAP_DNSMASQ_LOG);
}

int dnsmasq_write_conf(const char *path, const struct subnet *net,
                       const char *dns, const struct ra *ra) {
  struct conf c = {net, dns, ra};
  return write_atomic(path, 0644, body, &c);
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
