/* start-ap's teardown_bridged and del_bridge.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mode/managed.h"

#include <unistd.h>

#include "core/log.h"
#include "mode/ap.h"
#include "net/link.h"
#include "virtualap.h"

void managed_teardown(const char *container) {
  vap_log("Tearing down managed (bridged) AP...");
  ap_daemons_stop();
  ap_iface_del();
  struct nl nl;
  if (nl_open(&nl) == 0) {
    link_del(&nl, VAP_VETH_HOST);
    link_del(&nl, VAP_BRIDGE);
    nl_close(&nl);
  }
  unlink(VAP_STATE);
  vap_log("Managed AP stopped (container '%s' left running).", container);
}
