/* NDP proxy entries: the upstream router finds a client behind the phone.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_NEIGH_H
#define VAP_NEIGH_H

#include <stdbool.h>

#include "net/nl.h"

/* `ip -6 neigh replace|del proxy addr dev dev`. 0 or -errno; deleting one
 * that is not there is 0. */
int neigh_proxy(struct nl *nl, bool add, const char *addr, const char *dev);

#endif
