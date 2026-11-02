/* Shared by the tests/check_*.c files, run by `make check`.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CHECK_H
#define VAP_CHECK_H

#include <stdio.h>

extern int failed;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
      failed++;                                                                \
    }                                                                          \
  } while (0)

/* A mkstemp or mkdtemp template where the tests may write: TMPDIR, else
 * /tmp, which Android does not have (run there with TMPDIR=/data/local/tmp).
 * A TMPDIR too long for out is a failed check, not a cut template. */
void tmp_path(char *out, size_t size);

/* One per area, each in its own file: the netlink tests need libc's
 * net/if.h, the iptables ones the kernel's linux/if.h, and glibc will not
 * have both in one file. */
void test_ipt(void);
void test_radio(void);
void test_services(void);
void test_netlink_in_netns(void);

#endif
