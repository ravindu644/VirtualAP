/* The Wi-Fi's factory MAC, what the app's Static MAC mode starts from.
 * Copyright (c) 2026 ravindu644
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VAP_WIFI_MAC_H
#define VAP_WIFI_MAC_H

#include <stdbool.h>

/* aa:bb:cc:dd:ee:ff in lower case, or "" when no source has it:
 *  1. Android's Wi-Fi service (11 and up) saves the factory MAC it read from
 *     the HAL in its settings store, whatever the vendor or driver.
 *  2. ifname's address, but only while the kernel says nobody changed it
 *     (addr_assign_type 0): Android gives wlan0 a random MAC per network.
 *  3. The kernel's permanent address.
 *  4. Samsung's copy in efs, with no trailing newline: aa:bb:.. on Broadcom
 *     phones, twelve bare hex digits on MediaTek ones. */
void wifi_mac(const char *ifname, char out[18]);

/* The MAC at the start of s, aa:bb:cc:dd:ee:ff or twelve bare hex digits,
 * as aa:bb:.. in lower case. False for anything else, or all zeros. */
bool mac_take(const char *s, char out[18]);

#endif
