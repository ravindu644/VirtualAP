/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/csv.h"

#include <ctype.h>
#include <stddef.h>

int csv_each(const char *list, int (*fn)(const char *item, void *arg),
             void *arg) {
  char buf[64];
  size_t n = 0;
  for (const char *p = list;; p++) {
    if (*p == ',' || !*p) {
      buf[n] = '\0';
      int ret = n ? fn(buf, arg) : 0;
      if (ret)
        return ret;
      n = 0;
      if (!*p)
        return 0;
    } else if (!isspace((unsigned char)*p) && n + 1 < sizeof(buf)) {
      buf[n++] = *p;
    }
  }
}
