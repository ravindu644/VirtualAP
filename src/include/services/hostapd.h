/* hostapd: its config, its start, and what status asks it.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_HOSTAPD_H
#define VAP_HOSTAPD_H

#include "cli/options.h"
#include "radio/channel.h"
#include "virtualap.h"

#define HOSTAPD_CONF VAP_RUN "/hostapd.conf"
#define HOSTAPD_PID VAP_RUN "/hostapd.pid"
#define HOSTAPD_CTRL VAP_RUN "/hostapd"

/* hostapd.conf for the plan. 0 or -1. */
int hostapd_write_conf(const char *path, const struct vap_opts *o,
                       const struct plan *p);
/* Write the conf, launch hostapd and confirm it is still up 2 s later. A
 * 40 MHz 2.4 GHz plan the firmware refuses is retried at 20 MHz (p->width is
 * updated). 0, or -1 after logging why. */
int hostapd_start(const struct vap_opts *o, struct plan *p);
/* hostapd's own station list (MediaTek's gen4m has no dump_station, so the
 * driver's is always empty there). 0 when hostapd does not answer within 1 s:
 * a hung hostapd must not stall the status the app polls every 3 s. */
int hostapd_clients(void);

#endif
