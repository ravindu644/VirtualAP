# How VirtualAP works

VirtualAP creates a virtual AP interface, `ap0`, on the phone's Wi-Fi chip, runs `hostapd`
on it, and then works in one of two modes. Everything below is done by `virtualap`, a
static C binary running as root, built from `src/`; the app only calls its commands
(`start`, `stop`, `status`, `interfaces`, `caps`). It talks to the kernel directly over
netlink (rtnetlink for links, addresses, routes and rules; nl80211 for the radio) and over
the raw `ip_tables` socket. It spawns `hostapd` and `dnsmasq`, `iptables` for the TTL fix
only, and, in managed mode, `droidspaces` to find the container and run the OpenWrt setup
in it. Which containers are running, the list the app offers for managed mode, the app
asks Droidspaces itself (`droidspaces show --json`, or `show --format` on a release from
before `--json`).

## Routed mode ("Interface" in the app)

VirtualAP owns the hotspot's whole network layer. It gives `ap0` the gateway address
(`192.168.42.1/24` unless you chose another), serves DHCP and DNS with its own `dnsmasq`
(leases from `.10` to `.50` for 12 hours, DNS forwarded to the servers you typed, or to
`1.1.1.1` and `8.8.8.8` by default), and NATs the clients out through the upstream.

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
default-network rule, which always points at the network that has internet right now. It
is read from the kernel's rule list, the form Android 15 writes it in (a uid range over
every uid) included. The firewall rules go in through the raw `ip_tables` socket under
Android's xtables lock, the same lock netd and `iptables -w` hold, so neither side ever
overwrites the other's change.

The AP subnet's route is also mirrored into Android's `local_network` table (97). Without
that, a reply from a Droidspaces container (source `172.28.0.0/16`) to a hotspot client
would fall through to the WAN table and leave by the physical uplink instead of `ap0`, so
clients could not reach services inside containers.

### IPv6

Clients get IPv6 only when the upstream has a global address; a Wi-Fi network that hands
out only ULAs leaves the AP IPv4-only. There are two ways to give it, tried in this order:

- NAT66, Droidspaces' approach, when the kernel has `ip6table_nat` with MASQUERADE and the
  upstream's table has an IPv6 default route. `ap0` takes `::1` in a private /64 numbered
  after the IPv4 subnet (`fd76:6170:42::/64` for `192.168.42.1`), `dnsmasq` advertises it
  with itself as the DNS server, and the clients are masqueraded out of the upstream. It
  survives a change of upstream or prefix untouched.
- The upstream's own /64, the way Android's tethering does it, on kernels without IPv6 NAT
  (stock GKI ships none) when the upstream holds a global /64. `ap0` takes an EUI-64
  address in it and `dnsmasq` advertises the prefix off-link, so a client sends everything
  to the gateway. Each client address is routed as a /128 to `ap0` and answered for on the
  upstream with an NDP proxy entry. The monitor finds the addresses by pinging `ff02::1`
  from `ap0` every 3 seconds, the only thing that reveals them; it accepts only replies to
  its own echo, never for an address that is the phone's own or a neighbour on the
  upstream. Rule 7001 blackholes an upstream packet for an address in the /64 no client
  holds, which on cellular would otherwise bounce until its hop limit died. Without the
  monitor the prefix is not advertised.

Both add rule 7002, which gives the phone's own packets to a client's link-local address a
route: Android files an interface's own routes in a table no rule reads, and `dnsmasq`'s
answer to a router solicitation is one of those packets. IPv6 is best effort: when a step
fails, the AP stays IPv4-only.

## The monitor

A successful start leaves `virtualap` running as a monitor (`run/monitor.pid`), one process
with no threads, after Droidspaces' route monitor and USB-Arsenal's tether daemon. It
listens for the kernel's link, address, route and rule events, looks 200 ms after the last
event of a burst, and looks every 3 seconds regardless, because netd rewrites `iptables`
without an event. Each look:

- re-resolves the upstream, and when a new one has been seen twice in a row (a DHCP
  renewal drops the default route for a moment) moves rule 7010 and `run.state` to it, so
  a Wi-Fi to mobile handoff does not leave the clients without internet;
- puts back any rule or firewall entry netd flushed, and `ip_forward`, only when missing,
  so its own writes never wake it again;
