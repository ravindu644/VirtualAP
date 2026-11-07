/* What externals/iw does for `dev info`, `dev link`, `phy info` and
 * `interface add/del`, as the attributes it reads (info.c, link.c,
 * interface.c), without libnl.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <ctype.h>
#include <errno.h>
#include <linux/nl80211.h>
#include <net/if.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "core/core.h"
#include "internal.h"
#include "net/net.h"
#include "radio/radio.h"

/* Start a request: nlmsghdr + genlmsghdr for cmd. */
static void genl_req(struct nl_req *r, int family_id, int cmd, int flags) {
  struct genlmsghdr *g = nl_req_init(r, family_id, flags, GENL_HDRLEN);
  g->cmd = (uint8_t)cmd;
  g->version = 1;
}

/* The attributes of a generic netlink reply. */
static void genl_parse(struct rtattr *tb[], int max, const struct nlmsghdr *h) {
  nl_parse(tb, max, (struct rtattr *)((char *)NLMSG_DATA(h) + GENL_HDRLEN),
           (int)(h->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN)));
}

static void id_cb(const struct nlmsghdr *h, void *arg) {
  struct rtattr *tb[CTRL_ATTR_MAX + 1];
  genl_parse(tb, CTRL_ATTR_MAX, h);
  if (tb[CTRL_ATTR_FAMILY_ID] && RTA_PAYLOAD(tb[CTRL_ATTR_FAMILY_ID]) >= 2) {
    uint16_t id;
    memcpy(&id, RTA_DATA(tb[CTRL_ATTR_FAMILY_ID]), sizeof(id));
    *(int *)arg = id;
  }
}

/* A NETLINK_GENERIC socket with the id of family `name` resolved. 0, or
 * -errno (-ENOENT when the kernel has no such family). */
static int genl_open(struct nl *nl, const char *name, int *family_id) {
  int ret = nl_open_proto(nl, NETLINK_GENERIC);
  if (ret < 0)
    return ret;
  struct nl_req r;
  genl_req(&r, GENL_ID_CTRL, CTRL_CMD_GETFAMILY, 0);
  nl_attr_str(&r, CTRL_ATTR_FAMILY_NAME, name);
  int id = 0;
  ret = nl_get(nl, &r, id_cb, &id);
  if (ret == 0 && !id)
    ret = -ENOENT;
  if (ret < 0) {
    nl_close(nl);
    return ret;
  }
  *family_id = id;
  return 0;
}

/* Old kernel headers name the flag after what it was. */
#ifndef NL80211_FREQUENCY_ATTR_NO_IR
#define NL80211_FREQUENCY_ATTR_NO_IR NL80211_FREQUENCY_ATTR_PASSIVE_SCAN
#endif

struct session {
  struct nl nl;
  int id;
};

static int open_nl80211(struct session *s) {
  return genl_open(&s->nl, "nl80211", &s->id);
}

/* A request about ifname; -ENODEV when there is no such interface. */
static int req_for(struct session *s, struct nl_req *r, int cmd,
                   const char *ifname) {
  unsigned idx = if_nametoindex(ifname);
  if (!idx)
    return -ENODEV;
  genl_req(r, s->id, cmd, 0);
  nl_attr_u32(r, NL80211_ATTR_IFINDEX, idx);
  return 0;
}

static void add_freqs(struct phy_info *p, struct rtattr *freqs) {
  int rem = (int)RTA_PAYLOAD(freqs);
  for (struct rtattr *f = RTA_DATA(freqs); RTA_OK(f, rem);
       f = RTA_NEXT(f, rem)) {
    struct rtattr *tb[NL80211_FREQUENCY_ATTR_MAX + 1];
    nl_parse(tb, NL80211_FREQUENCY_ATTR_MAX, RTA_DATA(f), (int)RTA_PAYLOAD(f));
    if (!tb[NL80211_FREQUENCY_ATTR_FREQ] || p->nfreqs >= PHY_MAX_FREQS)
      continue;
    p->freqs[p->nfreqs].freq = (int)nl_u32(tb[NL80211_FREQUENCY_ATTR_FREQ]);
    p->freqs[p->nfreqs].disabled = tb[NL80211_FREQUENCY_ATTR_DISABLED] != NULL;
    /* "no IR" in iw on new kernels, "passive scan" on old ones: either way
     * the radio may not beacon there. */
    p->freqs[p->nfreqs].no_ir = tb[NL80211_FREQUENCY_ATTR_NO_IR] != NULL;
    p->nfreqs++;
  }
}

