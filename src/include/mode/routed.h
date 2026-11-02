/* Routed mode: ap0 is the clients' gateway, the phone NATs them out of the
 * upstream netd picked (or -o named).
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_ROUTED_H
#define VAP_ROUTED_H

#include "cli/options.h"

/* The whole start after the settings were validated and saved. 0, or -1
 * after logging why and tearing down whatever was set up. */
int routed_start(struct vap_opts *o);
/* Undo a routed AP from run.state (or what the settings say, when there is
 * none), every step tolerant of what is already gone. */
void routed_teardown(const struct vap_opts *o);

#endif
