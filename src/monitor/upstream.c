/* The reprobe of Droidspaces' route monitor (network.c) and USB-Arsenal's
 * tether daemon: netd rewrites what a start set up, on every network switch.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "internal.h"
#include "monitor/monitor.h"

#include <stdio.h>
#include <string.h>

#include "cli/cli.h"
#include "core/core.h"
#include "mode/mode.h"
#include "net/ipt/ipt.h"
#include "virtualap.h"

/* r into run.state. The values a stop restores are the ones from before the
 * AP first touched them: the state's when it has them, else the ones this
 * setup just read (IPv6 off at start, or NAT66, which never read proxy_ndp). */
static void save(const struct routed *r) {
  struct state st;
  char buf[16];
  if (!state_load(VAP_STATE, &st))
    return;
  snprintf(buf, sizeof(buf), "%d", r->up.table);
  state_set(&st, "upstream_table", buf);
  state_set(&st, "upstream_iface", r->up.iface);
  struct v6 v = r->v6;
  const char *fwd = state_get(&st, "prev_ip6_forward");
  const char *proxy = state_get(&st, "prev_ip6_proxy_ndp");
  if (*fwd)
    snprintf(v.prev_fwd, sizeof(v.prev_fwd), "%s", fwd);
  if (*proxy)
    snprintf(v.prev_proxy, sizeof(v.prev_proxy), "%s", proxy);
  ipv6_save(&v, &st);
  state_save(VAP_STATE, &st);
}

/* netd flushes the FORWARD chain on a restart, and firewall apps rewrite
 * it; nat's MASQUERADE goes with the former. */
static void ensure_firewall(const struct routed *r) {
  if (ipt_rules_present(AF_INET, AP_IFACE, r->net.net) == 0) {
    vap_warn("Firewall rules were removed (netd restart?) - restoring them.");
    ipt_forward_accept(AF_INET, AP_IFACE, true);
    ipt_masquerade(AF_INET, r->net.net, true);
  }
  if (!r->v6.mode)
    return;
  const char *masq = r->v6.mode == V6_NAT66 ? r->v6.net : NULL;
  if (ipt_rules_present(AF_INET6, AP_IFACE, masq) == 0) {
    ipt_forward_accept(AF_INET6, AP_IFACE, true);
    if (masq)
      ipt_masquerade(AF_INET6, masq, true);
  }
}

static void ensure_forwarding(const struct routed *r) {
  char v[8];
  if (read_file("/proc/sys/net/ipv4/ip_forward", v, sizeof(v)) > 0 &&
      strcmp(v, "1"))
    write_file("/proc/sys/net/ipv4/ip_forward", "1");
  if (r->v6.mode) {
    conf6_get("all", "forwarding", v, sizeof(v));
    if (*v && strcmp(v, "1"))
      conf6_set("all", "forwarding", "1");
  }
}

static bool same_v6(const struct v6 *a, const struct v6 *b) {
  /* Off is off, whatever interface it was off on. */
  return a->mode == b->mode &&
         (!a->mode || (!strcmp(a->net, b->net) && !strcmp(a->up, b->up)));
}

/* The setup that last failed, not to be tried again until the network
 * offers a different one: retrying it every beat would restart dnsmasq
 * every 3 s for nothing. */
static struct v6 failed;

/* Tear IPv6 down and set it up as want says, dnsmasq and the TTL fix
 * following. */
static void ipv6_apply(struct nl *nl, const struct vap_opts *o,
                       struct routed *r, const struct v6 *want) {
  struct state st;
  if (state_load(VAP_STATE, &st))
    ipv6_teardown(nl, &st);
  ipv6_setup(nl, o, &r->up, &r->net, &r->v6);
  if (want->mode && !r->v6.mode)
    failed = *want;
  save(r);
  /* dnsmasq advertises a fixed prefix; a new one needs a new dnsmasq. */
  pidfile_kill(DNSMASQ_PID, "dnsmasq");
  struct ra ra = {r->v6.prefix, r->v6.gw, r->v6.mode == V6_SHARED};
  dnsmasq_start(&r->net, o->dns_servers, r->v6.mode ? &ra : NULL);
  /* The TTL fix names the IPv6 source prefix, which just changed. A kernel that
   * refused it at the start refuses it again here, quietly: the start said
   * so already. */
  if (!strcmp(o->ttl_fix, "1"))
    ttl_refresh(r->net.net, r->v6.mode ? r->v6.net : NULL);
}