- redoes IPv6, `dnsmasq`'s advertisement and the TTL fix with it, when the upstream's IPv6
  changed: a new prefix, another interface, IPv6 appearing or gone.

In managed mode it watches the veth instead and plugs a new one into the container when
the container restarts. It exits when the AP is gone; `stop` kills it before anything else.

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
joins the bridge on the host side; the other end is created inside the container's network
namespace, already named `vaplan0` and with its MAC, in one request. A link created on the
host and renamed after the move shows up under its old name first on kernels before 5.x,
and OpenWrt's netifd does not look at it again. When the container is OpenWrt, VirtualAP
provisions it with one `uci` script run through `droidspaces run`: a static `vaplan`
interface on the configured gateway, an IPv6 /64 from the router's ULA pinned to the IPv4
subnet's number so netifd cannot renumber it, a DHCP pool with router advertisements, and a
reload of the iptables firewall, or a MASQUERADE rule when the image has no firewall
package. Any other container receives `vaplan0` and configures it itself.

The veth pair runs at MTU 1508 while `ap0` and the clients stay at 1500. Samsung's Broadcom
firmware hands up about a quarter of received frames with the 8-byte CCMP MIC still on the
end, and a bridge or a veth silently frees any frame longer than its MTU plus 18 bytes, so
at 1500 a full-size frame from a client was lost one time in four and uploads into the
container crawled at about 1 MB/s. The IP stack inside the container trims the MIC, and the
RA pins the clients' IPv6 MTU to 1500 with `ra_mtu`, because odhcpd otherwise advertises the
port's 1508.

Both ends of the veth get RPS, on every CPU but CPU0. A veth queues each packet on the CPU
that sent it, and one flow arrives from several, because the Wi-Fi driver spreads its
receive work over cores and TCP sends from whichever core took the last ACK. Without RPS
the far end sees the flow out of order, and TCP retransmits data that was never lost.
`vaplan0`'s mask is written by a child that enters the container's network namespace and
mounts a `/sys` of its own there, never through `/proc/<pid>/root`, whose symlinks the
container chooses. The kernel keeps only the CPUs online when the mask is written, so cores
parked at that moment stay out until the AP restarts.

A fresh veth has a new random MAC every start, and Windows keys its network profiles on the
gateway's MAC, so the app can pin one: a random MAC kept across starts, or the phone's own
Wi-Fi MAC. When the container restarts, its network namespace takes the veth with it; the
monitor sees the host end gone and plugs a new one in, and the OpenWrt config, which
survived the restart, picks it up.

## The TTL fix

With `-T 1`, forwarded packets leave with TTL 64 (hop limit 64 for IPv6), so to the
operator they look like the phone's own. The rules live in a mangle chain of their own,
`vap_ttl`, matched on the clients' source subnet in `FORWARD`, which only sees forwarded
packets after the kernel took its hop off. The backend probes for the kernel's TTL target
(`CONFIG_NETFILTER_XT_TARGET_HL`) by adding the real rule in a chain nothing jumps to; the
app greys the option out when the probe fails. These are the only rules still written by
the `iptables` binary.

## The radio

Most phone chips can only beacon on the channel the station is connected to, so the
backend follows the station's channel when it fits the band. The station's channel comes
from the frequency of the BSS it is associated with in the scan results, which is what
`iw dev wlan0 link` reads, because Samsung's Broadcom driver omits the channel from the
interface info. A station on a DFS channel (52 to 144) falls back to 2.4 GHz: phone chips
cannot beacon there. A station on another band, 6 GHz included, leaves the AP its own
default. With no station, 5 GHz starts on channel 36 and 2.4 GHz on channel 6.

Samsung's Broadcom FullMAC firmware boots in world regdomain "99", and the framework
sometimes never pushes a real country, so every 5 GHz channel stays NO-IR and `hostapd`
dies about five seconds in, after the liveness check. The wiphy is self-managed, so
`iw reg set` and `hostapd`'s `country_code` are ignored; the only lever is the driver's
private `COUNTRY` command over the supplicant's control socket, sent with the country the
framework reports (`ensure_5g_ir`). It fires only when the phy reports the channel as
NO-IR, and if the channel is still NO-IR afterwards the AP falls back to 2.4 GHz.
`hostapd.conf` keeps `country_code=US`.

