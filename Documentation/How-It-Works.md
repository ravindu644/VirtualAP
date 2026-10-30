# How VirtualAP works

VirtualAP creates a virtual AP interface, `ap0`, on the phone's Wi-Fi chip with `iw`, runs
`hostapd` on it, and then works in one of two modes. Everything below is done by one POSIX
shell script, `backend/start-ap`, running as root; the app only calls its subcommands.

## Routed mode ("Interface" in the app)

VirtualAP owns the hotspot's whole network layer. It gives `ap0` the gateway address
(`192.168.42.1/24` unless you chose another), serves DHCP and DNS with its own `dnsmasq`
(leases from `.10` to `.50` for 12 hours, DNS forwarded to the servers you typed, or to
`1.1.1.1` and `8.8.8.8` by default), and NATs the clients out through the upstream with
Android's `iptables`.

Traffic is steered with policy routing rules pinned above Android's own, which start at
priority 10000, so a VPN's catch-all rule cannot take the hotspot's traffic:

```
client -> ap0
       -> ip rule pref 7010: from all iif ap0 lookup <upstream table>
       -> MASQUERADE (-s <subnet> ! -d <subnet>)
       -> internet or VPN tunnel

reply  -> ip rule pref 7000: to <subnet> lookup main -> ap0
```

The upstream table comes from the interface you chose, or, on Auto, from Android's netd
default-network rule, which always points at the network that has internet right now.

The AP subnet's route is also mirrored into Android's `local_network` table (97). Without
that, a reply from a Droidspaces container (source `172.28.0.0/16`) to a hotspot client
would fall through to the WAN table and leave by the physical uplink instead of `ap0`, so
clients could not reach services inside containers.

### IPv6

Android hands the phone one /64 by SLAAC and no prefix delegation, and stock kernels rarely
ship `ip6table_nat`, so the clients get global addresses out of the phone's own /64, the
way Android's tethering does. `ap0` takes an EUI-64 address inside that /64 and `dnsmasq`
advertises the prefix off-link, so a client sends everything to the gateway and never tries
to resolve the upstream's own addresses on `ap0`. Each client address is then routed as a
/128 to `ap0` and answered for on the upstream with an NDP proxy entry, so the upstream
router finds the client behind the phone. No NAT.

A background loop finds the clients' global addresses by pinging `ff02::1` on `ap0` every
two seconds; nothing else reveals them, since the prefix is off-link and the clients never
solicit a global address. Two more rules support it: 7001 blackholes a packet from the
upstream for an address in the /64 that no client holds, which on cellular would otherwise
bounce between the phone and the gateway until its hop limit died, and 7002 gives the
phone's own packets to a client's link-local address a route, since Android files an
interface's own routes in a table no rule reads. IPv6 is best effort: when the upstream has
no global /64, or any step fails, the AP stays IPv4-only.

## Managed mode (`-K <container>`)

The hotspot's LAN is handed to a running Droidspaces container. VirtualAP gives `ap0` no
address, runs no `dnsmasq` and installs no NAT: it builds a layer-2 path and the container
is the router.

```
client -> ap0 (bridge port, no address)
       -> vap-br0 -> vaplan0 (inside the container)
       -> container LAN (DHCP, DNS, firewall)
       -> container NAT -> container WAN -> internet
```

`ap0` is enslaved to a host bridge, `vap-br0`, which carries no address and has IPv6
disabled so it never configures itself from the container's advertisements. A veth pair
joins the bridge on the host side; the peer moves into the container's network namespace as
`vaplan0`. When the container is OpenWrt, VirtualAP provisions it over `uci`: a static
`vaplan` interface on the configured gateway, an IPv6 /64 from the router's ULA pinned to
the IPv4 subnet's number so netifd cannot renumber it, a DHCP pool with router
advertisements, and a reload of the iptables firewall, or a MASQUERADE rule when the image
has no firewall package. Any other container receives `vaplan0` and configures it itself.

A fresh veth has a new random MAC every start, and Windows keys its network profiles on the
gateway's MAC, so the app can pin one: a random MAC kept across starts, or the phone's own
Wi-Fi MAC.

## The TTL fix

With `-T 1`, forwarded packets leave with TTL 64 (hop limit 64 for IPv6), so to the
operator they look like the phone's own. The rules live in a mangle chain of their own,
`vap_ttl`, matched on the clients' source subnet in `FORWARD`, which only sees forwarded
packets after the kernel took its hop off. The backend probes for the kernel's TTL target
(`CONFIG_NETFILTER_XT_TARGET_HL`) by adding the real rule in a chain nothing jumps to; the
app greys the option out when the probe fails.

