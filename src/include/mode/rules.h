/* The policy rules of a routed AP, added by the setup and the monitor and
 * removed by the teardowns, all from here so the three never disagree.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_RULES_H
#define VAP_RULES_H

#include <stdbool.h>

#include "core/core.h"
#include "mode/ipv6.h"
#include "net/netd.h"

/* IPv4's when net is set: replies to net via main (7000), everything from
 * ap0 via the upstream's table (7010). IPv6's when v has a mode: the same for
 * v's /64, ap0's link-local routes via main (7002), and for a shared /64 the
 * sink of what no client holds (7001). Each added unless already there;
 * with verbose a failure is logged with its priority (the monitor, which
 * retries every beat, asks quietly). 0, or -1 when any failed (every one is
 * still tried). */
int rules_add(struct nl *nl, const struct upstream *up,
              const struct subnet *net, const struct v6 *v, bool verbose);
/* Every rule of family at the priorities rules_add uses, whoever added it. */
void rules_del(struct nl *nl, int family);
/* Only the rules that name the upstream's table (PRIO_FROM_AP), both
 * families, a failure logged: an upstream switch moves those and leaves the
 * clients' return path in place. */
void rules_del_upstream(struct nl *nl);

#endif
