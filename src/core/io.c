/* Adapted from USB-Arsenal's utils.c and config.c.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/fsutil.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
