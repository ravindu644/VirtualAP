/* The exe check is USB-Arsenal's ua_read_our_pid.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/pidfile.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "core/fsutil.h"

static pid_t pidfile_read(const char *path) {
  char buf[32], *end;
  if (read_file(path, buf, sizeof(buf)) <= 0)
    return 0;
  long pid = strtol(buf, &end, 10);
  return pid <= 0 || *end || pid > INT_MAX ? 0 : (pid_t)pid;
}

pid_t pidfile_ours(const char *path, const char *exe) {
  char p[64], target[PATH_MAX];
  long pid = pidfile_read(path);
  if (!pid)
    return 0;
  snprintf(p, sizeof(p), "/proc/%ld/exe", pid);
  ssize_t n = readlink(p, target, sizeof(target) - 1);
  if (n <= 0)
    return 0;
  target[n] = '\0';
  /* Replacing the binary under a running daemon (an APK update) leaves its
   * exe as "... (deleted)"; it is still ours. */
  char *del = strstr(target, " (deleted)");
  if (del)
    *del = '\0';
  const char *base = strrchr(target, '/');
  return strcmp(base ? base + 1 : target, exe) == 0 ? (pid_t)pid : 0;
}

void pidfile_kill(const char *path, const char *exe) {
  /* Our own child that already exited is a zombie with no exe to check;
   * reaping it is harmless when the pid is somebody else's. */
  pid_t any = pidfile_read(path);
  if (any)
    waitpid(any, NULL, WNOHANG);
  pid_t pid = pidfile_ours(path, exe);
  if (pid > 0 && kill(pid, SIGTERM) == 0) {
    struct timespec tick = {0, 100 * 1000 * 1000};
    for (int i = 0; i < 20; i++) {
      /* Our own child on a rollback path stays a zombie until reaped. */
      waitpid(pid, NULL, WNOHANG);
      if (kill(pid, 0) < 0)
        break;
      nanosleep(&tick, NULL);
    }
  }
  unlink(path);
}

int pidfile_write(const char *path, pid_t pid) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd < 0)
    return -1;
  char buf[24];
  int n = snprintf(buf, sizeof(buf), "%d\n", (int)pid);
  ssize_t w = write(fd, buf, (size_t)n);
  return close(fd) == 0 && w == n ? 0 : -1;
}
