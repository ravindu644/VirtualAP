/* A custom chain in the raw table format is a lot of code for one optional
 * rule, so the TTL fix stays on the iptables binary.
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

static void clear(int family) {
  /* Bounded: a delete that keeps succeeding must not spin a stop forever. */
  for (int n = 0; n < 16; n++)
    if (MANGLE(family, "-D", "FORWARD", "-j", "vap_ttl") != 0)
      break;
  MANGLE(family, "-F", "vap_ttl");
  MANGLE(family, "-X", "vap_ttl");
}

void ttl_clear(void) {
  clear(AF_INET);
  clear(AF_INET6);
}

/* IPv6 calls the same field hop limit, a target of its own that a kernel
 * with TTL can still lack; IPv4 stays fixed either way, with a word. */
static void apply6(const char *src6) {
  if (!src6 || !*src6)
    return;
  MANGLE(AF_INET6, "-N", "vap_ttl");
  if (MANGLE(AF_INET6, "-A", "vap_ttl", "-s", src6, "-j", "HL", "--hl-set",
             "64") != 0 ||
      MANGLE(AF_INET6, "-I", "FORWARD", "-j", "vap_ttl") != 0) {
    MANGLE(AF_INET6, "-F", "vap_ttl");
    MANGLE(AF_INET6, "-X", "vap_ttl");
    vap_warn("TTL fix: no IPv6 hop limit target, IPv6 leaves unchanged.");
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
  apply6(src6);
  return 0;
}

int ttl_refresh(const char *src4, const char *src6) {
  /* The IPv4 jump gone means netd or a firewall app flushed mangle: all of
   * it goes back. */
  if (MANGLE(AF_INET, "-C", "FORWARD", "-j", "vap_ttl") != 0)
    return ttl_apply(src4, src6);
  clear(AF_INET6);
  apply6(src6);
  return 0;
}
