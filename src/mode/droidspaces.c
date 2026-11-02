/* start-ap's ds_run, ds_list, ds_container_pid and ds_is_openwrt.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/droidspaces.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "core/fsutil.h"
#include "core/proc.h"
#include "virtualap.h"

/* The install path, else whatever PATH has. */
static const char *ds_bin(void) {
  return access(DROIDSPACES_BIN, X_OK) == 0 ? DROIDSPACES_BIN : "droidspaces";
}

pid_t ds_container_pid(const char *name) {
  char path[300], buf[32];
  snprintf(path, sizeof(path), "/data/local/Droidspaces/Pids/%s.pid", name);
  for (char *p = path + strlen("/data/local/Droidspaces/Pids/"); *p; p++)
    if (*p == ' ')
      *p = '-';
  if (read_file(path, buf, sizeof(buf)) > 0) {
    /* Only a container init carries Droidspaces' marker: the file outlives
     * a reboot, and its pid may by now be anyone's. */
    long pid = atol(buf);
    char marker[64];
    snprintf(marker, sizeof(marker), "/proc/%ld/root/run/droidspaces", pid);
    if (pid > 1 && access(marker, F_OK) == 0)
      return (pid_t)pid;
  }
  const char *const argv[] = {ds_bin(), "-n", name, "pid", NULL};
  if (run_capture(argv, buf, sizeof(buf)) == 0 && atol(buf) > 1)
    return (pid_t)atol(buf);
  return 0;
}

int ds_list(void (*fn)(const char *name, pid_t pid, void *arg), void *arg) {
  static char out[64 * 1024];
  const char *const argv[] = {ds_bin(), "show", "--json", NULL};
  if (run_capture(argv, out, sizeof(out)) != 0)
    return -1;
  /* {"running":[{"name":"X","pid":N,...},...]}: every object of the running
   * list starts with its name and its pid. */
  static const char key[] = "{\"name\":\"";
  for (const char *p = strstr(out, key); p; p = strstr(p, key)) {
    p += sizeof(key) - 1;
    const char *end = strchr(p, '"');
    if (!end || strncmp(end, "\",\"pid\":", 8))
      continue;
    char name[256];
    size_t len = (size_t)(end - p);
    if (len >= sizeof(name))
      continue;
    memcpy(name, p, len);
    name[len] = '\0';
    fn(name, (pid_t)atol(end + 8), arg);
    p = end;
  }
  return 0;
}

int ds_run_script(const char *name, const char *script,
                  const char *const args[],
                  void (*line)(const char *text, void *arg), void *arg) {
  const char *argv[24] = {ds_bin(), "-n",  name, "--stdin", "run",
                          "sh",     "-s", "--"};
  int n = 8;
  for (int i = 0; args[i] && n < 23; i++)
    argv[n++] = args[i];
  argv[n] = NULL;
  return run_lines(argv, script, line, arg);
}

bool ds_is_openwrt(const char *name) {
  /* Read inside the container, as start-ap's ds_run did: through
   * /proc/<pid>/root an absolute symlink would resolve against the host. */
  char buf[4096];
  const char *const argv[] = {ds_bin(), "-n", name, "run", "cat",
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
