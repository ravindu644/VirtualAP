/* The TTL fix: forwarded packets leave with TTL (hop limit) 64, so tethered
 * traffic looks like the phone's own to the operator.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_TTL_H
#define VAP_TTL_H

#include <stdbool.h>

/* Does the kernel take the TTL target? Asked by building the real rule in a
 * chain nothing jumps to, so no packet ever sees it; one test covers a kernel
 * without the target and an iptables without the extension. */
bool ttl_supported(void);
/* Our rules live in a chain of their own, vap_ttl, so clearing them never
 * depends on knowing which subnet they were written for. */
void ttl_clear(void);
/* src4 = the tethered IPv4 source subnet, src6 the IPv6 one or NULL. 0, or -1
 * when the TTL target is missing (everything undone). */
int ttl_apply(const char *src4, const char *src6);
/* For a new IPv6 source prefix (NULL: none) while the fix is on: the IPv6
 * half again with the IPv4 rule left in place, so an IPv6 change never leaves
 * IPv4 unfixed while iptables runs; everything again when the IPv4 rule is
 * gone. ttl_apply's 0 or -1. */
int ttl_refresh(const char *src4, const char *src6);

#endif
