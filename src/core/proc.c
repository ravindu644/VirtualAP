/* Adapted from USB-Arsenal's utils.c.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/proc.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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

int run_capture(const char *const argv[], char *out, size_t size) {
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
  while ((r = read(pfd[0], total < size - 1 ? out + total : drain,
                   total < size - 1 ? size - 1 - total : sizeof(drain))) != 0) {
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
  return wait_exit(pid);
}
