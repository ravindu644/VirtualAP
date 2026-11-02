/* What monitor/'s own files share, and the tests check: not part of its
 * public header. Included as "internal.h", or by path from tests/.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MONITOR_INTERNAL_H
#define VAP_MONITOR_INTERNAL_H

#include "monitor/monitor.h"

/* monitor.c */

#define MONITOR_PID VAP_RUN "/monitor.pid"

/* upstream.c: One look at the network: follow the upstream, put back what
 * was taken. */

/* Re-resolve the upstream and move rule 7010 when it changed, re-add the
 * rules and firewall entries netd flushed, re-assert forwarding, and redo
 * IPv6 when its picture changed (another /64, another interface, IPv6
 * appearing or gone), restarting dnsmasq for the new advertisement. r and
 * run.state follow. Returns true when IPv6 was redone. Writes only what is
 * missing, so its own changes never wake it again. */
bool reprobe(struct nl *nl, const struct vap_opts *o, struct routed *r);

#endif
