/* The commands main dispatches to. Each takes the settings as loaded from
 * ap.conf and overridden by the flags, and returns the exit code.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CMD_H
#define VAP_CMD_H

#include "cli/cli.h"

/* start.c */

int cmd_start(struct vap_opts *o);
int cmd_save(struct vap_opts *o);
int cmd_stop(struct vap_opts *o);

/* status.c */

int cmd_status(struct vap_opts *o);
int cmd_clients(struct vap_opts *o);
int cmd_interfaces(struct vap_opts *o);
int cmd_caps(struct vap_opts *o);

/* block.c */

int cmd_block(struct vap_opts *o);
int cmd_unblock(struct vap_opts *o);
int cmd_blocked(struct vap_opts *o);
int cmd_allow(struct vap_opts *o);
int cmd_disallow(struct vap_opts *o);
int cmd_allowed(struct vap_opts *o);
int cmd_allow_only(struct vap_opts *o);

#endif
