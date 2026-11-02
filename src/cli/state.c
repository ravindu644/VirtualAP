/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "cli/state.h"

#include <stdio.h>
#include <string.h>

#include "core/fsutil.h"

void state_set(struct state *s, const char *key, const char *val) {
  size_t i;
  for (i = 0; i < s->n; i++)
    if (!strcmp(s->kv[i].key, key))
      break;
  if (i == s->n) {
    if (s->n == STATE_MAX) {
      s->full = true;
      return;
    }
    s->n++;
  }
  snprintf(s->kv[i].key, sizeof(s->kv[i].key), "%s", key);
  snprintf(s->kv[i].val, sizeof(s->kv[i].val), "%s", val ? val : "");
}

const char *state_get(const struct state *s, const char *key) {
  for (size_t i = 0; i < s->n; i++)
    if (!strcmp(s->kv[i].key, key))
      return s->kv[i].val;
  return "";
}

static void body(FILE *f, const void *arg) {
  const struct state *s = arg;
  for (size_t i = 0; i < s->n; i++)
    fprintf(f, "%s=%s\n", s->kv[i].key, s->kv[i].val);
}

int state_save(const char *path, const struct state *s) {
  /* A run.state missing a key would have the teardown guess it. */
  if (s->full)
    return -1;
  return write_atomic(path, 0644, body, s);
}

bool state_load(const char *path, struct state *s) {
  s->n = 0;
  s->full = false;
  FILE *f = fopen(path, "re");
  if (!f)
    return false;
  char line[512];
  while (fgets(line, sizeof(line), f)) {
    line[strcspn(line, "\r\n")] = '\0';
    char *eq = strchr(line, '=');
    if (!eq)
      continue;
    *eq = '\0';
    state_set(s, line, eq + 1);
  }
  fclose(f);
  return true;
}
