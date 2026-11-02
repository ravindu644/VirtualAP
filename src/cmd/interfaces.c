/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "cmd/cmd.h"
#include "core/log.h"
#include "virtualap.h"

#define MAX_IFACES 128

struct iface {
  char name[IF_NAMESIZE];
  char ip[INET_ADDRSTRLEN];
};

static int by_name(const void *a, const void *b) {
  return strcmp(((const struct iface *)a)->name,
                ((const struct iface *)b)->name);
}

/* Candidate upstreams for the app, "name:ip" or "name:no-ip" per line, in
 * name order. getifaddrs lists every link (AF_PACKET) and then the
 * addresses, primary IPv4 address first. */
int cmd_interfaces(struct vap_opts *o) {
  (void)o;
  struct ifaddrs *all;
  if (getifaddrs(&all) < 0) {
    vap_error("Cannot list the network interfaces.");
    return 1;
  }
  static struct iface ifs[MAX_IFACES];
  int n = 0;
  for (struct ifaddrs *a = all; a; a = a->ifa_next) {
    if (!strcmp(a->ifa_name, "lo") || !strcmp(a->ifa_name, AP_IFACE))
      continue;
    int i;
    for (i = 0; i < n && strcmp(ifs[i].name, a->ifa_name); i++)
      ;
    if (i == n) {
      if (n == MAX_IFACES)
        continue;
      snprintf(ifs[n].name, sizeof(ifs[n].name), "%s", a->ifa_name);
      ifs[n++].ip[0] = '\0';
    }
    if (!ifs[i].ip[0] && a->ifa_addr && a->ifa_addr->sa_family == AF_INET)
      inet_ntop(AF_INET, &((struct sockaddr_in *)(void *)a->ifa_addr)->sin_addr,
                ifs[i].ip, sizeof(ifs[i].ip));
  }
  freeifaddrs(all);
  qsort(ifs, (size_t)n, sizeof(ifs[0]), by_name);
  for (int i = 0; i < n; i++)
    printf("%s:%s\n", ifs[i].name, ifs[i].ip[0] ? ifs[i].ip : "no-ip");
  return 0;
}
