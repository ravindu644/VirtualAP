/* The engine's base: running programs, pid files, the lock, files, the log
 * and the small parsers.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CORE_H
#define VAP_CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

/* proc.c: Running other programs without a shell. */

/* argv's stdout into out (trailing newlines dropped), stderr to /dev/null.
 * Returns the exit code, -1 when it could not run or was killed. musl's
 * popen() runs /bin/sh, which Android does not have. */
int run_capture(const char *const argv[], char *out, size_t size);
/* run_capture that gives up after ms: SIGKILL, and -1. */
int run_capture_within(const char *const argv[], char *out, size_t size,
                       int ms);
/* argv with stdout and stderr appended to log (a daemon's own log, so an error
 * it prints before daemonizing is kept). Exit code or -1. */
int run_logged(const char *const argv[], const char *log);
/* argv with input on its stdin, line(text, arg) for each line it prints
 * (stdout and stderr together). argv[0] is an absolute path. Exit code
 * or -1. */
int run_lines(const char *const argv[], const char *input,
              void (*line)(const char *text, void *arg), void *arg);
/* argv with our stdout and stderr, so what it prints reaches the app. Exit
 * code or -1. */
int run_shown(const char *const argv[]);
/* argv with stdout and stderr on /dev/null; exit code or -1. */
int run_quiet(const char *const argv[]);

/* proc.c: Pid files that never signal a stranger. */

/* The pid in path if that process is alive and its program is named exe
 * ("hostapd", "dnsmasq", "virtualap"), else 0. The files live in /data and
 * survive a reboot, after which the pid may belong to anything. */
pid_t pidfile_ours(const char *path, const char *exe);
/* SIGTERM the process if it is ours, wait up to 2 s for it to go, then remove
 * the file either way: deleting ap0 under a hostapd still tearing down its own
 * interface is a race the kernel does not always win. */
void pidfile_kill(const char *path, const char *exe);
int pidfile_write(const char *path, pid_t pid);

/* proc.c: One start or stop at a time. */

/* Takes an exclusive flock on run/lock, waiting up to 30 s for another start
 * or stop (the app's button, the Quick Settings tile and a shell can all fire
 * at once). 0, or -1 after logging why. Released at exit, or by vap_unlock in
 * a forked child that must not hold it. */
int vap_lock(void);
void vap_unlock(void);

/* io.c: Small file helpers. */

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

/* io.c: Logging: stdout for the app's log sheet, a timestamped copy in
 * ap.log. */

/* The app tells the levels apart by these tags, so they are the contract. */
void vap_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void vap_warn(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void vap_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* "YYYY-mm-dd HH:MM:SS" local time, the log's and run.state's stamp. */
void log_stamp(char out[32]);
/* Where the timestamped copy goes; NULL keeps it on stdout only (tests). */
void log_set_file(const char *path);
/* One session per file: nothing reads ap.log but a person with a shell, and
 * without this it grows for as long as the phone lives. */
void log_truncate(void);

/* io.c: Address arithmetic the engine needs, nothing more. */

/* A dotted IPv4 address as inet_pton reads it: four octets of at most three
 * digits, no leading zero. */
bool valid_ip4(const char *s);

/* "addr/len" (or a bare address, a host prefix) into bin and *len, strictly:
 * a missing, empty or out of range length is refused, never read as /0. */
bool prefix_parse(int family, const char *s, unsigned char bin[16], int *len);

/* The AP's /24, derived from its gateway: the last octet is the host part. */
struct subnet {
  char gw[16];         /* 192.168.42.1 */
  char net[20];        /* 192.168.42.0/24 */
  char begin[16];      /* .10, the DHCP pool */
  char end[16];        /* .50 */
  unsigned char third; /* 42: the /64s are numbered after it */
};
/* false when gw is not a host address (.0 and .255 refused). */
bool subnet_from_gw(const char *gw, struct subnet *out);

/* io.c: Comma separated lists, as the settings carry them (-d). */

/* fn on each entry of list, blanks removed and empties skipped, as the sh
 * engine's word splitting had it. Stops at the first non-zero fn and
 * returns it, else 0. Entries are cut at 63 bytes. */
int csv_each(const char *list, int (*fn)(const char *item, void *arg),
             void *arg);

#endif
