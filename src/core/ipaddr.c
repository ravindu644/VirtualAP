/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/ipaddr.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool valid_ip4(const char *s) {
  struct in_addr a;
  return inet_pton(AF_INET, s, &a) == 1;
}

bool prefix_parse(int family, const char *s, unsigned char bin[16], int *len) {
  char buf[INET6_ADDRSTRLEN + 8];
  if (snprintf(buf, sizeof(buf), "%s", s) >= (int)sizeof(buf))
    return false;
  int max = family == AF_INET6 ? 128 : 32;
  *len = max;
  char *slash = strchr(buf, '/');
  if (slash) {
    *slash = '\0';
    char *end;
    long l = strtol(slash + 1, &end, 10);
    if (end == slash + 1 || *end || l < 0 || l > max)
      return false;
    *len = (int)l;
  }
  return inet_pton(family, buf, bin) == 1;
}

bool subnet_from_gw(const char *gw, struct subnet *out) {
  struct in_addr a;
  if (inet_pton(AF_INET, gw, &a) != 1)
    return false;
  const unsigned char *o = (const unsigned char *)&a.s_addr;
  if (o[3] == 0 || o[3] == 255)
    return false;
  snprintf(out->gw, sizeof(out->gw), "%u.%u.%u.%u", o[0], o[1], o[2], o[3]);
  snprintf(out->net, sizeof(out->net), "%u.%u.%u.0/24", o[0], o[1], o[2]);
  snprintf(out->begin, sizeof(out->begin), "%u.%u.%u.10", o[0], o[1], o[2]);
  snprintf(out->end, sizeof(out->end), "%u.%u.%u.50", o[0], o[1], o[2]);
  out->third = o[2];
  return true;
}
