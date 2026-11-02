/* The veth born in the container and its re-plug after a restart are
 * Droidspaces' and USB-Arsenal's gateway mode.
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/managed.h"

#include <ctype.h>
#include <errno.h>
#include <net/if.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "cli/state.h"
#include "core/fsutil.h"
#include "core/ipaddr.h"
#include "core/log.h"
#include "mode/ap.h"
#include "mode/droidspaces.h"
#include "mode/ipv6.h"
#include "mode/openwrt.h"
#include "mode/rps.h"
#include "net/link.h"
#include "net/netns.h"
#include "net/ttl.h"
#include "radio/wifi_mac.h"
#include "services/hostapd.h"
#include "virtualap.h"

/* The mask is Droidspaces' ds_net_steer_rx, built in 32-bit words for up to
 * 1024 CPUs. */
void rps_mask_from(const char *present, char *mask, size_t size) {
  mask[0] = '\0';
  /* "0-7" or "0,2-5": the highest CPU id is the last number. */
  const char *p = present + strlen(present);
  while (p > present && !isdigit((unsigned char)p[-1]))
    p--;
  while (p > present && isdigit((unsigned char)p[-1]))
    p--;
  int max = atoi(p);
  if (max < 1 || max > 1023)
    return;
  size_t off = 0;
  for (int w = max / 32; w >= 0; w--) {
    uint32_t bits = 0;
    for (int b = 0; b < 32; b++) {
      int cpu = w * 32 + b;
      if (cpu >= 1 && cpu <= max)
        bits |= 1u << b;
    }
    int n =
        snprintf(mask + off, size - off, w == max / 32 ? "%x" : ",%08x", bits);
    /* A short buffer gives no mask rather than a wrong one. */
    if (n < 0 || (size_t)n >= size - off) {
      mask[0] = '\0';
      return;
    }
    off += (size_t)n;
  }
}

void rps_mask(char *mask, size_t size) {
  char buf[64];
  mask[0] = '\0';
  if (read_file("/sys/devices/system/cpu/present", buf, sizeof(buf)) >= 0)
    rps_mask_from(buf, mask, size);
}

static bool write_mask(const char *sys, const char *ifname, const char *mask) {
  char path[160];
  snprintf(path, sizeof(path), "%s/class/net/%s/queues/rx-0/rps_cpus", sys,
           ifname);
  return write_file(path, mask) == 0;
}

bool rps_host(const char *ifname, const char *mask) {
  return write_mask("/sys", ifname, mask);
}

bool rps_in_netns(int netns_fd, const char *ifname, const char *mask) {
  fflush(stdout);
  pid_t pid = fork();
  if (pid == 0) {
    /* A mount namespace of our own: the sysfs mounted here, which shows the
     * namespace it was mounted from, is gone when this child is. */
    if (setns(netns_fd, CLONE_NEWNET) < 0 || unshare(CLONE_NEWNS) < 0 ||
        mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) < 0 ||
        mount("sysfs", VAP_RUN, "sysfs", MS_NOSUID | MS_NODEV | MS_NOEXEC,
              NULL) < 0)
      _exit(1);
    _exit(write_mask(VAP_RUN, ifname, mask) ? 0 : 1);
  }
  int st;
  return pid > 0 && waitpid(pid, &st, 0) == pid && WIFEXITED(st) &&
         WEXITSTATUS(st) == 0;
}

/* A link just created answers EPERM for a moment; six tries, 0.3 s apart. */
static int retry(int (*fn)(struct nl *, const char *, const char *),
                 struct nl *nl, const char *a, const char *b) {
  struct timespec wait = {0, 300 * 1000 * 1000};
  int ret = 0;
  for (int i = 0; i < 6; i++) {
    if ((ret = fn(nl, a, b)) == 0)
      return 0;
    nanosleep(&wait, NULL);
  }
  return ret;
}

/* retry() passes two names; link_set_up takes a bool. */
static int up(struct nl *nl, const char *ifname, const char *unused) {
  (void)unused;
  return link_set_up(nl, ifname, true);
}

/* Inside the container: its port up, and its MAC read back. */
struct port {
  char mac[18];
};

static int port_up(void *arg) {
  struct port *p = arg;
  struct nl nl;
  if (nl_open(&nl) < 0)
    return -1;
  /* Born in the same request as the host end, so just as new. */
  int ret = retry(up, &nl, VAP_PORT, NULL);
  nl_close(&nl);
  struct ifreq ifr = {0};
  snprintf(ifr.ifr_name, IFNAMSIZ, "%s", VAP_PORT);
  int s = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (s >= 0 && ioctl(s, SIOCGIFHWADDR, &ifr) == 0) {
    const unsigned char *m = (const unsigned char *)ifr.ifr_hwaddr.sa_data;
    snprintf(p->mac, sizeof(p->mac), "%02x:%02x:%02x:%02x:%02x:%02x", m[0],
             m[1], m[2], m[3], m[4], m[5]);
  }
  if (s >= 0)
    close(s);
  return ret;
}

