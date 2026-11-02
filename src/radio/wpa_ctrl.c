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
