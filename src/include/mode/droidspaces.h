/* Droidspaces containers, as managed mode needs them.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_DROIDSPACES_H
#define VAP_DROIDSPACES_H

#include <stdbool.h>
#include <sys/types.h>

/* The running container's init as `droidspaces -n name pid` reports it, 0
 * when it is not running. */
pid_t ds_container_pid(const char *name);
/* script on sh's stdin inside the container, args as its $1..: values never
 * pass through a shell's quoting. line(text) for each line it prints. The
 * container's own namespaces, cgroup and limits apply, as droidspaces run
 * sets them. Exit code, or -1. */
int ds_run_script(const char *name, const char *script,
                  const char *const args[],
                  void (*line)(const char *text, void *arg), void *arg);
/* Its /etc/os-release says ID=openwrt (quoted or not). */
bool ds_is_openwrt(const char *name);

#endif
