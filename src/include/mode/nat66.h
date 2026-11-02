/* NAT66 the Droidspaces way: a private /64 on ap0, masqueraded out of
 * whatever the upstream is. Survives an upstream or prefix change untouched.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NAT66_H
#define VAP_NAT66_H

#include "mode/ipv6.h"

/* ip6table_nat with MASQUERADE, and an IPv6 default route in the upstream's
 * table to masquerade out of. */
bool nat66_possible(struct nl *nl, const struct upstream *up);
/* v->net/prefix/gw filled from the gateway's third octet; 0, or -1 with the
 * reason in why. */
int nat66_setup(struct nl *nl, const struct upstream *up,
                const struct subnet *net, struct v6 *v, const char **why);

#endif
