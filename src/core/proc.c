/* Adapted from USB-Arsenal's utils.c.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/proc.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int wait_exit(pid_t pid) {
  int status;
  while (waitpid(pid, &status, 0) < 0)
    if (errno != EINTR)
      return -1;
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* stdout goes to out_fd (or /dev/null when -1), stderr to /dev/null. The
 * parent's stdout is flushed first, or the child would inherit and print
 * whatever the buffer held a second time. */
static pid_t start(const char *const argv[], int out_fd) {
  fflush(stdout);
  pid_t pid = fork();
  if (pid != 0)
    return pid;
  /* stdin too: under the app it is libsu's command pipe, and a child reading
   * it would swallow the app's next command. */
  int null = open("/dev/null", O_RDWR | O_CLOEXEC);
  dup2(null, STDIN_FILENO);
  dup2(out_fd >= 0 ? out_fd : null, STDOUT_FILENO);
  dup2(null, STDERR_FILENO);
  /* An ignored signal stays ignored across exec; main ignores SIGPIPE for
   * itself only. */
  signal(SIGPIPE, SIG_DFL);
  /* execv's prototype predates const; it never writes through argv. */
  execv(argv[0], (char *const *)(uintptr_t)argv);
  _exit(127);
}

int run_quiet(const char *const argv[]) {
  pid_t pid = start(argv, -1);
  return pid < 0 ? -1 : wait_exit(pid);
}

int run_capture(const char *const argv[], char *out, size_t size) {
  int pfd[2];
  if (size == 0 || pipe2(pfd, O_CLOEXEC) < 0)
    return -1;
  pid_t pid = start(argv, pfd[1]);
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