void ipv6_off(struct nl *nl, const struct vap_opts *o, struct routed *r,
              const char *why) {
  vap_warn("IPv6: %s - IPv4 only.", why);
  failed = r->v6;
  struct vap_opts off = *o;
  strcpy(off.ipv6, "0");
  struct v6 none = {0};
  ipv6_apply(nl, &off, r, &none);
}

static bool redo_ipv6(struct nl *nl, const struct vap_opts *o,
                      struct routed *r) {
  struct v6 want;
  struct in6_addr upaddr;
  ipv6_decide(nl, o, &r->up, &r->net, &want, &upaddr);
  if (same_v6(&want, &r->v6))
    return false;
  /* NAT66 still holding on a new upstream interface: its prefix comes from
   * the IPv4 subnet and the MASQUERADE names no interface, so nothing needs
   * a redo, as Droidspaces' NAT66 rides a switch. The new upstream needs
   * accept_ra at 2, and 7010 goes back on its table straight after
   * reprobe's IPv4 one. Checked before `failed`: none of it can fail. */
  if (want.mode == V6_NAT66 && r->v6.mode == V6_NAT66) {
    snprintf(r->v6.up, sizeof(r->v6.up), "%s", want.up);
    ipv6_keep_ra(r->v6.up);
    rules_add(nl, &r->up, NULL, &r->v6, true);
    vap_log("IPv6: NAT66 now leaves through %s.", r->v6.up);
    return false;
  }
  if (want.mode && same_v6(&want, &failed))
    return false;
  vap_log("IPv6 changed (%s on %s -> %s on %s), redoing it.",
          r->v6.mode ? r->v6.net : "off", r->v6.mode ? r->v6.up : "-",
          want.mode ? want.net : "off", want.mode ? want.up : "-");
  ipv6_apply(nl, o, r, &want);
  return true;
}

bool reprobe(struct nl *nl, const struct vap_opts *o, struct routed *r) {
  struct upstream now;
  bool moved = false;
  if (upstream_resolve(nl, o->upstream, &now) < 0) {
    /* Auto keeps the old rule, there is nothing better to point it at and a
     * reconnect should not flap it. A pinned upstream that is gone takes its
     * rule with it: the table can be recycled for another network, and pinned
     * traffic must not leave through anything else. */
    if (strcmp(o->upstream, "auto") && r->up.table) {
      vap_log("Pinned upstream %s is gone - clients have no internet until it "
              "returns.",
              r->up.iface);
      rules_del_upstream(nl);
      memset(&r->up, 0, sizeof(r->up));
      moved = true;
    }
  } else if (now.table != r->up.table || strcmp(now.iface, r->up.iface)) {
    vap_log("Upstream changed: %s (table %d) -> %s (table %d)",
            r->up.table ? r->up.iface : "none", r->up.table, now.iface,
            now.table);
    /* IPv4 swapped back to back, as Droidspaces' monitor does; IPv6 follows
     * in redo_ipv6 once it knows what the new upstream offers. */
    rules_del_upstream(nl);
    r->up = now;
    rules_add(nl, &r->up, &r->net, NULL, true);
    moved = true;
  }
  /* IPv6 first, so stale IPv6 rules are not put back just to be removed. */
  bool redone = redo_ipv6(nl, o, r);
  /* A redo saved the state itself. */
  if (moved && !redone)
    save(r);
  /* ponytail: one rule dump per rule, a handful every beat; a single dump
   * checked against all of them if netd's rule list ever makes that show. */
  rules_add(nl, &r->up, &r->net, &r->v6, false);
  ensure_firewall(r);
  ensure_forwarding(r);
  return redone;
}
