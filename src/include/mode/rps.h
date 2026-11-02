/* Receive packet steering on the managed-mode veth.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_RPS_H
#define VAP_RPS_H

#include <stdbool.h>
#include <stddef.h>

/* A veth queues each packet on the CPU that sent it, and one flow arrives
 * from several: the Wi-Fi driver spreads its receive work, the container's
 * TCP sends from whichever core took the last ACK. The far end then sees the
 * flow out of order and retransmits what was never lost. RPS hashes each
 * flow to one CPU: every CPU but CPU0, the mask Droidspaces uses on its own
 * veths. Best effort; true when both ends took it. */

/* The mask written, every CPU but CPU0, 32-bit words most significant
 * first: "fe" for 0-7, "ff,fffffffe" for 0-39, "" for one CPU or a buffer too
 * short. RPS_MASK_MAX holds the widest, 1024 CPUs. */
#define RPS_MASK_MAX 300
void rps_mask(char *out, size_t size);
/* The same from the text of /sys/devices/system/cpu/present, for the tests. */
void rps_mask_from(const char *present, char *out, size_t size);
/* mask on ifname in the host's namespace. */
bool rps_host(const char *ifname, const char *mask);
/* ifname in the namespace of netns_fd. Moving a link between namespaces
 * drops its map, and the host's /sys shows only the host's links, so a
 * child steps in and mounts a sysfs of its own for that namespace; never
 * the container's /proc/<pid>/root/sys, whose symlinks the container picks. */
bool rps_in_netns(int netns_fd, const char *ifname, const char *mask);

#endif
