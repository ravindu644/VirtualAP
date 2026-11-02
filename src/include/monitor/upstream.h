/* One look at the network: follow the upstream, put back what was taken.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_MON_UPSTREAM_H
#define VAP_MON_UPSTREAM_H

#include <stdbool.h>

#include "cli/cli.h"
#include "mode/mode.h"

/* Re-resolve the upstream and move rule 7010 when it changed, re-add the
 * rules and firewall entries netd flushed, re-assert forwarding, and redo
 * IPv6 when its picture changed (another /64, another interface, IPv6
 * appearing or gone), restarting dnsmasq for the new advertisement. r and
 * run.state follow. Returns true when IPv6 was redone. Writes only what is
 * missing, so its own changes never wake it again. */
bool reprobe(struct nl *nl, const struct vap_opts *o, struct routed *r);
/* IPv6 off for good on this AP (the discovery a shared /64 needs could not
 * run): torn down, dnsmasq without the advertisement, not retried. */
void ipv6_off(struct nl *nl, const struct vap_opts *o, struct routed *r,
              const char *why);
/* Tear IPv6 down and set it up as want says, dnsmasq and the TTL fix
 * following. */
void ipv6_apply(struct nl *nl, const struct vap_opts *o, struct routed *r,
                const struct v6 *want);

#endif