static void wiphy_cb(const struct nlmsghdr *h, void *arg) {
  struct phy_info *p = arg;
  struct rtattr *tb[NL80211_ATTR_MAX + 1];
  genl_parse(tb, NL80211_ATTR_MAX, h);
  if (!tb[NL80211_ATTR_WIPHY_BANDS])
    return;
  int rem = (int)RTA_PAYLOAD(tb[NL80211_ATTR_WIPHY_BANDS]);
  for (struct rtattr *b = RTA_DATA(tb[NL80211_ATTR_WIPHY_BANDS]);
       RTA_OK(b, rem); b = RTA_NEXT(b, rem)) {
    /* 0 is 2.4 GHz and 1 is 5 GHz on every kernel. */
    int idx = b->rta_type == 0 ? BAND_24 : b->rta_type == 1 ? BAND_5 : -1;
    struct rtattr *bt[NL80211_BAND_ATTR_MAX + 1];
    nl_parse(bt, NL80211_BAND_ATTR_MAX, RTA_DATA(b), (int)RTA_PAYLOAD(b));
    if (bt[NL80211_BAND_ATTR_FREQS])
      add_freqs(p, bt[NL80211_BAND_ATTR_FREQS]);
    if (idx < 0)
      continue;
    p->band[idx].present = true;
    /* iw: "HT20/HT40" is bit 1 of the HT capabilities; "VHT Capabilities"
     * needs both the capabilities and the MCS set. */
    if (bt[NL80211_BAND_ATTR_HT_CAPA] &&
        RTA_PAYLOAD(bt[NL80211_BAND_ATTR_HT_CAPA]) >= 2) {
      uint16_t capa;
      memcpy(&capa, RTA_DATA(bt[NL80211_BAND_ATTR_HT_CAPA]), sizeof(capa));
      if (capa & 0x2)
        p->band[idx].ht40 = true;
    }
    if (bt[NL80211_BAND_ATTR_VHT_CAPA] && bt[NL80211_BAND_ATTR_VHT_MCS_SET])
      p->band[idx].vht = true;
  }
}

int nl80211_phy_info(const char *ifname, struct phy_info *out) {
  struct session s;
  memset(out, 0, sizeof(*out));
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  struct nl_req r;
  ret = req_for(&s, &r, NL80211_CMD_GET_WIPHY, ifname);
  if (ret == 0) {
    nl_attr(&r, NL80211_ATTR_SPLIT_WIPHY_DUMP, NULL, 0);
    ret = nl_dump(&s.nl, &r, wiphy_cb, out);
  }
  nl_close(&s.nl);
  return ret;
}

/* A request about a wiphy by index, with no netdev involved. */
static void req_for_phy(struct session *s, struct nl_req *r, int cmd,
                        int wiphy) {
  genl_req(r, s->id, cmd, 0);
  nl_attr_u32(r, NL80211_ATTR_WIPHY, (uint32_t)wiphy);
}

int nl80211_phy_info_idx(int wiphy, struct phy_info *out) {
  struct session s;
  memset(out, 0, sizeof(*out));
  if (wiphy < 0)
    return -ENODEV;
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  struct nl_req r;
  req_for_phy(&s, &r, NL80211_CMD_GET_WIPHY, wiphy);
  nl_attr(&r, NL80211_ATTR_SPLIT_WIPHY_DUMP, NULL, 0);
  ret = nl_dump(&s.nl, &r, wiphy_cb, out);
  nl_close(&s.nl);
  return ret;
}

/* The wiphy behind an interface, from GET_INTERFACE. */
static void iface_wiphy_cb(const struct nlmsghdr *h, void *arg) {
  struct rtattr *tb[NL80211_ATTR_MAX + 1];
  genl_parse(tb, NL80211_ATTR_MAX, h);
  if (tb[NL80211_ATTR_WIPHY])
    *(int *)arg = (int)nl_u32(tb[NL80211_ATTR_WIPHY]);
}