/* The fixed gateway MAC (-m) as six bytes, NULL to keep a random one. ap0 is
 * a port on the same bridge, and a bridge keeps frames for its ports' own
 * MACs, so a gateway sharing ap0's would never see a client. */
static const uint8_t *gw_mac(const char *want, uint8_t out[6]) {
  char mac[18], ap[32];
  if (!*want)
    return NULL;
  if (!mac_take(want, mac) ||
      sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &out[0], &out[1], &out[2],
             &out[3], &out[4], &out[5]) != 6 ||
      (out[0] & 1)) {
    vap_warn("Could not set the gateway MAC to %s, keeping a random one.",
             want);
    return NULL;
  }
  if (read_file("/sys/class/net/" AP_IFACE "/address", ap, sizeof(ap)) > 0 &&
      !strcmp(ap, mac)) {
    vap_warn("The gateway MAC %s is the hotspot radio's own, keeping a random "
             "one.",
             want);
    return NULL;
  }
  return out;
}

/* The container process and namespace the port went into. Set by the start,
 * and inherited by the monitor it forks, so the monitor starts from the
 * namespace actually used rather than whatever is there by its first look. */
static pid_t plugged_pid;
static ino_t plugged_ns;

/* The veth: the host end a bridge port, the container's end born inside its
 * namespace as VAP_PORT, with the gateway MAC and the MIC-sized MTU, in one
 * request. A link moved in and renamed afterwards shows up under its old
 * name first on kernels before 5.x, and OpenWrt's netifd never looks again.
 * Fills mac with the container end's MAC. */
static int plug(const struct vap_opts *o, pid_t pid, struct nl *nl,
                char mac[18]) {
  int fd = netns_open(pid);
  if (fd < 0) {
    vap_error(fd == -EINVAL
                  ? "Container '%s' shares the host's network - it has "
                    "no LAN port to give."
                  : "Cannot open container '%s''s network namespace.",
              o->container);
    return -1;
  }
  link_del(nl, VAP_VETH_HOST);
  uint8_t bytes[6];
  const uint8_t *fixed = gw_mac(o->gw_mac, bytes);
  if (link_add_veth(nl, VAP_VETH_HOST, VAP_PORT, fd, fixed, VAP_MTU) < 0) {
    vap_error("Failed to create the veth pair into container '%s'.",
              o->container);
    close(fd);
    return -1;
  }
  if (fixed)
    vap_log("Gateway MAC fixed at %s.", o->gw_mac);
  struct port p = {""};
  if (retry(link_set_master, nl, VAP_VETH_HOST, VAP_BRIDGE) < 0 ||
      retry(up, nl, VAP_VETH_HOST, NULL) < 0) {
    vap_error("Failed to bring %s into %s.", VAP_VETH_HOST, VAP_BRIDGE);
    goto fail;
  }
  if (in_netns(fd, port_up, &p) < 0) {
    vap_error("Could not bring up %s inside container '%s'.", VAP_PORT,
              o->container);
    goto fail;
  }
  snprintf(mac, 18, "%s", p.mac);
  /* One CPU has nothing to spread over. Best effort, but half of it brings
   * the reordering back, so that says so. */
  char mask[RPS_MASK_MAX];
  rps_mask(mask, sizeof(mask));
  bool host = *mask && rps_host(VAP_VETH_HOST, mask);
  bool port = *mask && rps_in_netns(fd, VAP_PORT, mask);
  if (host && port)
    vap_log("RPS on %s and %s (CPU mask %s).", VAP_VETH_HOST, VAP_PORT, mask);
  else if (host || port)
    vap_warn("RPS took on %s only.", host ? VAP_VETH_HOST : VAP_PORT);
  struct stat st;
  plugged_pid = pid;
  plugged_ns = fstat(fd, &st) == 0 ? st.st_ino : 0;
  close(fd);
  return 0;
fail:
  /* Half a port is no port. A start then fails; a replug leaves the host end
   * gone, so the monitor sees the container still unplugged and tries again. */
  link_del(nl, VAP_VETH_HOST);
  close(fd);
  return -1;
}

bool managed_plugged(void) {
  /* Usually the old namespace goes and takes the whole veth along, but one
   * something still holds a reference to outlives its last process, and the
   * host end stays, peered into a namespace nobody uses. Its inode cannot be
   * reused while it is held, and once it is freed the host end is gone. */
  return if_nametoindex(VAP_VETH_HOST) && plugged_ns &&
         netns_ino(plugged_pid) == plugged_ns;
}

