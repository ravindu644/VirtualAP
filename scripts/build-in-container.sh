#!/bin/sh
#
# Runs INSIDE an aarch64 Alpine container (see build-static.sh).
# Produces the static aarch64 binaries for VirtualAP:
#   hostapd + hostapd_cli  - AP daemon (nl80211, WPA2-PSK)
#   iw                     - virtual interface management
#   dnsmasq                - DHCP + DNS for AP clients
#   busybox                - reliable coreutils (Alpine's busybox-static)
#
# Sources come from the vendored git submodules, bind-mounted read-only at
# /externals/{hostapd,iw,dnsmasq} (our own forks - see externals/ and
# .gitmodules). They are copied out before building so the mount stays clean.
#
# Output -> /work/out, build scratch -> /work/.src-cache
set -e

# Arch label drives the per-arch output subdir. build-static.sh passes it via
# $ARCH_LABEL (authoritative - it knows the requested --platform). Fall back to
# `uname -m` only for standalone/manual runs; uname is unreliable under emulation.
if [ -z "${ARCH_LABEL:-}" ]; then
    case "$(uname -m)" in
        aarch64)                 ARCH_LABEL=aarch64 ;;
        armv7l|armv8l|armv7|armhf|arm)  ARCH_LABEL=armhf ;;
        *) echo "unsupported build arch: $(uname -m)"; exit 1 ;;
    esac
fi
echo "### Target arch: $ARCH_LABEL (uname -m: $(uname -m))"

# Sanity-check: the image's own arch must match the requested one, so a
# wrong-platform container (stale image cache) fails loudly instead of silently
# producing mislabeled binaries. Ask apk, not uname: a 32-bit arm container
# running natively on a 64-bit ARM host still reports the kernel's aarch64.
case "$ARCH_LABEL:$(apk --print-arch)" in
    aarch64:aarch64|armhf:armv7) ;;
    *) echo "ERROR: ARCH_LABEL=$ARCH_LABEL but the image is $(apk --print-arch) - wrong-platform image?"; exit 1 ;;
esac

OUT="/work/out/$ARCH_LABEL"
SRC="/work/.src-cache/$ARCH_LABEL"
mkdir -p "$OUT" "$SRC"

# Fully static, no PIE: a plain -static link on Alpine yields a static-PIE that
# still needs /lib/ld-musl-*.so.1 at runtime (absent on Android). -no-pie gives
# a true standalone ELF with no interpreter.
LDF="-static -no-pie"
CF="-Os -fno-pie"

# Copy a vendored source tree out of the read-only mount (drop the submodule
# .git pointer - the build doesn't need it).
vendor() {
    rm -rf "$SRC/$1"
    cp -a "/externals/$1" "$SRC/$1"
    rm -rf "$SRC/$1/.git"
}

echo "### Installing build deps"
apk add --no-cache build-base linux-headers pkgconf \
    libnl3-dev libnl3-static openssl-dev openssl-libs-static busybox-static >/dev/null

# --- busybox ---------------------------------------------------------------
# Alpine's busybox-static is already a fully-static binary; just ship it. Used
# by the backend scripts for reliable coreutils (Android's toybox is flaky).
echo "### Staging static busybox"
cp /bin/busybox.static "$OUT/busybox"
strip "$OUT/busybox" 2>/dev/null || true

# --- hostapd ---------------------------------------------------------------
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

# --- iw --------------------------------------------------------------------
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

# --- dnsmasq ---------------------------------------------------------------
echo "### Building dnsmasq"
vendor dnsmasq
cd "$SRC/dnsmasq"
# The getpwnam("root") privilege-drop is patched out directly in our fork
# (Droidspaces/dnsmasq-vap): Android has no root entry in /etc/passwd and static
# musl (unlike bionic) won't synthesize one, so stock dnsmasq dies with "unknown
# user or group: root". The in-tree patch leaves ent_pw NULL for user=root, and
# the setuid block is guarded by it, so dnsmasq just stays root (needed for
# :53/:67). The old build-time sed is kept here (disabled) for reference:
#
# sed -i \
#     's/if (daemon->username \&\& !(ent_pw = getpwnam(daemon->username)))/if (daemon->username \&\& strcmp(daemon->username, "root") != 0 \&\& !(ent_pw = getpwnam(daemon->username)))/' \
#     src/dnsmasq.c
#
# Assert the vendored source actually carries the patch (catches a submodule
# bumped to an unpatched upstream).
grep -q 'strcmp(daemon->username, "root")' src/dnsmasq.c || {
    echo "dnsmasq source is missing the getpwnam(root) patch - check externals/dnsmasq"; exit 1; }
make clean >/dev/null 2>&1 || true
# Default dnsmasq has no external lib deps; plain static link works.
make -j"$(nproc)" CFLAGS="$CF" LDFLAGS="$LDF"
strip src/dnsmasq
cp src/dnsmasq "$OUT"/

# --- Verify ----------------------------------------------------------------
echo "### Results"
cd "$OUT"
for b in busybox hostapd hostapd_cli iw dnsmasq; do
    printf '%-12s ' "$b"
    file -b "$b"
done
echo "### All binaries built into $OUT"
