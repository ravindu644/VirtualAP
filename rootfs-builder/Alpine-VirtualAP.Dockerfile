# VirtualAP minimal Alpine payload
# Only what the AP engine truly needs lives here:
#   hostapd  - the AP daemon (nl80211)
#   dnsmasq  - DHCP + DNS for AP clients
#   iw       - virtual interface management (many phones ship no iw binary,
#              so ALL wireless ops run through this one)
#   busybox-static - extracted by build_zip.sh and shipped host-side for
#              unshare/nsenter (Android has neither)
# Routing/firewall (ip, iptables) intentionally NOT included - those run on
# the Android side via /system/bin.
ARG TARGETPLATFORM
FROM alpine:3.23 AS customizer

RUN apk update && apk upgrade && \
    apk add --no-cache \
    hostapd \
    dnsmasq \
    iw \
    busybox-static && \
    rm -rf /var/cache/apk/*

# Stage 2: flatten to a clean export
FROM scratch AS export
COPY --from=customizer / /
