# VirtualAP

Turn a rooted Android phone into a real Wi-Fi router.

VirtualAP runs a proper access point on your phone's Wi-Fi chip. You pick the network
name, password, band and channel, the gateway address your devices see, and where their
traffic goes: mobile data, Wi-Fi, or a VPN.

## What you can do

- **Run OpenWrt as your hotspot's router.** Firewall zones, port forwards, traffic rules,
  bandwidth limits and the LuCI web interface, all on your phone. See managed mode below.
- **Share a VPN.** Every device on the hotspot goes through your phone's VPN. Nothing to
  install on the TV, the console or the laptop.
- **Repeat Wi-Fi.** Stay connected to a Wi-Fi network and rebroadcast it as your own.
- **Keep a fixed gateway.** The gateway address never changes, so port forwards and saved
  SSH targets keep working.
- **Tune the radio.** 2.4 or 5 GHz, channel and width, WPA2 or WPA3, hidden network name.
- **Hide tethering from your operator** with the optional TTL fix, on kernels that
  support it.

## Managed mode: OpenWrt runs your hotspot

This is what sets VirtualAP apart. Hand the hotspot to an OpenWrt container running in
[Droidspaces](https://github.com/ravindu644/Droidspaces-OSS), and OpenWrt manages every
connected device the way a real Wi-Fi router does. Open the gateway address you chose in a
browser and you are in LuCI.

With OpenWrt in charge you can:

- build a firewall with as many zones as you like
- set up port forwards and traffic rules
- limit bandwidth and speed per device
- run a guest network that cannot reach your other devices
- see every connected device, and give it a fixed address and a name
- choose the DNS servers for the whole network, and add ad blocking for every device from
  OpenWrt's package feed
- give your devices IPv6 as well as IPv4
- put your Droidspaces containers behind the same router, on the same network as your Wi-Fi
  devices or walled off from them
- install anything else OpenWrt offers with `opkg`

OpenWrt's WAN side is Droidspaces, and Droidspaces switches uplinks in real time between
mobile data, Wi-Fi and VPN tunnels. Your hotspot gets that for free:

1. In Droidspaces, set `tun0` as the first upstream interface of the OpenWrt container,
   followed by `wlan0` and `rmnet0`. Your VPN is not running yet, so `tun0` does not exist.
2. Start VirtualAP. Your hotspot's traffic leaves through Wi-Fi or mobile data.
3. Start your VPN app. Droidspaces sees `tun0` appear and moves OpenWrt onto it, so every
   device on the hotspot is behind the VPN a moment later. Nothing to restart.

This is the first native OpenWrt router for an Android hotspot: OpenWrt runs in a
container on the phone's own kernel, with no virtual machine in between, and you set it up
from an app.

Managed mode needs:

- root access
- [Droidspaces](https://github.com/ravindu644/Droidspaces-OSS) installed and working
- the OpenWrt container from the
  [official Droidspaces rootfs repository](https://github.com/Droidspaces/Droidspaces-rootfs-builder/releases),
  installed and running

## Interface mode

The simple mode, with no container and no web interface. VirtualAP hands out addresses
itself and sends your devices out through the interface you pick, or follows whichever
network Android is currently using. It is all you need to share a VPN or to repeat Wi-Fi.

## Requirements

- A rooted phone
- Android 8.0 or newer
- An ARM phone, 64-bit or 32-bit. One APK covers both.
- For managed mode: Droidspaces and its OpenWrt container (see above)
- For the TTL fix: a kernel with the netfilter TTL target (`CONFIG_NETFILTER_XT_TARGET_HL`),
  which stock kernels do not always include. The app tests for it and greys the option out
  when it is missing, and the hotspot works the same without it.

## Get started

1. Download the latest APK from [Releases](https://github.com/ravindu644/VirtualAP/releases)
   and install it.
2. Open the app and grant root. It sets up everything it needs by itself. There is no
   Magisk module to flash and no reboot.
3. Enter a network name and a password.
4. Choose the upstream: an **interface** (or Auto), or your OpenWrt **container** for
   managed mode.
5. Start the hotspot and connect your devices.

In managed mode, open `http://<your gateway address>` from a connected device to reach
LuCI.

## More

- [How it works](Documentation/How-It-Works.md): the routing, the bridge and the files on
  the phone
- [Contributing](CONTRIBUTING.md)

## License

GNU General Public License v3.0. See [LICENSE](LICENSE).

VirtualAP grew out of the
[`start-hotspot`](https://github.com/ravindu644/Ubuntu-Chroot/blob/main/tools/start-hotspot)
script from [Ubuntu-Chroot](https://github.com/ravindu644/Ubuntu-Chroot). The Android app
is developed with the help of an AI assistant.