/* What a dump of every wiphy teaches: the first one seen, and the first one
 * that can run an AP. A split dump sends the interface types in the first
 * message of each wiphy, so that is where they are looked for. */
struct wiphy_pick {
  int first, ap;
};

static void wiphy_pick_cb(const struct nlmsghdr *h, void *arg) {
  struct wiphy_pick *w = arg;
  struct rtattr *tb[NL80211_ATTR_MAX + 1];
  genl_parse(tb, NL80211_ATTR_MAX, h);
  if (!tb[NL80211_ATTR_WIPHY])
    return;
  int idx = (int)nl_u32(tb[NL80211_ATTR_WIPHY]);
  if (w->first < 0)
    w->first = idx;
  if (w->ap >= 0 || !tb[NL80211_ATTR_SUPPORTED_IFTYPES])
    return;
  int rem = (int)RTA_PAYLOAD(tb[NL80211_ATTR_SUPPORTED_IFTYPES]);
  for (struct rtattr *t = RTA_DATA(tb[NL80211_ATTR_SUPPORTED_IFTYPES]);
       RTA_OK(t, rem); t = RTA_NEXT(t, rem))
    if (t->rta_type == NL80211_IFTYPE_AP)
      w->ap = idx;
}

int nl80211_find_wiphy(const char *prefer_ifname) {
  struct session s;
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  int idx = -1;
  struct nl_req r;
  if (prefer_ifname && *prefer_ifname &&
      req_for(&s, &r, NL80211_CMD_GET_INTERFACE, prefer_ifname) == 0)
    nl_get(&s.nl, &r, iface_wiphy_cb, &idx);
  if (idx < 0) {
    struct wiphy_pick w = {-1, -1};
    genl_req(&r, s.id, NL80211_CMD_GET_WIPHY, 0);
    nl_attr(&r, NL80211_ATTR_SPLIT_WIPHY_DUMP, NULL, 0);
    ret = nl_dump(&s.nl, &r, wiphy_pick_cb, &w);
    /* A wiphy with no interface types listed is still better than none. */
    idx = w.ap >= 0 ? w.ap : w.first;
    if (idx < 0)
      ret = ret < 0 ? ret : -ENODEV;
  }
  nl_close(&s.nl);
  return idx >= 0 ? idx : ret;
}

static void scan_cb(const struct nlmsghdr *h, void *arg) {
  int *freq = arg;
  struct rtattr *tb[NL80211_ATTR_MAX + 1];
  genl_parse(tb, NL80211_ATTR_MAX, h);
  if (!tb[NL80211_ATTR_BSS])
    return;
  struct rtattr *bss[NL80211_BSS_MAX + 1];
  nl_parse(bss, NL80211_BSS_MAX, RTA_DATA(tb[NL80211_ATTR_BSS]),
           (int)RTA_PAYLOAD(tb[NL80211_ATTR_BSS]));
  if (!bss[NL80211_BSS_STATUS] || !bss[NL80211_BSS_FREQUENCY])
    return;
  uint32_t st = nl_u32(bss[NL80211_BSS_STATUS]);
  if (st == NL80211_BSS_STATUS_ASSOCIATED ||
      st == NL80211_BSS_STATUS_IBSS_JOINED)
    *freq = (int)nl_u32(bss[NL80211_BSS_FREQUENCY]);
}

int nl80211_sta_freq(const char *ifname) {
  struct session s;
  int ret = open_nl80211(&s), freq = 0;
  if (ret < 0)
    return ret;
  /* Android scans in the background, and a scan dump that spans a new scan
   * comes back interrupted; the associated BSS is still the one found. */
  for (int attempt = 0; attempt < 3 && !freq; attempt++) {
    struct nl_req r;
    ret = req_for(&s, &r, NL80211_CMD_GET_SCAN, ifname);
    if (ret == 0)
      ret = nl_dump(&s.nl, &r, scan_cb, &freq);
    if (ret != -EINTR)
      break;
  }
  nl_close(&s.nl);
  return freq ? freq : ret < 0 ? ret : 0;
}

