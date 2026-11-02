/* Routed mode: ap0 is the clients' gateway, the phone NATs them out of the
 * upstream netd picked (or -o named).
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_ROUTED_H
#define VAP_ROUTED_H

#include "cli/options.h"
#include "core/ipaddr.h"
#include "mode/ipv6.h"
#include "net/netd.h"

/* What a running routed AP was set up with, for the monitor to keep right. */
struct routed {
  struct upstream up;
  struct subnet net;
  struct v6 v6;
};

/* The whole start after the settings were validated and saved, lan the
 * subnet of the gateway start checked; r filled on success. 0, or -1 after
 * logging why and tearing down what was set up. */
int routed_start(struct vap_opts *o, const struct subnet *lan,
                 struct routed *r);
/* Undo a routed AP from run.state (or what the settings say, when there is
 * none), every step tolerant of what is already gone. */
void routed_teardown(const struct vap_opts *o);

#endif
