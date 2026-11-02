/* start-ap's config_network, start_services, teardown and the routed half
 * of cmd_start.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/routed.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli/state.h"
#include "core/fsutil.h"
#include "core/ipaddr.h"
#include "core/log.h"
#include "mode/ap.h"
#include "mode/ipv6.h"
#include "net/addr.h"
#include "net/ipt_rules.h"
#include "net/link.h"
#include "net/netd.h"
#include "net/route.h"
#include "net/rule.h"
#include "net/ttl.h"
#include "services/dnsmasq.h"
#include "services/hostapd.h"
#include "virtualap.h"

#define IP_FORWARD "/proc/sys/net/ipv4/ip_forward"

static int config_network(struct nl *nl, const struct vap_opts *o,
                          const struct upstream *up, const struct plan *p,
                          const struct subnet *net, struct v6 *v6) {
  if (ap_iface_create(o->phy_iface) < 0)
    return -1;
  vap_log("Configuring gateway address %s...", net->gw);
  link_set_up(nl, AP_IFACE, false);
  addr_flush(nl, AP_IFACE, AF_UNSPEC, false);
  if (addr_add(nl, AP_IFACE, AF_INET, net->gw, 24, false) < 0) {
    vap_error("Failed to assign %s", net->gw);
    return -1;
  }
  if (link_set_up(nl, AP_IFACE, true) < 0) {
    vap_error("Failed to bring up %s", AP_IFACE);
    return -1;
  }
  char prev_fwd[8] = "";
  read_file(IP_FORWARD, prev_fwd, sizeof(prev_fwd));
  if (write_file(IP_FORWARD, "1") < 0) {
    vap_error("Failed to enable IP forwarding");
    return -1;
  }

  vap_log("Installing routes and policy rules...");
  struct route main_rt = {AF_INET,       net->net,      AP_IFACE, net->gw,
                          RT_TABLE_MAIN, RTPROT_STATIC, RT_SCOPE_LINK};
  if (route_replace(nl, &main_rt) < 0) {
    vap_error("Failed to add subnet route to main");
    return -1;
  }
  /* Mirrored in local_network so container replies (from 172.28.0.0/16)
   * reach the clients through Droidspaces' rule 6095. */
  struct route local_rt = main_rt;
  local_rt.table = ANDROID_LOCAL_NETWORK_TABLE;
  if (route_replace(nl, &local_rt) < 0)
    vap_warn("Could not add subnet route to local_network table (%d) - port "
             "forwards from containers may not work",
             ANDROID_LOCAL_NETWORK_TABLE);
  /* Replies to the clients resolve via main, above every netd rule (VPN
   * catch-alls included); everything from them goes to the upstream's
   * table. */
  struct rule to = {AF_INET, NULL, net->net, NULL, NULL, RT_TABLE_MAIN,
                    PRIO_TO_SUBNET, false};
  struct rule from = {AF_INET, NULL, NULL, AP_IFACE, NULL, up->table,
                      PRIO_FROM_AP, false};
  if (rule_add(nl, &to) < 0)
    vap_warn("to-subnet rule failed");
  if (rule_add(nl, &from) < 0) {
    vap_error("Failed to add upstream steering rule (table %d)", up->table);
    return -1;
  }
  /* Scoped to ap0; the MASQUERADE names no interface, so it survives an
   * upstream change. */
  if (ipt_forward_accept(AF_INET, AP_IFACE, true) < 0) {
    vap_error("FORWARD rules failed");
    return -1;
  }
  if (ipt_masquerade(AF_INET, net->net, true) < 0) {
    vap_error("MASQUERADE insert failed");
    return -1;
  }

  /* IPv6 is best effort and leaves the IPv4 setup alone when it cannot. */
  ipv6_setup(nl, o, up, net, v6);

  /* Written before hostapd starts: status reads it, and a failed start's
   * teardown is driven by it. */
  struct state st = {0};
  char buf[32];
  state_set(&st, "upstream", o->upstream);
  snprintf(buf, sizeof(buf), "%d", up->table);
  state_set(&st, "upstream_table", buf);
  state_set(&st, "upstream_iface", up->iface);
  snprintf(buf, sizeof(buf), "%d", p->band);
  state_set(&st, "band", buf);
  snprintf(buf, sizeof(buf), "%d", p->channel);
  state_set(&st, "channel", buf);
  state_set(&st, "ssid", o->ssid);
  state_set(&st, "gateway", net->gw);
  state_set(&st, "dns_servers", o->dns_servers);
  state_set(&st, "prev_ip_forward", prev_fwd);
  ipv6_save(v6, &st);
  log_stamp(buf);
  state_set(&st, "started", buf);
  if (state_save(VAP_STATE, &st) < 0) {
    vap_error("Cannot write %s", VAP_STATE);
    return -1;
  }
  vap_log("Network configuration completed.");
  return 0;
}

