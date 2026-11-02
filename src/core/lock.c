/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/lock.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

#include "core/log.h"
#include "virtualap.h"

static int lock_fd = -1;

int vap_lock(void) {
  /* A restart runs stop from inside start, under the lock it already holds. */
  if (lock_fd >= 0)
    return 0;
  lock_fd = open(VAP_RUN "/lock", O_RDWR | O_CREAT | O_CLOEXEC, 0600);
  if (lock_fd < 0) {
    vap_error("Cannot open %s/lock: %s", VAP_RUN, strerror(errno));
    return -1;
  }
  struct timespec tick = {0, 100 * 1000 * 1000};
  for (int i = 0; i < 300; i++) {
    if (flock(lock_fd, LOCK_EX | LOCK_NB) == 0)
      return 0;
    if (errno != EWOULDBLOCK && errno != EINTR)
      break;
    if (i == 0)
      vap_log("Waiting for another start or stop to finish...");
    nanosleep(&tick, NULL);
  }
  vap_error("Another start or stop is still running.");
  vap_unlock();
  return -1;
}

void vap_unlock(void) {
  if (lock_fd >= 0)
    close(lock_fd);
  lock_fd = -1;
}
