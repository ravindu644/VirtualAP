/* The daemon a successful start leaves behind: it follows the upstream and
 * repairs what netd undoes, for as long as the AP is up.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MONITOR_H
#define VAP_MONITOR_H

#include "cli/options.h"
#include "mode/routed.h"
#include "virtualap.h"

#define MONITOR_PID VAP_RUN "/monitor.pid"

/* Fork the monitor for the routed AP r describes, or for a managed one when
 * r is NULL, and return once it runs. 0, or -1 after a warning: the AP itself
 * is up either way, it just is not kept right. A managed AP's monitor
 * re-plugs the veth when the container restarts. */
int monitor_spawn(const struct vap_opts *o, struct routed *r);
/* SIGTERM it, if the pid file names one of ours, and wait for it to go. */
void monitor_stop(void);

#endif