void routed_teardown(const struct vap_opts *o) {
  struct state st;
  state_load(VAP_STATE, &st);
  /* The subnet from the state file: a restart with a new -g has already
   * saved the new gateway, and the rules to remove are the old one's. */
  struct subnet net;
  const char *gw = state_get(&st, "gateway");
  if (!subnet_from_gw(*gw ? gw : o->ip_gw, &net) &&
      !subnet_from_gw(GW_DEFAULT, &net))
    return;
  vap_log("Bringing down %s and reverting settings...", AP_IFACE);
  ap_daemons_stop();
  struct nl nl;
  if (nl_open(&nl) == 0) {
    rule_del_prio(&nl, AF_INET, PRIO_FROM_AP);
    rule_del_prio(&nl, AF_INET, PRIO_TO_SUBNET);
    ipv6_teardown(&nl, &st);
    struct route rt = {AF_INET,       net.net, AP_IFACE, NULL,
                       RT_TABLE_MAIN, 0,       0};
    route_del(&nl, &rt);
    rt.table = ANDROID_LOCAL_NETWORK_TABLE;
    route_del(&nl, &rt);
    nl_close(&nl);
  }
  ipt_masquerade(AF_INET, net.net, false);
  ipt_forward_accept(AF_INET, AP_IFACE, false);
  /* ip_forward goes back to 0 only if we were the ones who enabled it. */
  if (!strcmp(state_get(&st, "prev_ip_forward"), "0"))
    write_file(IP_FORWARD, "0");
  ap_iface_del();
  unlink(VAP_STATE);
  vap_log("AP stopped, cleanup completed.");
}

int routed_start(struct vap_opts *o, struct routed *r) {
  struct nl nl;
  struct upstream up;
  struct subnet net;
  struct plan p;
  struct v6 v6;
  if (nl_open(&nl) < 0) {
    vap_error("Cannot open a netlink socket.");
    return -1;
  }
  subnet_from_gw(o->ip_gw, &net);
  if (upstream_resolve(&nl, o->upstream, &up, true) < 0) {
    nl_close(&nl);
    return -1;
  }
  /* A -d with no address in it means the default pair; decided here so
   * dnsmasq and the state file agree. ap.conf keeps the value as given. */
  if (!dns_any(o->dns_servers))
    snprintf(o->dns_servers, sizeof(o->dns_servers), "%s", DNS_DEFAULT);
  ap_plan_radio(o, &p);
  ap_hold_signals();
  int ok = config_network(&nl, o, &up, &p, &net, &v6) == 0 && !ap_interrupted();
  nl_close(&nl);
  if (ok) {
    vap_log("Generating service configs (band %dGHz, channel %d)...", p.band,
            p.channel);
    struct ra ra = {v6.prefix, v6.gw, v6.mode == V6_SHARED};
    ok = hostapd_start(o, &p) == 0 && !ap_interrupted() &&
         dnsmasq_start(&net, o->dns_servers, v6.mode ? &ra : NULL) == 0 &&
         !ap_interrupted();
  }
  if (!ok) {
    routed_teardown(o);
    return -1;
  }

  vap_log("VirtualAP is ACTIVE!");
  vap_log("  SSID    : %s (%dGHz, channel %d, %dMHz)", o->ssid, p.band,
          p.channel, p.width);
  vap_log("  Gateway : %s (static - never changes)", net.gw);
  vap_log("  Upstream: %s (table %d)", up.iface[0] ? up.iface : o->upstream,
          up.table);
  if (v6.mode == V6_NAT66)
    vap_log("  IPv6    : %s, NAT66 out of %s", v6.net, v6.up);
  else if (v6.mode == V6_SHARED)
    vap_log("  IPv6    : %s shared from %s (global, NDP proxy, no NAT)",
            v6.net, v6.up);
  else
    vap_log("  IPv6    : off");

  /* Cleared first, so switching the option off takes effect on a restart. */
  ttl_clear();
  if (!strcmp(o->ttl_fix, "1")) {
    if (ttl_apply(net.net, v6.mode ? v6.net : NULL) == 0)
      vap_log("  TTL fix : forwarded packets leave with TTL 64");
    else
      vap_warn("TTL fix is not available: the kernel or iptables lacks the "
               "TTL target. Continuing without it.");
  }
  /* A stop that came during the last steps is still a stop. */
  if (ap_interrupted()) {
    routed_teardown(o);
    return -1;
  }
  r->up = up;
  r->net = net;
  r->v6 = v6;
  return 0;
}
