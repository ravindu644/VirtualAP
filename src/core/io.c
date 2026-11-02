/* Adapted from USB-Arsenal's utils.c and config.c.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "core/core.h"
#include "virtualap.h"

int read_file(const char *path, char *buf, size_t size) {
  if (size == 0)
    return -1;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return -1;
  ssize_t total = 0, r = 1;
  while ((size_t)total < size - 1 &&
         (r = read(fd, buf + total, size - 1 - (size_t)total)) > 0)
    total += r;
  close(fd);
  if (r < 0)
    return -1;
  buf[total] = '\0';
  while (total > 0 && (buf[total - 1] == '\n' || buf[total - 1] == '\r'))
    buf[--total] = '\0';
  return (int)total;
}

int write_file(const char *path, const char *content) {
  int fd = open(path, O_WRONLY | O_TRUNC | O_CLOEXEC);
  if (fd < 0)
    return -1;
  size_t len = strlen(content), done = 0;
  while (done < len) {
    ssize_t w = write(fd, content + done, len - done);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      int saved = errno;
      close(fd);
      errno = saved;
      return -1;
    }
    done += (size_t)w;
  }
  return close(fd);
}

int write_atomic(const char *path, mode_t mode,
                 void (*body)(FILE *, const void *), const void *arg) {
  char tmp[PATH_MAX];
  if (snprintf(tmp, sizeof(tmp), "%s.XXXXXX", path) >= (int)sizeof(tmp))
    return -1;
  int fd = mkstemp(tmp);
  if (fd < 0)
    return -1;
  fchmod(fd, mode);
  FILE *f = fdopen(fd, "w");
  if (!f) {
    close(fd);
    unlink(tmp);
    return -1;
  }
  body(f, arg);
  int ok = fflush(f) == 0 && !ferror(f) && fsync(fileno(f)) == 0;
  if (fclose(f) != 0 || !ok || rename(tmp, path) < 0) {
    unlink(tmp);
    return -1;
  }
  return 0;
}

int mkdir_p(const char *path, mode_t mode) {
  char tmp[PATH_MAX];
  if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) {
    errno = ENAMETOOLONG;
    return -1;
  }
  for (char *p = tmp + 1; *p; p++) {
    if (*p != '/')
      continue;
    *p = '\0';
    if (mkdir(tmp, mode) < 0 && errno != EEXIST)
      return -1;
    *p = '/';
  }
  return mkdir(tmp, mode) < 0 && errno != EEXIST ? -1 : 0;
}

static const char *log_file = VAP_LOG;

void log_stamp(char out[32]) {
  time_t now = time(NULL);
  struct tm tm;
  strftime(out, 32, "%Y-%m-%d %H:%M:%S", localtime_r(&now, &tm));
}

void log_set_file(const char *path) { log_file = path; }

void log_truncate(void) {
  if (!log_file)
    return;
  /* A missing file stays missing: emit creates it with the first line. */
  write_file(log_file, "");
}

/* Opened per line with O_APPEND, so the lines of the start, a later stop and
 * the monitor never tear each other, whichever process writes. */
static void emit(const char *tag, const char *fmt, va_list ap) {
  char msg[1024], line[1100], stamp[32];
  vsnprintf(msg, sizeof(msg), fmt, ap);
  printf("%s%s\n", tag, msg);
  if (!log_file)
    return;
  log_stamp(stamp);
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

bool valid_ip4(const char *s) {
  struct in_addr a;
  return inet_pton(AF_INET, s, &a) == 1;
}

bool prefix_parse(int family, const char *s, unsigned char bin[16], int *len) {
  char buf[INET6_ADDRSTRLEN + 8];
  if (snprintf(buf, sizeof(buf), "%s", s) >= (int)sizeof(buf))
    return false;
  int max = family == AF_INET6 ? 128 : 32;
  *len = max;
  char *slash = strchr(buf, '/');
  if (slash) {
    *slash = '\0';
    char *end;
    long l = strtol(slash + 1, &end, 10);
    if (end == slash + 1 || *end || l < 0 || l > max)
      return false;
    *len = (int)l;
  }
  return inet_pton(family, buf, bin) == 1;
}

bool subnet_from_gw(const char *gw, struct subnet *out) {
  struct in_addr a;
  if (inet_pton(AF_INET, gw, &a) != 1)
    return false;
  const unsigned char *o = (const unsigned char *)&a.s_addr;
  if (o[3] == 0 || o[3] == 255)
    return false;
  snprintf(out->gw, sizeof(out->gw), "%u.%u.%u.%u", o[0], o[1], o[2], o[3]);
  snprintf(out->net, sizeof(out->net), "%u.%u.%u.0/24", o[0], o[1], o[2]);
  snprintf(out->begin, sizeof(out->begin), "%u.%u.%u.10", o[0], o[1], o[2]);
  snprintf(out->end, sizeof(out->end), "%u.%u.%u.50", o[0], o[1], o[2]);
  out->third = o[2];
  return true;
}

int csv_each(const char *list, int (*fn)(const char *item, void *arg),
             void *arg) {
  char buf[64];
  size_t n = 0;
  for (const char *p = list;; p++) {
    if (*p == ',' || !*p) {
      buf[n] = '\0';
      int ret = n ? fn(buf, arg) : 0;
      if (ret)
        return ret;
      n = 0;
      if (!*p)
        return 0;
    } else if (!isspace((unsigned char)*p) && n + 1 < sizeof(buf)) {
      buf[n++] = *p;
    }
  }
}
