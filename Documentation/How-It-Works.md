# How VirtualAP works

VirtualAP creates a virtual AP interface (`ap0`) on the phone's Wi-Fi chip with `iw`, then runs `hostapd` on it. From there it operates in one of two modes.

## Routed mode (default, "Interface" in the app)

VirtualAP owns all of Layer-3 for the hotspot:

* Assigns the gateway IP to `ap0` (default `192.168.42.1/24`).
* Serves DHCP and DNS with its own `dnsmasq` (lease pool `.10` to `.50`, 12 h).
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

The upstream table is resolved either from an explicit interface you choose, or, in `auto` mode, from Android's `netd` default-network rule, which always points at whatever network currently has internet.

**Container port forwarding.** So hotspot clients can reach services inside Droidspaces containers, VirtualAP also mirrors the AP subnet route into Android's `local_network` table (97). Without that mirror, container reply packets (from `172.28.0.0/16`) would fall through to the WAN table and leak out the physical uplink instead of returning to the client via `ap0`.

## Managed mode (`-K <container>`)

The hotspot's LAN is handed to a running Droidspaces container. VirtualAP assigns **no** IP, runs **no** dnsmasq, and installs **no** NAT of its own. It only builds a neutral Layer-2 path and lets the container be the router:

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

Because the container owns the LAN, a single OpenWrt instance can route the Wi-Fi hotspot **and** one or more Droidspaces gateway-mode containers at the same time, all from the same LuCI control plane, turning the phone into a self-contained router for physical clients and containerized workloads alike.

## On the phone

The backend is a single shell engine (`start-ap`) plus the static binaries, deployed to `/data/local/virtualap`. The Android app is the control surface: it validates root, installs/updates the backend automatically (re-deploying whenever an app update ships new binaries), persists your configuration, and streams the live log to a terminal view. No Magisk module or reboot is required.
