/* After USB-Arsenal's net/netns.c (in_netns, ds_open_container_netns).
 * Copyright (C) 2026 ravindu644 <droidcasts@protonmail.com>
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "net/netns.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/log.h"

int netns_open(pid_t pid) {
  char p[64];
  snprintf(p, sizeof(p), "/proc/%d/ns/net", (int)pid);
  int fd = open(p, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -errno;
  struct stat theirs, ours;
  if (fstat(fd, &theirs) < 0 || stat("/proc/self/ns/net", &ours) < 0 ||
      theirs.st_ino == ours.st_ino) {
    close(fd);
    return -EINVAL;
  }
  return fd;
}

ino_t netns_ino(pid_t pid) {
  char p[64];
  struct stat st;
  snprintf(p, sizeof(p), "/proc/%d/ns/net", (int)pid);
  return stat(p, &st) == 0 ? st.st_ino : 0;
}

int in_netns(int fd, int (*fn)(void *), void *arg) {
  int self = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
  if (self < 0)
    return -errno;
  if (setns(fd, CLONE_NEWNET) < 0) {
    int e = -errno;
    close(self);
    return e;
  }
  int ret = fn(arg);
  if (setns(self, CLONE_NEWNET) < 0) {
    vap_error("Could not return to the host's network namespace.");
    _exit(1);
  }
  close(self);
  return ret;
}
