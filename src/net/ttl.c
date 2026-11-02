/* start-ap's ttl_supported, ttl_clear and ttl_apply. A custom chain in the
 * raw table format is a lot of code for one optional rule, so these stay on
 * the iptables binary.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/ttl.h"

#include <stddef.h>

#include <sys/socket.h>

#include "net/ipt_rules.h"

#define V4 AF_INET
#define V6 AF_INET6

#define MANGLE(fam, ...)                                                       \
  ipt_bin(fam, "mangle", (const char *const[]){__VA_ARGS__, NULL})

bool ttl_supported(void) {
  MANGLE(V4, "-N", "vap_ttl_probe");
  bool ok =
      MANGLE(V4, "-A", "vap_ttl_probe", "-j", "TTL", "--ttl-set", "64") == 0;
  MANGLE(V4, "-F", "vap_ttl_probe");
  MANGLE(V4, "-X", "vap_ttl_probe");
  return ok;
}

void ttl_clear(void) {
  static const int fams[] = {V4, V6};
  for (int i = 0; i < 2; i++) {
    /* Bounded: a delete that keeps succeeding must not spin a stop forever. */
    for (int n = 0; n < 16; n++)
      if (MANGLE(fams[i], "-D", "FORWARD", "-j", "vap_ttl") != 0)
        break;
    MANGLE(fams[i], "-F", "vap_ttl");
    MANGLE(fams[i], "-X", "vap_ttl");
  }
}

/* FORWARD only sees forwarded packets, after the kernel took its one off the
 * TTL, so the phone's own traffic is left alone and 64 is what leaves.
 * Matching the source keeps it to outbound packets: replies on their way to
 * the clients keep the TTL they arrived with. */
int ttl_apply(const char *src4, const char *src6) {
  ttl_clear();
  MANGLE(V4, "-N", "vap_ttl");
  if (MANGLE(V4, "-A", "vap_ttl", "-s", src4, "-j", "TTL", "--ttl-set", "64") !=
          0 ||
      MANGLE(V4, "-I", "FORWARD", "-j", "vap_ttl") != 0) {
    ttl_clear();
    return -1;
  }
  /* IPv6 calls the same field hop limit; best effort, as in start-ap. */
  if (src6 && *src6) {
    MANGLE(V6, "-N", "vap_ttl");
    if (MANGLE(V6, "-A", "vap_ttl", "-s", src6, "-j", "HL", "--hl-set", "64") ==
        0)
      MANGLE(V6, "-I", "FORWARD", "-j", "vap_ttl");
  }
  return 0;
}
