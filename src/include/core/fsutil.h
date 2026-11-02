/* Small file helpers.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_FSUTIL_H
#define VAP_FSUTIL_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

/* The whole file into buf, NUL terminated, trailing newlines dropped.
 * Returns its length, or -1. */
int read_file(const char *path, char *buf, size_t size);
/* Into an existing file only (sysfs, /proc/sys): a typo in a path fails with
 * ENOENT instead of creating a stray file. 0 or -1 with errno. */
int write_file(const char *path, const char *content);
/* mkstemp next to path, body(f, arg), fsync, rename: a reader never sees half
 * a file. 0 or -1. */
int write_atomic(const char *path, mode_t mode,
                 void (*body)(FILE *, const void *), const void *arg);
int mkdir_p(const char *path, mode_t mode);

#endif
