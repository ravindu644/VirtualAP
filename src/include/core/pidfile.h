/* Pid files that never signal a stranger.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_PIDFILE_H
#define VAP_PIDFILE_H

#include <sys/types.h>

/* The pid in path if that process is alive and its program is named exe
 * ("hostapd", "dnsmasq", "virtualap"), else 0. The files live in /data and
 * survive a reboot, after which the pid may belong to anything. */
pid_t pidfile_ours(const char *path, const char *exe);
/* SIGTERM the process if it is ours, wait up to 2 s for it to go, then remove
 * the file either way: deleting ap0 under a hostapd still tearing down its own
 * interface is a race the kernel does not always win. */
void pidfile_kill(const char *path, const char *exe);
int pidfile_write(const char *path, pid_t pid);
/* The pid in path if that process's command line holds needle, else 0: for
 * a shell loop, whose exe is just sh. */
pid_t pidfile_cmdline(const char *path, const char *needle);

#endif
