/* run.state: what a running AP was started with, key=value per line. stop
 * tears down from it and status prints it, so it is written by the sh engine
 * and this one alike.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_STATE_H
#define VAP_STATE_H

#include <stdbool.h>
#include <stddef.h>

#define STATE_MAX 32

struct state {
  size_t n;
  bool full; /* a key was dropped: state_save refuses to write it */
  struct {
    char key[32];
    char val[256];
  } kv[STATE_MAX];
};

/* Appends, or replaces a key already set. A key past STATE_MAX is dropped
 * and marks the state full. */
void state_set(struct state *s, const char *key, const char *val);
const char *state_get(const struct state *s, const char *key);
int state_save(const char *path, const struct state *s);
/* false when there is no file: no AP is up. */
bool state_load(const char *path, struct state *s);

#endif
