/* Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>

#include "cli/state.h"
#include "cmd/cmd.h"
#include "core/lock.h"
#include "mode/managed.h"
#include "mode/routed.h"
#include "monitor/monitor.h"
#include "net/ttl.h"
#include "virtualap.h"

void stop_ap(const struct vap_opts *o) {
  /* First, so it cannot put back what the teardown removes. */
  monitor_stop();
  ttl_clear();
  struct state st;
  if (state_load(VAP_STATE, &st) && !strcmp(state_get(&st, "mode"), "bridged"))
    managed_teardown(*state_get(&st, "container") ? state_get(&st, "container")
                                                   : o->container);
  else
    routed_teardown(o);
}

/* Always 0: a stop that found nothing to stop did its job. */
int cmd_stop(struct vap_opts *o) {
  if (vap_lock() < 0)
    return 1;
  stop_ap(o);
  return 0;
}
