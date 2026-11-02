/* Policy routing rules.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_RULE_H
#define VAP_RULE_H

#include <stdbool.h>

#include "net/nl.h"

struct rule {
  int family;
  const char *from; /* "addr/len", NULL for all */
  const char *to;   /* "addr/len", NULL for all */
  const char *iif;  /* NULL for any */
  const char *oif;  /* NULL for any */
  int table;        /* the table looked up, unless blackhole */
  int prio;
  bool blackhole;
};

/* Adds the rule unless an identical one is already there. Kernels before
 * 4.12 ignore NLM_F_EXCL on rules and would stack a second copy, so the dump
 * is asked first. 0 or -errno. */
int rule_add(struct nl *nl, const struct rule *ru);
bool rule_exists(struct nl *nl, const struct rule *ru);
/* Every rule of family at prio, whoever added it. 0 or -errno. */
int rule_del_prio(struct nl *nl, int family, int prio);

#endif