## The radio

Most phone chips can only beacon on the channel the station is connected to, so the
backend follows the station's channel when it fits the band. The station's channel comes
from the `freq:` line of `iw dev wlan0 link`, because Samsung's Broadcom driver omits the
channel line from `iw dev info`. A station on a DFS channel (52 to 144) falls back to
2.4 GHz: phone chips cannot beacon there. With no station, 5 GHz starts on channel 36 and
2.4 GHz on channel 6.

Samsung's Broadcom FullMAC firmware boots in world regdomain "99", and the framework
sometimes never pushes a real country, so every 5 GHz channel stays NO-IR and `hostapd`
dies about five seconds in, after the liveness check. The wiphy is self-managed, so
`iw reg set` and `hostapd`'s `country_code` are ignored; the only lever is the driver's
private `COUNTRY` command over the supplicant's control socket, sent by `hostapd_cli` with
the country the framework reports (`ensure_5g_ir`). It fires only when the phy reports the
channel as NO-IR, and if the channel is still NO-IR afterwards the AP falls back to
2.4 GHz. `hostapd.conf` keeps `country_code=US`.

Width follows what the chip and channel can do: 40 MHz on 2.4 GHz only when asked for by
name, up to 80 MHz on 5 GHz when the phy lists VHT and the channel sits in an 80 MHz block.
HT40 and VHT need `hostapd` to skip its 20/40 MHz coexistence scan, which FullMAC chips
cannot run on an AP interface; the `no_pri_sec_switch` option that does that is a patch in
our `hostapd` fork, and the build asserts the patch is present. If `hostapd` refuses 40 MHz
on 2.4 GHz, the backend retries at 20.

MediaTek's gen4m driver has no `dump_station`, so the client count comes from `hostapd`'s
own station list through `hostapd_cli`, which needs `-s` pointed at a directory that exists
because Android has no `/tmp`.

## On the phone

The script itself lives in the app's private files directory and is extracted from the APK
on every launch, so it can never be stale. The rest is under `/data/local/virtualap`:

- `bin/`: `hostapd`, `hostapd_cli`, `iw`, `dnsmasq` and `busybox`, fully static musl
  binaries for the phone's arch. The script's coreutils all go through that busybox,
  because Android's toybox differs between phones; `ip` and `iptables` are Android's own.
- `ap.conf`: the last flags the app passed, single-quote escaped and sourced on the next
  run, so a flag that is omitted keeps its saved value.
- `run.state`: what the running AP needs torn down, the gateway, the upstream, the IPv6
  interface and the sysctl values to restore. `status` keys "running" off it and the
  `hostapd` pid file; `stop` removes it before it exits.
- `run/`: the pid files, the generated `hostapd.conf` and `dnsmasq.conf`, the leases, the
  IPv6 client list and `hostapd`'s control socket.
- `logs/`: `ap.log`, a timestamped copy of every line the app showed, truncated on each
  start, plus `hostapd.log` and `dnsmasq.log`, because both daemons would otherwise take
  their failure reason with them.

The app deploys `bin/` on first run and again whenever an APK ships different binaries: the
build writes `PAYLOAD_VERSION`, a hash of each arch's binaries, next to them, and the app
compares it with the one it last deployed. There is no Magisk module and no reboot.

## Building

The binaries are built from the git submodules under `externals/`, our own forks of
`hostapd`, `iw` and `dnsmasq`, by `scripts/build-static.sh`. Each arch builds in an Alpine
3.23 container of that arch, under QEMU on an x86 host, with `-static -no-pie` so the
result is a standalone ELF with no interpreter; busybox is Alpine's own static build. The
`dnsmasq` fork drops the `getpwnam("root")` privilege drop, since Android has no root entry
in `/etc/passwd` and static musl will not synthesize one. The output lands in
`backend/<arch>/`, which is gitignored, and the `prepareAssets` gradle task stages it and
the script into the APK's assets, failing when either arch is missing.

CI builds each arch on an ARM runner, cached on the `externals/` and `scripts/` trees, then
builds the APK with JDK 17. A pushed `v*` tag that matches `VERSION` publishes a release
with the APK, a `SHA256SUMS` file and a changelog of every commit since the previous tag;
the tag must match `VERSION` and the signing key must be present, or the run stops. CI
runs no formatter and no linter.
