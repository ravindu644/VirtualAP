/* Running other programs without a shell.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_PROC_H
#define VAP_PROC_H

#include <stddef.h>

/* argv's stdout into out (trailing newlines dropped), stderr to /dev/null.
 * Returns the exit code, -1 when it could not run or was killed. musl's
 * popen() runs /bin/sh, which Android does not have. */
int run_capture(char *const argv[], char *out, size_t size);
/* argv with stdout and stderr on /dev/null; exit code or -1. */
int run_quiet(char *const argv[]);

#endif
