/* Adapted from USB-Arsenal's utils.c.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/core.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "virtualap.h"

static int wait_exit(pid_t pid) {
  int status;
  while (waitpid(pid, &status, 0) < 0)
    if (errno != EINTR)
      return -1;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* stdin from in_fd, stdout to out_fd, stderr to err_fd, each /dev/null when
 * -1. The parent's stdout is flushed first, or the child would
 * inherit and print whatever the buffer held a second time. */
static pid_t start(const char *const argv[], int in_fd, int out_fd,
                   int err_fd) {
  fflush(stdout);
  pid_t pid = fork();
  if (pid != 0)
    return pid;
  /* stdin too: under the app it is libsu's command pipe, and a child reading
   * it would swallow the app's next command. */
  int null = open("/dev/null", O_RDWR | O_CLOEXEC);
  dup2(in_fd >= 0 ? in_fd : null, STDIN_FILENO);
  dup2(out_fd >= 0 ? out_fd : null, STDOUT_FILENO);
  dup2(err_fd >= 0 ? err_fd : null, STDERR_FILENO);
  /* An ignored signal stays ignored across exec, and a blocked one stays
   * blocked: main ignores SIGPIPE and the start holds SIGTERM for itself
   * only. A hostapd that inherited the hold would never stop. */
  signal(SIGPIPE, SIG_DFL);
  sigset_t none;
  sigemptyset(&none);
  sigprocmask(SIG_SETMASK, &none, NULL);
  /* execv's prototype predates const; it never writes through argv. Every
   * program is named by its absolute path: nothing is looked up in PATH. */
  execv(argv[0], (char *const *)(uintptr_t)argv);
  _exit(127);
}

int run_quiet(const char *const argv[]) {
  pid_t pid = start(argv, -1, -1, -1);
  return pid < 0 ? -1 : wait_exit(pid);
}

int run_shown(const char *const argv[]) {
  pid_t pid = start(argv, -1, STDOUT_FILENO, STDERR_FILENO);
  return pid < 0 ? -1 : wait_exit(pid);
}

int run_lines(const char *const argv[], const char *input,
              void (*line)(const char *text, void *arg), void *arg) {
  int in[2], out[2];
  if (pipe2(in, O_CLOEXEC) < 0)
    return -1;
  if (pipe2(out, O_CLOEXEC) < 0) {
    close(in[0]);
    close(in[1]);
    return -1;
  }
  pid_t pid = start(argv, in[0], out[1], out[1]);
  close(in[0]);
  close(out[1]);
  if (pid < 0) {
    close(in[1]);
    close(out[0]);
    return -1;
  }
  /* A script is a few KB, well under a pipe's buffer: written whole before
   * the output is read, without deadlock. */
  size_t len = strlen(input), done = 0;
  while (done < len) {
    ssize_t w = write(in[1], input + done, len - done);
    if (w <= 0)
      break;
    done += (size_t)w;
  }
  close(in[1]);
  FILE *f = fdopen(out[0], "r");
  if (f) {
    char buf[512];
    while (fgets(buf, sizeof(buf), f)) {
      buf[strcspn(buf, "\r\n")] = '\0';
      if (*buf)
        line(buf, arg);
    }
    fclose(f);
  } else {
    close(out[0]);
  }
  return wait_exit(pid);
}

int run_logged(const char *const argv[], const char *log) {
  int fd = open(log, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  pid_t pid = start(argv, -1, fd, fd);
  if (fd >= 0)
    close(fd);
  return pid < 0 ? -1 : wait_exit(pid);
}

static long long now_ms(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* wait_exit that kills pid at end. One stuck in the kernel cannot die yet, and
 * the caller must not stay with it: it is left behind after a last 200 ms. */
static int wait_until(pid_t pid, long long end) {
  bool killed = false;
  for (;;) {
    int status;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == pid)
      return !killed && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (r < 0 && errno != EINTR)
      return -1;
    if (now_ms() >= end) {
      if (killed)
        return -1;
      kill(pid, SIGKILL);
      killed = true;
      end += 200;
    }
    struct timespec tick = {0, 10 * 1000 * 1000};
    nanosleep(&tick, NULL);
  }
}

int run_capture(const char *const argv[], char *out, size_t size) {
  return run_capture_within(argv, out, size, -1);
}

int run_capture_within(const char *const argv[], char *out, size_t size,
                       int ms) {
  long long end = now_ms() + ms;
  int pfd[2];
  if (size == 0 || pipe2(pfd, O_CLOEXEC) < 0)
    return -1;
  pid_t pid = start(argv, -1, pfd[1], -1);
  close(pfd[1]);
  if (pid < 0) {
    close(pfd[0]);
    return -1;
  }
  size_t total = 0;
  ssize_t r;
  char drain[256];
  /* Read to EOF even past size, so a chatty child never blocks on a full
   * pipe while we wait for it. */
  for (;;) {
    if (ms >= 0) {
      struct pollfd p = {pfd[0], POLLIN, 0};
      long long left = end - now_ms();
      if (left <= 0 || poll(&p, 1, (int)left) <= 0)
        break;
    }
    r = read(pfd[0], total < size - 1 ? out + total : drain,
             total < size - 1 ? size - 1 - total : sizeof(drain));
    if (r == 0)
      break;
    if (r < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (total < size - 1)
      total += (size_t)r;
  }
  close(pfd[0]);
  out[total] = '\0';
  while (total > 0 && (out[total - 1] == '\n' || out[total - 1] == '\r'))
    out[--total] = '\0';
  return ms < 0 ? wait_exit(pid) : wait_until(pid, end);
}

/* The exe check is USB-Arsenal's ua_read_our_pid. */
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
