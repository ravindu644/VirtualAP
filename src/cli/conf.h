/* ap.conf, in the format the sh engine sourced, so either engine reads the
 * other's file.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CONF_H
#define VAP_CONF_H

#include "cli/options.h"

/* Missing file: nothing changes. Unknown keys are skipped, as sourcing a
 * variable nothing reads was harmless. 0, or -1 when unreadable. */
int conf_load(const char *path, struct vap_opts *o);
/* KEY='value' per line, ' written as '\'', byte for byte what save_conf in
 * the sh engine wrote. 0 or -1. */
int conf_save(const char *path, const struct vap_opts *o);

/* One sh word (quotes and backslashes resolved) from *s into out; *s moves
 * past it. Exposed for the tests. */
void conf_unquote(const char **s, char *out, size_t size);

#endif
