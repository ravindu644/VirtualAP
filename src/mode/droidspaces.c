/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/droidspaces.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "core/proc.h"
#include "virtualap.h"

pid_t ds_container_pid(const char *name) {
  /* Droidspaces checks its own pid file against the live process; a stopped
   * container prints NONE and exits 1. */
  char buf[32];
  const char *const argv[] = {DROIDSPACES_BIN, "-n", name, "pid", NULL};
  long pid = run_capture(argv, buf, sizeof(buf)) == 0 ? atol(buf) : 0;
  return pid > 1 ? (pid_t)pid : 0;
}

int ds_run_script(const char *name, const char *script,
                  const char *const args[],
                  void (*line)(const char *text, void *arg), void *arg) {
  const char *argv[24] = {DROIDSPACES_BIN, "-n", name, "--stdin",
                          "run",           "sh", "-s", "--"};
  int n = 8;
  for (int i = 0; args[i] && n < 23; i++)
    argv[n++] = args[i];
  argv[n] = NULL;
  return run_lines(argv, script, line, arg);
}

bool ds_is_openwrt(const char *name) {
  /* Read inside the container: through /proc/<pid>/root an absolute
   * symlink would resolve against the host. */
  char buf[4096];
  const char *const argv[] = {DROIDSPACES_BIN,   "-n", name, "run", "cat",
                              "/etc/os-release", NULL};
  if (run_capture(argv, buf, sizeof(buf)) != 0)
    return false;
  for (char *l = buf; l; l = strchr(l, '\n') ? strchr(l, '\n') + 1 : NULL) {
    if (strncmp(l, "ID=", 3))
      continue;
    l += 3;
    if (*l == '"' || *l == '\'')
      l++;
    return !strncasecmp(l, "openwrt", 7);
  }
  return false;
}
