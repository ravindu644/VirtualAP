/* The commands main dispatches to. Each takes the settings as loaded from
 * ap.conf and overridden by the flags, and returns the exit code.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CMD_H
#define VAP_CMD_H

#include "cli/options.h"

int cmd_start(struct vap_opts *o);
int cmd_stop(struct vap_opts *o);
int cmd_status(struct vap_opts *o);
/* stop's teardown, for start to clear a previous AP under its own lock. */
void stop_ap(const struct vap_opts *o);
int cmd_interfaces(struct vap_opts *o);
int cmd_caps(struct vap_opts *o);

#endif
