/* The mask is Droidspaces' ds_net_steer_rx, which has no 30-CPU limit
 * (start-ap's sh arithmetic was 32 bit).
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/rps.h"

#include <ctype.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/fsutil.h"
#include "core/log.h"
#include "virtualap.h"

/* "fe" for CPUs 0-7, 32-bit words most significant first, "" for one CPU. */
void rps_mask(char *mask, size_t size) {
  char buf[64];
  mask[0] = '\0';
  if (read_file("/sys/devices/system/cpu/present", buf, sizeof(buf)) < 0)
    return;
  /* "0-7" or "0,2-5": the highest CPU id is the last number. */
  char *p = buf + strlen(buf);
  while (p > buf && !isdigit((unsigned char)p[-1]))
    p--;
  while (p > buf && isdigit((unsigned char)p[-1]))
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
    off += (size_t)snprintf(mask + off, size - off,
                            w == max / 32 ? "%x" : ",%08x", bits);
  }
}

static bool write_mask(const char *sys, const char *ifname, const char *mask) {
  char path[160];
  snprintf(path, sizeof(path), "%s/class/net/%s/queues/rx-0/rps_cpus", sys,
           ifname);
  return write_file(path, mask) == 0;
}

bool rps_host(const char *ifname) {
  char mask[300];
  rps_mask(mask, sizeof(mask));
  return *mask && write_mask("/sys", ifname, mask);
}

bool rps_in_netns(int netns_fd, const char *ifname) {
  char mask[300];
  rps_mask(mask, sizeof(mask));
  if (!*mask)
    return false;
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
