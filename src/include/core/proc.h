/* Running other programs without a shell.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_PROC_H
#define VAP_PROC_H

#include <stddef.h>

/* argv's stdout into out (trailing newlines dropped), stderr to /dev/null.
 * Returns the exit code, -1 when it could not run or was killed. musl's
 * popen() runs /bin/sh, which Android does not have. */
int run_capture(const char *const argv[], char *out, size_t size);
/* argv with stdout and stderr appended to log (a daemon's own log, so an error
 * it prints before daemonizing is kept). Exit code or -1. */
int run_logged(const char *const argv[], const char *log);
/* argv with input on its stdin, line(text, arg) for each line it prints
 * (stdout and stderr together). A bare name is looked up in PATH. Exit code
 * or -1. */
int run_lines(const char *const argv[], const char *input,
              void (*line)(const char *text, void *arg), void *arg);
/* argv with our stdout and stderr, so what it prints reaches the app. Exit
 * code or -1. */
int run_shown(const char *const argv[]);
/* argv with stdout and stderr on /dev/null; exit code or -1. */
int run_quiet(const char *const argv[]);

#endif
