/* VirtualAP engine: turns a rooted phone's Wi-Fi into an access point.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cli/conf.h"
#include "cli/options.h"
#include "cmd/cmd.h"
#include "core/fsutil.h"
#include "core/log.h"
#include "virtualap.h"

static const struct command {
  const char *name;
  int (*fn)(struct vap_opts *);
} commands[] = {
    {"start", cmd_start},
    {"stop", cmd_stop},
    {"status", cmd_status},
    {"interfaces", cmd_interfaces},
    {"caps", cmd_caps},
    {"containers", cmd_containers},
};

int main(int argc, char **argv) {
  /* The app reads a pipe, which stdio would fill and hand over only at exit;
   * line by line, the log sheet follows the start as it happens. */
  setvbuf(stdout, NULL, _IOLBF, 0);
  /* An app that dies mid-start closes the pipe; finishing the start or the
   * teardown matters more than the lines nobody reads. */
  signal(SIGPIPE, SIG_IGN);

  if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") ||
      !strcmp(argv[1], "help")) {
    print_usage("virtualap");
    return 0;
  }
  if (geteuid() != 0) {
    log_set_file(NULL);
    vap_error("virtualap must run as root.");
    return 1;
  }
  mkdir_p(VAP_BASE "/logs", 0755);
  mkdir_p(VAP_RUN, 0755);

  const struct command *cmd = NULL;
  for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++)
    if (!strcmp(argv[1], commands[i].name))
      cmd = &commands[i];
  if (!cmd) {
    vap_error("Unknown command: %s", argv[1]);
    print_usage("virtualap");
    return 1;
  }

  /* Saved settings first, the flags on top: a flag left out keeps its value
   * from the last start. */
  struct vap_opts o;
  opts_defaults(&o);
  conf_load(VAP_CONF, &o);
  if (opts_parse(&o, argc - 1, argv + 1) < 0)
    return 1;
  return cmd->fn(&o);
}
