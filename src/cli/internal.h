/* What cli/'s own files share, and the tests check: not part of its
 * public header. Included as "internal.h", or by path from tests/.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CLI_INTERNAL_H
#define VAP_CLI_INTERNAL_H

#include "cli/cli.h"

/* options.c: The settings: one table gives each its flag, its ap.conf key,
 * its default and its usage line, so the parser, the config file and --help
 * never drift. */

/* In the order ap.conf lists them. */
extern const struct vap_key vap_keys[];
extern const size_t vap_nkeys;

const char *opt_cfield(const struct vap_opts *o, const struct vap_key *k);
const struct vap_key *opt_by_conf(const char *name);

/* The one way a value gets in, from a flag or from ap.conf: refuses a line
 * break (ap.conf, hostapd.conf and run.state are line based, so it would
 * become a directive of its own) and anything that does not fit, logging
 * which. 0 or -1. */
int opt_set(struct vap_opts *o, const struct vap_key *k, const char *val);

/* conf.c: ap.conf, in the format v2.1.0's shell engine sourced, so a
 * downgrade still reads it. */

/* One sh word (quotes and backslashes resolved) from *s into out; *s moves
 * past it. Exposed for the tests. */
void conf_unquote(const char **s, char *out, size_t size);

#endif
