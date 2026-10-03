# VirtualAP

Turn a rooted Android phone into a real Wi-Fi access point - with a **static gateway**, a **selectable upstream** (mobile data, Wi-Fi, Ethernet, or a VPN tunnel), and an optional **container-managed router** for a full OpenWrt control plane on your phone.

Unlike the stock Android hotspot, VirtualAP gives you a fixed LAN, control over the band/channel/security, and the ability to send every connected client through whatever network - or VPN - you choose.

## Navigation

* [Requirements](#requirements)
* [Features](#features)
* [Use Cases](#use-cases)
* [How It Works](#how-it-works)
  * [Routed Mode](#routed-mode-default)
  * [Managed Mode](#managed-mode--k-container)
  * [On the Phone](#on-the-phone)
* [License](#license)

## Requirements

* **Root access** - required to create the virtual interface and manage `ip`/`iptables` routing.
* **Architecture** - 64-bit (`aarch64`) or 32-bit (`armhf`) ARM. A single APK supports both.
* **Android** - 8.0 (API 26) or newer.

## Features

* **Static gateway IP** - the LAN gateway never changes, so port forwards, SSH configs, and bookmarks keep working across restarts. Configurable, and applies to both routed and managed modes.
* **Selectable upstream** - route hotspot clients through mobile data, Wi-Fi, Ethernet, or a virtual interface (e.g. WireGuard `tun0`). Pick one explicitly, or let **auto-detection** follow Android's active default network.
* **Wi-Fi repeater** - stay connected to a Wi-Fi network and rebroadcast it as a hotspot at the same time, no extra hardware.
* **VPN hotspot** - set a VPN tunnel as the upstream and every connected device is transparently routed through it.
* **Bands & channel control** - 2.4 GHz or 5 GHz, manual or auto channel, and 20/40/80 MHz width on 5 GHz (with safe fallback when the chip or channel can't do it).
* **Security modes** - Open, WPA2-Personal, WPA2/WPA3 transition, or WPA3-Personal (SAE), plus an optional Protected Management Frames (802.11w) toggle.
* **Hidden SSID** - broadcast or hide the network name.
* **TTL fix** (optional, `-T 1`) - forwarded packets leave with TTL 64 like the phone's own traffic, so the operator cannot tell tethered clients apart by TTL. Needs a kernel with the netfilter TTL target (`CONFIG_NETFILTER_XT_TARGET_HL`); without it the option is greyed out in the app and skipped by the backend.
* **DHCP + DNS** - served locally by `dnsmasq`, with optional custom upstream DNS servers.
* **Same-channel concurrency** - the AP follows the Wi-Fi station's current channel, which is what most phone chips require and avoids 5 GHz beaconing failures.
* **Managed mode** - hand the hotspot's LAN to a running [Droidspaces](https://github.com/ravindu644/Droidspaces) container, letting OpenWrt (or any container) own DHCP/DNS/NAT/firewall.
* **Runs directly on Android** - `hostapd`, `dnsmasq`, `iw`, and `busybox` ship as fully-static ARM binaries. No chroot, no namespaces, no Magisk module, no reboot. Routing and firewalling use Android's own `ip`/`iptables`.
* **Self-managing app** - checks root on every launch, re-deploys the backend automatically after an update, and streams the live backend log to a built-in terminal view.

## Use Cases

* **Stable tethering** - share your connection with a gateway that doesn't shuffle on every reconnect, so forwarded ports and saved SSH/RDP targets stay valid.
* **Portable VPN router** - connect the phone to a VPN, then route a whole room of devices (TV, console, laptop) through it without installing a VPN client on any of them.
* **Extend / reshare Wi-Fi** - repeat a weak network into another room, or reshare a single-device Wi-Fi login to all your gear.
* **Bridge networks** - bring an Ethernet/USB uplink to Wi-Fi clients, or vice-versa.
* **A real router on your phone** - in managed mode, run OpenWrt with full LuCI: firewall zones, traffic rules, ad-blocking, and more - governing both the Wi-Fi clients and your Droidspaces containers from one place.
* **Reach containerized services** - devices on the hotspot can talk to services running inside Droidspaces containers, with reply traffic routed correctly back to the LAN.

## How It Works

VirtualAP creates a virtual AP interface (`ap0`) on the phone's Wi-Fi chip with `iw`, then runs `hostapd` on it. From there it operates in one of two modes.

### Routed Mode (default)

VirtualAP owns all of Layer-3 for the hotspot:

* Assigns the gateway IP to `ap0` (default `192.168.42.1/24`).
* Serves DHCP and DNS with its own `dnsmasq` (lease pool `…​.10`–`…​.50`, 12 h).
* NATs client traffic out to the selected upstream using Android's `iptables`.

Traffic is steered with policy routing rules pinned **above** Android's `netd` rule range so VPN catch-all rules and the system's unreachable guards can't hijack hotspot traffic:

```
Outbound:
client ➔ ap0 (gateway IP, hostapd)
       ➔ ip rule pref 7010: from all iif ap0 lookup <upstream table>
       ➔ MASQUERADE (-s <subnet> ! -d <subnet>)
       ➔ Internet / VPN tunnel

Replies:
reply  ➔ ip rule pref 7000: to <subnet> lookup main ➔ ap0
```

The upstream table is resolved either from an explicit interface you choose, or - in `auto` mode - from Android's `netd` default-network rule, which always points at whatever network currently has internet.

**Container port-forwarding:** so hotspot clients can reach services inside Droidspaces containers, VirtualAP also mirrors the AP subnet route into Android's `local_network` table (97). Without that mirror, container reply packets (from `172.28.0.0/16`) would fall through to the WAN table and leak out the physical uplink instead of returning to the client via `ap0`.

### Managed Mode (`-K <container>`)

The hotspot's LAN is handed to a running Droidspaces container. VirtualAP assigns **no** IP, runs **no** dnsmasq, and installs **no** NAT of its own - it only builds a neutral Layer-2 path and lets the container be the router:

* `ap0` is enslaved to a host bridge `vap-br0` that carries no IP.
* A veth pair is created; the host end joins `vap-br0`, and the peer is moved into the container's network namespace and renamed `vaplan0`.
* The container provides DHCP, DNS, NAT, and firewalling for every connected client.

```
client ➔ ap0 (L2 bridge port, no IP)
       ➔ vap-br0 ➔ vaplan0 (inside the container)
       ➔ container LAN (DHCP / DNS / firewall)
       ➔ container NAT ➔ container WAN ➔ Internet
```

When the container is **OpenWrt**, VirtualAP auto-provisions it over UCI: a static `vaplan` interface using the configured gateway (default `192.168.42.1/24`), a DHCP pool, and a masqueraded firewall zone toward the WAN. Non-OpenWrt containers simply receive `vaplan0` and configure it themselves.

Because the container owns the LAN, a single OpenWrt instance can route the Wi-Fi hotspot **and** one or more Droidspaces gateway-mode containers at the same time - all from the same LuCI control plane - turning the phone into a self-contained router for physical clients and containerized workloads alike.

### On the Phone

The backend is a single shell engine (`start-ap`) plus the static binaries, deployed to `/data/local/virtualap`. The Android app is the control surface: it validates root, installs/updates the backend automatically (re-deploying whenever an app update ships new binaries), persists your configuration, and streams the live log to a terminal view - no Magisk module or reboot required.

## License

Licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE).

---

> [!NOTE]
> **Heritage & assistance.** VirtualAP grew out of the [`start-hotspot`](https://github.com/ravindu644/Ubuntu-Chroot/blob/main/tools/start-hotspot) script from my own [Ubuntu-Chroot](https://github.com/ravindu644/Ubuntu-Chroot) project - the original idea and core of this backend - now reworked into fully-static binaries that run directly on Android. The Android front-end is developed with the assistance of an AI companion.