static void width_cb(const struct nlmsghdr *h, void *arg) {
  struct rtattr *tb[NL80211_ATTR_MAX + 1];
  genl_parse(tb, NL80211_ATTR_MAX, h);
  if (!tb[NL80211_ATTR_CHANNEL_WIDTH])
    return;
  /* In MHz, what the app shows. Widths with no entry here (80+80, 5 MHz,
   * the S1G ones) read as 0 and leave status to the conf. */
  static const int mhz[] = {[NL80211_CHAN_WIDTH_20_NOHT] = 20,
                            [NL80211_CHAN_WIDTH_20] = 20,
                            [NL80211_CHAN_WIDTH_40] = 40,
                            [NL80211_CHAN_WIDTH_80] = 80,
                            [NL80211_CHAN_WIDTH_160] = 160,
                            [NL80211_CHAN_WIDTH_10] = 10,
                            [13] = 320};
  uint32_t w = nl_u32(tb[NL80211_ATTR_CHANNEL_WIDTH]);
  if (w < sizeof(mhz) / sizeof(mhz[0]))
    *(int *)arg = mhz[w];
}

int nl80211_width(const char *ifname) {
  struct session s;
  int width = 0;
  if (open_nl80211(&s) < 0)
    return 0;
  struct nl_req r;
  if (req_for(&s, &r, NL80211_CMD_GET_INTERFACE, ifname) == 0)
    nl_get(&s.nl, &r, width_cb, &width);
  nl_close(&s.nl);
  return width;
}

int nl80211_add_ap(const char *phy_iface, const char *name) {
  struct session s;
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  struct nl_req r;
  ret = req_for(&s, &r, NL80211_CMD_NEW_INTERFACE, phy_iface);
  if (ret == 0) {
    nl_attr_str(&r, NL80211_ATTR_IFNAME, name);
    nl_attr_u32(&r, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_AP);
    ret = nl_talk(&s.nl, &r);
  }
  nl_close(&s.nl);
  return ret;
}

int nl80211_add_ap_phy(int wiphy, const char *name) {
  struct session s;
  if (wiphy < 0)
    return -ENODEV;
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  struct nl_req r;
  req_for_phy(&s, &r, NL80211_CMD_NEW_INTERFACE, wiphy);
  nl_attr_str(&r, NL80211_ATTR_IFNAME, name);
  nl_attr_u32(&r, NL80211_ATTR_IFTYPE, NL80211_IFTYPE_AP);
  ret = nl_talk(&s.nl, &r);
  nl_close(&s.nl);
  return ret;
}

int nl80211_del(const char *name) {
  struct session s;
  int ret = open_nl80211(&s);
  if (ret < 0)
    return ret;
  struct nl_req r;
  ret = req_for(&s, &r, NL80211_CMD_DEL_INTERFACE, name);
  if (ret == 0)
    ret = nl_talk(&s.nl, &r);
  nl_close(&s.nl);
  return ret;
}

/* The client half of hostapd's src/common/wpa_ctrl.c. hostapd_cli binds its
 * reply socket under /tmp, which Android does not have, hence the old -s;
 * an abstract autobind needs no directory at all and leaves nothing behind. */
/* A datagram socket bound to an address the kernel picks, so the daemon has
 * somewhere to send its reply, and connected to path. -errno on failure. */
static int open_ctrl(const char *path) {
  struct sockaddr_un to = {.sun_family = AF_UNIX};
  if (snprintf(to.sun_path, sizeof(to.sun_path), "%s", path) >=
      (int)sizeof(to.sun_path))
    return -ENAMETOOLONG;
  int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -errno;
  struct sockaddr_un me = {.sun_family = AF_UNIX};
  if (bind(fd, (struct sockaddr *)&me, sizeof(sa_family_t)) < 0 ||
      connect(fd, (struct sockaddr *)&to, sizeof(to)) < 0) {
    int e = -errno;
    close(fd);
    return e;
  }
  return fd;
}

int wpa_ctrl_send(const char *path, const char *cmd) {
  int fd = open_ctrl(path);
  if (fd < 0)
    return fd;
  /* Never block on a daemon whose queue is full: nothing waits for this. */
  int ret = send(fd, cmd, strlen(cmd), MSG_DONTWAIT) < 0 ? -errno : 0;
  close(fd);
  return ret;
}

