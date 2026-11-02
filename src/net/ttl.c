/* start-ap's ttl_supported, ttl_clear and ttl_apply. A custom chain in the
 * raw table format is a lot of code for one optional rule, so these stay on
 * the iptables binary.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/ttl.h"

#include <stddef.h>

#include <sys/socket.h>

#include "core/log.h"
#include "net/ipt_rules.h"

#define MANGLE(fam, ...)                                                       \
  ipt_bin(fam, "mangle", (const char *const[]){__VA_ARGS__, NULL})

bool ttl_supported(void) {
  MANGLE(AF_INET, "-N", "vap_ttl_probe");
  bool ok = MANGLE(AF_INET, "-A", "vap_ttl_probe", "-j", "TTL", "--ttl-set",
                   "64") == 0;
  MANGLE(AF_INET, "-F", "vap_ttl_probe");
  MANGLE(AF_INET, "-X", "vap_ttl_probe");
  return ok;
}

void ttl_clear(void) {
  static const int fams[] = {AF_INET, AF_INET6};
  for (size_t i = 0; i < sizeof(fams) / sizeof(fams[0]); i++) {
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
  MANGLE(AF_INET, "-N", "vap_ttl");
  if (MANGLE(AF_INET, "-A", "vap_ttl", "-s", src4, "-j", "TTL", "--ttl-set",
             "64") != 0 ||
      MANGLE(AF_INET, "-I", "FORWARD", "-j", "vap_ttl") != 0) {
    ttl_clear();
    return -1;
  }
  /* IPv6 calls the same field hop limit, a target of its own that a kernel
   * with TTL can still lack; IPv4 stays fixed either way, with a word. */
  if (src6 && *src6) {
    MANGLE(AF_INET6, "-N", "vap_ttl");
    if (MANGLE(AF_INET6, "-A", "vap_ttl", "-s", src6, "-j", "HL", "--hl-set",
               "64") != 0 ||
        MANGLE(AF_INET6, "-I", "FORWARD", "-j", "vap_ttl") != 0) {
      MANGLE(AF_INET6, "-F", "vap_ttl");
      MANGLE(AF_INET6, "-X", "vap_ttl");
      vap_warn("TTL fix: no IPv6 hop limit target, IPv6 leaves unchanged.");
    }
  }
  return 0;
}
