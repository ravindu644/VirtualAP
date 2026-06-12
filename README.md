# VirtualAP

VirtualAP is a software utility designed to configure a virtual access point on rooted Android devices.

> [!NOTE]
> This application is a proof of concept. The front-end user interface is developed with the assistance of an AI companion. The backend routing engine is derived from the pre-existing [Ubuntu-Chroot](https://github.com/ravindu644/Ubuntu-Chroot) project.

## Navigation

* [System Requirements](#system-requirements)
* [Features](#features)
* [Repository Layout](#repository-layout)
* [Build Instructions](#build-instructions)
* [Android Application Lifecycle](#android-application-lifecycle)
* [Routing and Architecture](#routing-and-architecture)
* [License](#license)

## System Requirements

* **Root Access**: Root permissions are required to perform network routing operations, control iptables, and manage virtual interfaces.
* **Architecture**: Aarch64 (ARM64-v8a) CPU architecture.
* **Android Version**: Android 8.0 (SDK 26) or higher.

## Features

* **Configurable Gateway IP**: Unlike the default Android hotspot, the gateway address remains static. This ensures that port forwards, bookmarks, and SSH configurations remain valid.
* **Selectable Upstream Interface**: Direct traffic through Mobile Data, Wi-Fi, Ethernet, or virtual interfaces like WireGuard tun0 to tunnel all connected clients automatically.
* **Wi-Fi Repeater Mode**: Connect your phone to any Wi-Fi network and share it as a hotspot simultaneously. The phone acts as a wireless repeater, allowing other devices to access the network without additional hardware.
* **VPN Hotspot**: Set a VPN tunnel interface (such as WireGuard tun0) as the upstream. All devices connected to the hotspot are automatically routed through the VPN, turning your phone into a portable VPN access point.
* **Automatic Upstream Detection**: Reads the default network routing rules from the Android netd system to identify the active internet connection.
* **DHCP and DNS Services**: Powered by dnsmasq inside the chroot environment to serve local clients.
* **Same-Channel Concurrency**: The access point dynamically follows the Wi-Fi station channel. This addresses stability issues with 5GHz connectivity.
* **Minimal Footprint**: Relies on a 4.4MB Alpine rootfs containing only hostapd, dnsmasq, and iw. Firewall and routing tasks leverage the native Android iptables and ip tools.

## Repository Layout

```
VirtualAP/
├── Android/           ← Companion application (Root validation, installer, AP control)
├── backend/           ← Shell backend: vap.sh (Chroot controller) and start-ap (AP engine)
└── rootfs-builder/    ← Dockerfile and build scripts for the Alpine rootfs tarball
```

## Build Instructions

### 1. Build the Alpine rootfs
The rootfs build process requires Docker. It utilizes binfmt to cross-compile for arm64:
```bash
./rootfs-builder/build_rootfs.sh
```

### 2. Build the Android APK
Compile the Android application using Gradle:
```bash
cd Android && ./gradlew assembleRelease
```
The Gradle `prepareAssets` task executes automatically before compilation to copy `backend/vap.sh`, `backend/start-ap`, `backend/bin/busybox`, and the compiled rootfs tarball into the application assets.

## Android Application Lifecycle

Upon first execution, the application validates root privileges and deploys the backend environment to `/data/local/virtualap/`. The installation process extracts the bundled Alpine rootfs, copy the control scripts, and configures file permissions. The application operates independently without requiring Magisk modules or system reboots.

## Routing and Architecture

```
Client Outbound:
client ➔ ap0 (Gateway IP, hostapd)
       ➔ ip rule pref 7010: from all iif ap0 lookup <upstream table>
       ➔ MASQUERADE (-s <subnet> ! -d <subnet>)
       ➔ Internet or VPN tunnel

Client Inbound / Replies:
replies ➔ ip rule pref 7000: to <subnet> lookup main ➔ ap0
```

The routing rules are configured with high priority (7000 and 7010) to sit above the Android netd rule range. This prevents VPN configuration overrides from hijacking client traffic and bypasses the native unreachable guard rules.

### Container Port Forwarding Integration

To support accessing containerized services (such as those running inside Droidspaces) from devices connected to the VirtualAP hotspot, the routing engine mirrors the access point subnet into Android's default local network routing table (table 97):

```
Client ➔ Gateway IP (Port Forwarded Port)
       ➔ DNAT (host port redirected to container IP 172.28.0.0/16)
       ➔ Container responds to Client IP (<subnet>)
       ➔ Android Rule 6095 matches: from 172.28.0.0/16 lookup local_network (table 97)
       ➔ Route lookup matches mirrored subnet route: <subnet> dev ap0
       ➔ Packet successfully routed back to Client via ap0
```

Without mirroring the route to table 97, Android's policy routing for the container subnet would fall through to the physical WAN interface table, causing reply packets to leak to the external WAN and breaking the port-forwarding connection.

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for details.
