/* The hostapd / wpa_supplicant control socket, what hostapd_cli speaks.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_WPA_CTRL_H
#define VAP_WPA_CTRL_H

#include <stddef.h>

/* cmd to the daemon listening at path, its reply in reply (NUL terminated)
 * within timeout_ms. Returns the reply's length, or -errno (-ETIMEDOUT for
 * a daemon that never answers, so a hung hostapd cannot stall a status;
 * -EMSGSIZE for a reply that does not fit). */
int wpa_ctrl_request(const char *path, const char *cmd, char *reply,
                     size_t size, int timeout_ms);
/* cmd without waiting for the reply: wpa_supplicant runs a driver command
 * before it answers, and on Samsung's firmware the answer never comes back
 * (SELinux). 0 or -errno. */
int wpa_ctrl_send(const char *path, const char *cmd);

#endif
