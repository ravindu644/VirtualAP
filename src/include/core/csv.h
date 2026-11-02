/* Comma separated lists, as the settings carry them (-d).
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_CSV_H
#define VAP_CSV_H

/* fn on each entry of list, blanks removed and empties skipped, as the sh
 * engine's word splitting had it. Stops at the first non-zero fn and
 * returns it, else 0. Entries are cut at 63 bytes. */
int csv_each(const char *list, int (*fn)(const char *item, void *arg),
             void *arg);

#endif
