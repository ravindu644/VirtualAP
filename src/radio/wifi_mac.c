/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/fsutil.h"
#include "net/link.h"
#include "radio/wifi_mac.h"

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