int wpa_ctrl_request(const char *path, const char *cmd, char *reply,
                     size_t size, int timeout_ms) {
  if (!size)
    return -EINVAL;
  int fd = open_ctrl(path);
  if (fd < 0)
    return fd;
  /* Never ATTACHed, so the daemon sends nothing unsolicited: the first
   * datagram back is the reply. MSG_DONTWAIT because a hostapd that stopped
   * reading lets its queue fill, and a blocking send would then wait for
   * good. An interrupted poll counts as no answer: nothing here installs a
   * signal handler. */
  struct pollfd p = {fd, POLLIN, 0};
  int ret = -ETIMEDOUT;
  if (send(fd, cmd, strlen(cmd), MSG_DONTWAIT) < 0) {
    ret = -errno;
  } else if (poll(&p, 1, timeout_ms) > 0) {
    /* MSG_TRUNC: the datagram's real length, so a reply cut to fit is an
     * error rather than a shorter answer. */
    ssize_t n = recv(fd, reply, size - 1, MSG_TRUNC);
    if (n < 0)
      ret = -errno;
    else if ((size_t)n > size - 1)
      ret = -EMSGSIZE;
    else
      reply[ret = (int)n] = '\0';
  }
  close(fd);
  return ret;
}

bool mac_take(const char *s, char out[18]) {
  /* Samsung's efs file on MediaTek phones holds twelve bare hex digits,
   * the colon form only on Broadcom ones; both come out as aa:bb:.. */
  bool bare = true;
  for (int i = 0; i < 12; i++)
    bare = bare && isxdigit((unsigned char)s[i]);
  if (bare && !isxdigit((unsigned char)s[12])) {
    for (int i = 0, o = 0; i < 12; i++) {
      out[o++] = (char)tolower((unsigned char)s[i]);
      if (i % 2 && i < 11)
        out[o++] = ':';
    }
  } else {
    for (int i = 0; i < 17; i++) {
      bool colon = i % 3 == 2;
      if (colon ? s[i] != ':' : !isxdigit((unsigned char)s[i]))
        return false;
      out[i] = (char)tolower((unsigned char)s[i]);
    }
  }
  out[17] = '\0';
  /* A HAL that could not read it answers with zeros. */
  return strcmp(out, "00:00:00:00:00:00") != 0;
}

/* <string name="wifi_sta_factory_mac_address">aa:..</string>, the colon form
 * only. The buffer is the file's size, tens of KB. */
static bool from_store(const char *path, char out[18]) {
  static const char key[] = "\"wifi_sta_factory_mac_address\">";
  struct stat st;
  if (stat(path, &st) < 0 || st.st_size <= 0 || st.st_size > (64 << 20))
    return false;
  size_t size = (size_t)st.st_size + 1;
  char *buf = malloc(size);
  bool ok = false;
  if (buf && read_file(path, buf, size) > 0) {
    const char *k = strstr(buf, key);
    const char *v = k ? k + sizeof(key) - 1 : NULL;
    ok = v && strnlen(v, 18) == 18 && v[17] == '<' && v[2] == ':' &&
         mac_take(v, out);
  }
  free(buf);
  return ok;
}

void wifi_mac(const char *ifname, char out[18]) {
  char path[96], buf[64];
  if (from_store("/data/misc/apexdata/com.android.wifi/WifiConfigStore.xml",
                 out) ||
      from_store("/data/misc/wifi/WifiConfigStore.xml", out))
    return;
  snprintf(path, sizeof(path), "/sys/class/net/%s/addr_assign_type", ifname);
  if (read_file(path, buf, sizeof(buf)) > 0 && !strcmp(buf, "0")) {
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", ifname);
    if (read_file(path, buf, sizeof(buf)) >= 17 && mac_take(buf, out))
      return;
  }
  struct nl nl;
  if (nl_open(&nl) == 0) {
    int ret = link_permaddr(&nl, ifname, buf);
    nl_close(&nl);
    if (ret == 0 && mac_take(buf, out))
      return;
  }
  static const char *const efs[] = {"/efs/wifi/.mac.info",
                                    "/mnt/vendor/efs/wifi/.mac.info"};
  for (int i = 0; i < 2; i++)
    if (read_file(efs[i], buf, sizeof(buf)) >= 12 && mac_take(buf, out))
      return;
  out[0] = '\0';
}
