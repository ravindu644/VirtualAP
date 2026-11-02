/* The OpenWrt LAN as one script run in the container: one droidspaces run
 * instead of thirty, the values as positional parameters.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/openwrt.h"

#include <stdio.h>
#include <string.h>

#include "core/log.h"
#include "mode/droidspaces.h"
#include "virtualap.h"

/* $1 gateway, $2 subnet, $3 port, $4 the third octet. Lines starting WARN
 * become warnings in the app's log, the rest plain lines. */
static const char script[] =
    "GW=$1 NET=$2 PORT=$3 HINT=$4\n"
    "uci set network.vaplan=interface\n"
    "uci set network.vaplan.device=\"$PORT\"\n"
    "uci set network.vaplan.proto=static\n"
    "uci set network.vaplan.ipaddr=\"$GW\"\n"
    "uci set network.vaplan.netmask=255.255.255.0\n"
    /* A /64 from the router's ULA; inert without one or an IPv6 upstream. */
    "uci set network.vaplan.ip6assign=64\n"
    /* netifd numbers those /64s in interface-name order and renumbers them
     * all when an interface is added, moving every hotspot client to a new
     * subnet; pinned to the IPv4 subnet's number unless set by hand. */
    "uci -q get network.vaplan.ip6hint >/dev/null 2>&1 ||"
    " uci set network.vaplan.ip6hint=\"$HINT\"\n"
    "uci commit network\n"
    "uci set dhcp.vaplan=dhcp\n"
    "uci set dhcp.vaplan.interface=vaplan\n"
    "uci set dhcp.vaplan.start=100\n"
    "uci set dhcp.vaplan.limit=150\n"
    "uci set dhcp.vaplan.leasetime=12h\n"
    /* Announced as the default router though the only prefix is a ULA: the
     * upstream is NAT66 and never delegates a public one. */
    "uci set dhcp.vaplan.ra=server\n"
    "uci set dhcp.vaplan.dhcpv6=server\n"
    "uci set dhcp.vaplan.ra_default=1\n"
    /* odhcpd advertises the port's MTU, 1508, and clients would send it. */
    "uci set dhcp.vaplan.ra_mtu=1500\n"
    "uci commit dhcp\n"
    "/etc/init.d/network reload >/dev/null 2>&1\n"
    "/etc/init.d/dnsmasq restart >/dev/null 2>&1\n"
    "/etc/init.d/odhcpd reload >/dev/null 2>&1\n"
    /* Most Android kernels lack nf_tables (fw4 fails) but have x_tables. */
    "sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1\n"
    "if iptables-legacy --version >/dev/null 2>&1; then\n"
    "  if [ -e /etc/init.d/firewall ]; then\n"
    "    /etc/init.d/firewall reload >/dev/null 2>&1\n"
    "    echo \"OpenWrt firewall (fw3) reloaded - '$PORT' masqueraded out the"
    " wan zone.\"\n"
    "  else\n"
    "    iptables-legacy -t nat -D POSTROUTING -s \"$NET\" ! -d \"$NET\" -j"
    " MASQUERADE 2>/dev/null\n"
    "    if iptables-legacy -t nat -A POSTROUTING -s \"$NET\" ! -d \"$NET\" -j"
    " MASQUERADE; then\n"
    "      echo \"OpenWrt NAT installed (iptables-legacy MASQUERADE).\"\n"
    "    else\n"
    "      echo \"WARN OpenWrt: failed to add iptables-legacy MASQUERADE"
    " rule.\"\n"
    "    fi\n"
    "  fi\n"
    "elif nft list ruleset >/dev/null 2>&1; then\n"
    "  /etc/init.d/firewall reload >/dev/null 2>&1\n"
    "  echo \"WARN OpenWrt is using nftables/fw4 - make sure '$PORT' sits in a"
    " masqueraded firewall zone.\"\n"
    "else\n"
    "  echo \"WARN OpenWrt has no iptables-legacy and no working nftables.\"\n"
    "  echo \"WARN Clients will get an IP but NO internet. Inside the"
    " container, run:\"\n"
    "  echo \"WARN     opkg update && opkg install iptables-zz-legacy"
    " firewall\"\n"
    "fi\n";

static void line(const char *text, void *arg) {
  (void)arg;
  if (!strncmp(text, "WARN ", 5))
    vap_warn("%s", text + 5);
  else
    vap_log("%s", text);
}

void openwrt_provision(const char *container, const struct subnet *net) {
  vap_log("Provisioning OpenWrt LAN (%s) on %s...", net->gw, VAP_PORT);
  char hint[4];
  snprintf(hint, sizeof(hint), "%u", net->third);
  const char *const args[] = {net->gw, net->net, VAP_PORT, hint, NULL};
  if (ds_run_script(container, script, args, line, NULL) != 0)
    vap_warn("OpenWrt provisioning hit issues - check the container.");
}