Width follows what the chip and channel can do, from the capabilities of that band: 40 MHz
on 2.4 GHz only when asked for by name, up to 80 MHz on 5 GHz when the band lists VHT and
the channel sits in an 80 MHz block. HT40 and VHT need `hostapd` to skip its 20/40 MHz
coexistence scan, which FullMAC chips cannot run on an AP interface; the
`no_pri_sec_switch` option that does that is a patch in our `hostapd` fork, and the build
asserts the patch is present. If `hostapd` refuses 40 MHz on 2.4 GHz, the backend retries
at 20.

MediaTek's gen4m driver has no `dump_station`, so the client count comes from `hostapd`'s
own station list over its control socket, bound to an abstract address since Android has
no `/tmp`, with a one second timeout so a hung `hostapd` cannot stall `status`.

## On the phone

Everything is under `/data/local/virtualap`:

- `bin/`: `virtualap`, `hostapd` and `dnsmasq`, fully static musl binaries for the phone's
  arch. `iptables` is Android's own.
- `ap.conf`: the last flags the app passed, as `KEY='value'` lines, so a flag that is
  omitted keeps its saved value. It holds the Wi-Fi password, so it is readable by root
  only.
- `run.state`: what the running AP needs torn down, the gateway, the upstream, the IPv6
  mode and interface and the sysctl values to restore. `status` keys "running" off it and
  the `hostapd` pid file; `stop` removes it before it exits.
- `run/`: the pid files (`hostapd`, `dnsmasq`, the monitor), the generated `hostapd.conf`
  and `dnsmasq.conf`, the leases, the IPv6 client list, `hostapd`'s control socket and the
  lock that keeps two starts or stops from running at once.
- `logs/`: `ap.log`, a timestamped copy of every line the app showed and of what the
  monitor did, truncated on each start, plus `hostapd.log` and `dnsmasq.log`, because both
  daemons would otherwise take their failure reason with them.

A pid file is only ever acted on when the process it names runs the expected program, so a
pid reused after a reboot is never signalled. The app deploys `bin/` on first run and again
whenever an APK ships different binaries: the build writes `PAYLOAD_VERSION`, a hash of
each arch's binaries, next to them, and the app compares it with the one it last deployed.
There is no Magisk module and no reboot.

## Building

`hostapd` and `dnsmasq` are built from the git submodules under `externals/`, our own forks,
by `scripts/build-static.sh`. Each arch builds in an Alpine 3.23 container of that arch,
under QEMU on an x86 host, with `-static -no-pie` so the result is a standalone ELF with no
interpreter. The `dnsmasq` fork drops the `getpwnam("root")` privilege drop, since Android
has no root entry in `/etc/passwd` and static musl will not synthesize one. `virtualap`
links nothing but libc and needs no container: the root `Makefile` builds it the
Droidspaces way, `make aarch64 armhf` with the musl-cross gcc in
`~/toolchains/<triplet>-cross` (or `MUSL_CROSS`). The output lands in `backend/<arch>/`,
which is gitignored, and the `prepareAssets` gradle task stages the three binaries into the
APK's assets, failing when any is missing.

`src/` has one module per concern, its header under `src/include/` beside the others of its
layer: `core`, `cli`, `net` (rtnetlink, the `ip_tables` engine), `radio` (nl80211, the
control socket, channel policy), `services` (`hostapd`, `dnsmasq`), `mode` (routed,
managed, IPv6), `monitor` and `cmd`. `make check` runs the asserts in `tests/`, the netlink
ones against the kernel inside a throwaway user and network namespace, so neither root nor
a phone is needed; built for the phone and run there as root, they also exercise the real
`ip_tables`.

CI builds `hostapd` and `dnsmasq` for each arch on an ARM runner, cached on the
`externals/` and `scripts/` trees. The APK job then fetches Droidspaces' published musl
toolchains (checked against their sha256 and cached on their release URL), runs
`make check` and `make aarch64 armhf` with warnings as errors, and builds the APK with
JDK 17. A pushed `v*` tag that matches `VERSION` publishes a release with the APK, a
`SHA256SUMS` file and a changelog of every commit since the previous tag; the tag must
match `VERSION` and the signing key must be present, or the run stops. CI runs no
formatter and no linter.
