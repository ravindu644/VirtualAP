/* Working in a container's network namespace from the host.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NETNS_H
#define VAP_NETNS_H

#include <sys/types.h>

/* /proc/<pid>/ns/net, opened once and used for every step, so a pid reused
 * in between can never redirect one. -EINVAL when it is our own namespace
 * (a container on the host's network has no veth to give), -errno else. */
int netns_open(pid_t pid);
/* The inode of pid's network namespace, 0 when pid is gone. It names the
 * namespace while that lives; a freed one's inode can be handed out again. */
ino_t netns_ino(pid_t pid);
/* fn(arg) with this thread inside fd's namespace, then back. setns moves the
 * calling thread only, so the engine stays single threaded. fn's return, or
 * -errno when the namespace could not be entered. Not coming back is fatal:
 * the rest of the run would act on the container. */
int in_netns(int fd, int (*fn)(void *arg), void *arg);

#endif
