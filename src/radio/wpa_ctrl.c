/* The client half of hostapd's src/common/wpa_ctrl.c. hostapd_cli binds its
 * reply socket under /tmp, which Android does not have, hence the old -s;
 * an abstract autobind needs no directory at all and leaves nothing behind.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "radio/wpa_ctrl.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

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
  int ret = -ETIMEDOUT;
  if (send(fd, cmd, strlen(cmd), 0) < 0) {
    ret = -errno;
  } else {
    struct pollfd p = {fd, POLLIN, 0};
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long long deadline = now.tv_sec * 1000LL + now.tv_nsec / 1000000 + timeout_ms;
    /* Unsolicited events start with '<'; only an attached client gets them,
     * but skip them rather than take one for the reply, within the one
     * deadline, so a chatty daemon that never answers cannot hold us. */
    for (;;) {
      clock_gettime(CLOCK_MONOTONIC, &now);
      long long left =
          deadline - (now.tv_sec * 1000LL + now.tv_nsec / 1000000);
      if (left <= 0)
        break;
      int pr = poll(&p, 1, (int)left);
      if (pr < 0 && errno == EINTR)
        continue;
      if (pr <= 0)
        break;
      ssize_t n = recv(fd, reply, size - 1, 0);
      if (n < 0) {
        ret = -errno;
        break;
      }
      reply[n] = '\0';
      if (n > 0 && reply[0] == '<')
        continue;
      ret = (int)n;
      break;
    }
  }
  close(fd);
  return ret;
}
