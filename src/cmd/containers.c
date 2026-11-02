/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>

#include "cmd/cmd.h"
#include "mode/droidspaces.h"

static void print(const char *name, pid_t pid, void *arg) {
  (void)pid;
  (void)arg;
  printf("%s\n", name);
}

/* Running Droidspaces containers, one name per line; nothing when it is not
 * installed or nothing runs, which is how the app knows to hide its
 * container controls. Always 0. */
int cmd_containers(struct vap_opts *o) {
  (void)o;
  ds_list(print, NULL);
  return 0;
}
