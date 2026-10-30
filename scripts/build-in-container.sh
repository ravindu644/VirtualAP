#!/bin/sh
#
# Runs inside an Alpine container of the target arch, aarch64 or armhf (see
# build-static.sh). Produces the static binaries for VirtualAP:
#   hostapd + hostapd_cli  - AP daemon (nl80211, WPA2 and WPA3)
#   iw                     - virtual interface management
#   dnsmasq                - DHCP + DNS for AP clients
#   busybox                - reliable coreutils (Alpine's busybox-static)
#
# Sources come from the vendored git submodules, bind-mounted read-only at
# /externals/{hostapd,iw,dnsmasq} (our own forks - see externals/ and
# .gitmodules). They are copied out before building so the mount stays clean.
#
# Output -> /out, build scratch -> /tmp/src.
set -e

: "${ARCH_LABEL:?set by build-static.sh}"
echo "### Target arch: $ARCH_LABEL (uname -m: $(uname -m))"

# Sanity-check: the image's own arch must match the requested one, so a
# wrong-platform container (stale image cache) fails loudly instead of silently
# producing mislabeled binaries. Ask apk, not uname: a 32-bit arm container
# running natively on a 64-bit ARM host still reports the kernel's aarch64.
case "$ARCH_LABEL:$(apk --print-arch)" in
    aarch64:aarch64|armhf:armv7) ;;
    *) echo "ERROR: ARCH_LABEL=$ARCH_LABEL but the image is $(apk --print-arch) - wrong-platform image?"; exit 1 ;;
esac

OUT="/out"
SRC="/tmp/src"
mkdir -p "$SRC"

# Fully static, no PIE: a plain -static link on Alpine yields a static-PIE that
# still needs /lib/ld-musl-*.so.1 at runtime (absent on Android). -no-pie gives
# a true standalone ELF with no interpreter.
LDF="-static -no-pie"
CF="-Os -fno-pie"

# Copy a vendored source tree out of the read-only mount (drop the submodule
# .git pointer - the build doesn't need it). The make clean after each copy is
# for objects a host-side build left behind, which git status does not show.
vendor() {
    rm -rf "$SRC/$1"
    cp -a "/externals/$1" "$SRC/$1"
    rm -rf "$SRC/$1/.git"
}

echo "### Installing build deps"
apk add --no-cache build-base linux-headers pkgconf \
    libnl3-dev libnl3-static openssl-dev openssl-libs-static busybox-static >/dev/null

# Alpine's busybox-static is already a fully-static binary; just ship it. Used
# by the backend scripts for reliable coreutils (Android's toybox is flaky).
echo "### Staging static busybox"
cp /bin/busybox.static "$OUT/busybox"
strip "$OUT/busybox" 2>/dev/null || true

echo "### Building hostapd"
vendor hostapd
cd "$SRC/hostapd/hostapd"
# Assert the fork wires no_pri_sec_switch into the config parser. start-ap emits
# no_pri_sec_switch=1 for HT40/VHT to skip the OBSS coexistence scan, which
# FullMAC chips (Broadcom brcmfmac on the Galaxy S10) abort with -95. Stock
# hostapd has the struct field but no parser line, so an unpatched submodule
# would silently reject the option and crash on 40/80MHz. (See externals/hostapd.)
grep -q 'no_pri_sec_switch' config_file.c || {
    echo "hostapd source is missing the no_pri_sec_switch config patch - check externals/hostapd"; exit 1; }
cat > .config <<'EOF'
CONFIG_DRIVER_NL80211=y
CONFIG_LIBNL32=y
CONFIG_IEEE80211N=y
CONFIG_IEEE80211AC=y
CONFIG_IEEE80211AX=y
CONFIG_ACS=y
# WPA3-Personal (SAE) + WPA2/WPA3 transition mode. Without this, hostapd rejects
# wpa_key_mgmt=SAE at runtime (only WPA-PSK/WPA2 is understood).
CONFIG_SAE=y
# Enables the -f logfile flag so hostapd writes its output to a file. Without
# this, -f is a silent no-op and logs/hostapd.log stays empty. Needed because
# hostapd daemonizes with -B and its output would otherwise be lost.
CONFIG_DEBUG_FILE=y
EOF
make clean >/dev/null 2>&1 || true
# PKG_CONFIG --static pulls libnl-3/libnl-genl-3 + libcrypto static deps.
make -j"$(nproc)" PKG_CONFIG="pkg-config --static" \
    LDFLAGS="$LDF" EXTRA_CFLAGS="$CF" hostapd hostapd_cli
strip hostapd hostapd_cli
cp hostapd hostapd_cli "$OUT"/

echo "### Building iw"
vendor iw
cd "$SRC/iw"
make clean >/dev/null 2>&1 || true
# iw's Makefile does `LDFLAGS += $(pkg-config --libs ...)`, so LDFLAGS must come
# from the ENVIRONMENT (not a make-cmdline override, which would suppress the +=).
export LDFLAGS="$LDF"
export PKG_CONFIG="pkg-config --static"
make -j"$(nproc)" V=1 CFLAGS="$CF $(pkg-config --cflags libnl-3.0)"
unset LDFLAGS PKG_CONFIG
strip iw
cp iw "$OUT"/

echo "### Building dnsmasq"
vendor dnsmasq
cd "$SRC/dnsmasq"
# The getpwnam("root") privilege drop is patched out in our fork: Android has
# no root entry in /etc/passwd and static musl (unlike bionic) will not
# synthesize one, so stock dnsmasq dies with "unknown user or group: root". The
# patch leaves ent_pw NULL for user=root and the setuid block is guarded by it,
# so dnsmasq stays root (needed for :53 and :67). Assert the vendored source
# carries it, so a submodule bumped to an unpatched upstream fails here.
grep -q 'strcmp(daemon->username, "root")' src/dnsmasq.c || {
    echo "dnsmasq source is missing the getpwnam(root) patch - check externals/dnsmasq"; exit 1; }
make clean >/dev/null 2>&1 || true
# Default dnsmasq has no external lib deps; plain static link works.
make -j"$(nproc)" CFLAGS="$CF" LDFLAGS="$LDF"
strip src/dnsmasq
cp src/dnsmasq "$OUT"/

echo "### Results"
cd "$OUT"
for b in busybox hostapd hostapd_cli iw dnsmasq; do
    printf '%-12s ' "$b"
    file -b "$b"
done