int managed_replug(const struct vap_opts *o, pid_t pid) {
  struct nl nl;
  char mac[18];
  if (nl_open(&nl) < 0)
    return -1;
  int ret = plug(o, pid, &nl, mac);
  nl_close(&nl);
  if (ret < 0)
    return ret;
  vap_log("Container '%s' restarted - its LAN port is back.", o->container);
  /* Without -m the new port has a new random MAC; status shows the real one. */
  struct state st;
  if (state_load(VAP_STATE, &st)) {
    state_set(&st, "gw_mac", mac);
    state_save(VAP_STATE, &st);
  }
  return 0;
}

static int wire(const struct vap_opts *o, pid_t pid, const struct subnet *net,
                const struct plan *p) {
  vap_log("Managed mode: wiring %s to container '%s' (PID %d) via %s.",
          AP_IFACE, o->container, (int)pid, VAP_BRIDGE);
  if (ap_iface_create(o->phy_iface) < 0)
    return -1;
  struct nl nl;
  if (nl_open(&nl) < 0)
    return -1;
  /* A stale bridge from a previous run; plug() clears a stale veth. */
  link_del(&nl, VAP_BRIDGE);
  int ret = -1;
  char mac[18] = "";
  if (link_add_bridge(&nl, VAP_BRIDGE) < 0) {
    vap_error("Failed to create %s", VAP_BRIDGE);
    goto out;
  }
  /* No IP, IPv6 included: Android leaves accept_ra at 2, so the host would
   * take an address and a default route from the container's own RAs. */
  conf6_set(VAP_BRIDGE, "disable_ipv6", "1");
  if (retry(up, &nl, VAP_BRIDGE, NULL) < 0) {
    vap_error("Failed to bring up %s", VAP_BRIDGE);
    goto out;
  }
  if (plug(o, pid, &nl, mac) < 0)
    goto out;
  /* The container owns L3: OpenWrt is provisioned, anything else gets the
   * port and does the rest itself. */
  if (ds_is_openwrt(o->container)) {
    openwrt_provision(o->container, net);
  } else {
    vap_log("Container is not OpenWrt - delivered %s. The container must",
            VAP_PORT);
    vap_log("configure it (assign an IP, run DHCP + NAT) on its own.");
  }
  struct state st = {0};
  state_set(&st, "mode", "bridged");
  state_set(&st, "container", o->container);
  state_set(&st, "bridge", VAP_BRIDGE);
  /* Read back rather than assumed, so status shows a random one too. */
  state_set(&st, "gw_mac", mac);
  if (ap_state_save(&st, o, p, net) < 0)
    goto out;
  vap_log("Managed network wiring completed.");
  ret = 0;
out:
  nl_close(&nl);
  return ret;
}

int managed_start(const struct vap_opts *o, const struct subnet *lan) {
  struct subnet net = *lan;
  struct plan p;
  pid_t pid = ds_container_pid(o->container);
  if (!pid) {
    vap_error("Container '%s' is not running - start it first.", o->container);
    return -1;
  }
  ap_plan_radio(o, &p);
  ap_hold_signals();
  int ok = wire(o, pid, &net, &p) == 0 && !ap_interrupted();
  if (ok) {
    vap_log("Starting hostapd (managed/bridged mode)...");
    /* ap0 joins the bridge only once hostapd has it up. */
    struct nl nl = {-1, 0};
    ok = hostapd_start(o, &p) == 0;
    if (ok && (nl_open(&nl) < 0 ||
               retry(link_set_master, &nl, AP_IFACE, VAP_BRIDGE) < 0)) {
      vap_error("Failed to enslave %s to %s.", AP_IFACE, VAP_BRIDGE);
      ok = 0;
    }
    nl_close(&nl);
  }
  if (!ok || ap_interrupted()) {
    managed_teardown(o->container);
    return -1;
  }
  vap_log("%s bridged into %s - clients now live on the container's LAN.",
          AP_IFACE, VAP_BRIDGE);
  vap_log("VirtualAP is ACTIVE (managed by container '%s')!", o->container);
  vap_log("  SSID    : %s (%dGHz, channel %d, %dMHz)", o->ssid, p.band,
          p.channel, p.width);
  vap_log("  Upstream: container '%s' via %s -> %s", o->container, VAP_BRIDGE,
          VAP_PORT);
  vap_log("  LAN     : served by the container (%s)", net.gw);
  /* The container NATs the clients, so their traffic reaches the host from
   * Droidspaces' own NAT subnets. */
  ap_ttl(o, DS_NAT_NET, DS_NAT_NET6);
  /* A stop that came during the last steps is still a stop. */
  if (ap_interrupted()) {
    managed_teardown(o->container);
    return -1;
  }
  return 0;
}

void managed_teardown(const char *container) {
  vap_log("Tearing down managed (bridged) AP...");
  ap_daemons_stop();
  ttl_clear();
  ap_iface_del();
  struct nl nl;
  if (nl_open(&nl) == 0) {
    link_del(&nl, VAP_VETH_HOST);
    link_del(&nl, VAP_BRIDGE);
    nl_close(&nl);
  }
  unlink(VAP_STATE);
  vap_log("Managed AP stopped (container '%s' left running).", container);
}
