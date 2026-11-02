/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/log.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "virtualap.h"

static const char *log_file = VAP_LOG;

void log_set_file(const char *path) { log_file = path; }

void log_truncate(void) {
  if (!log_file)
    return;
  int fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd >= 0)
    close(fd);
}

/* Opened per line with O_APPEND, so the lines of the start, a later stop and
 * the monitor never tear each other, whichever process writes. */
static void emit(const char *tag, const char *fmt, va_list ap) {
  char msg[1024], line[1100], stamp[32];
  vsnprintf(msg, sizeof(msg), fmt, ap);
  printf("%s%s\n", tag, msg);
  if (!log_file)
    return;
  time_t now = time(NULL);
  struct tm tm;
  strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", localtime_r(&now, &tm));
  int n = snprintf(line, sizeof(line), "[%s] %s%s\n", stamp, tag, msg);
  int fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  if (fd < 0)
    return;
  if (n > (int)sizeof(line) - 1)
    n = (int)sizeof(line) - 1;
  ssize_t w = write(fd, line, (size_t)n);
  (void)w;
  close(fd);
}

#define LEVEL(name, tag)                                                       \
  void name(const char *fmt, ...) {                                            \
    va_list ap;                                                                \
    va_start(ap, fmt);                                                         \
    emit(tag, fmt, ap);                                                        \
    va_end(ap);                                                                \
  }

LEVEL(vap_log, "")
LEVEL(vap_warn, "[WARN] ")
LEVEL(vap_error, "[ERROR] ")
