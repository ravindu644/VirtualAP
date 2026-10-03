#!/usr/bin/env bash
#
# Build fully-static aarch64 + armhf binaries (hostapd, iw, dnsmasq, busybox)
# for VirtualAP.
#
# A static musl binary carries its own libc/libnl, so it runs directly on
# Android (any phone, any bionic) with no chroot. Each arch is built in an Alpine
# container of that arch under qemu emulation - reproducible, no host toolchain
# needed beyond Docker.
#
# Sources are vendored as git submodules under externals/ (our own forks of
# hostap/iw/dnsmasq, so the build survives upstream going away).
#
# Usage:  ./scripts/build-static.sh [aarch64] [armhf]   (no argument: both)
# Output: staged into backend/{aarch64,armhf}/ (also left in scripts/out/<arch>/)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
IMAGE="alpine:3.23"

# arch label -> docker platform (x86 intentionally unsupported). CI builds one
# arch per runner, so the arches to build can be named on the command line.
[ $# -eq 0 ] && set -- aarch64 armhf
ARCHES=""
for want in "$@"; do
    case "$want" in
        aarch64) ARCHES="$ARCHES aarch64:linux/arm64" ;;
        armhf)   ARCHES="$ARCHES armhf:linux/arm/v7" ;;
        *) echo "unknown arch '$want' (expected: aarch64, armhf)" >&2; exit 1 ;;
    esac
done

# Fetch/refresh the vendored sources on the host (uses the https URLs in
# .gitmodules - no SSH keys needed).
echo "[*] Initializing source submodules..."
git -C "$REPO" submodule update --init externals/hostapd externals/iw externals/dnsmasq

for pair in $ARCHES; do
    label="${pair%%:*}"; platform="${pair##*:}"
    echo "[*] Building $label ($platform) in $IMAGE ..."
    # Pull the image for THIS platform right before running. alpine:3.23 is a
    # single local tag, so it points at whichever arch was pulled last and
    # `docker run --platform` silently reuses that wrong-arch image. Re-pulling
    # per iteration keeps the local tag in sync with $platform.
    docker pull --quiet --platform "$platform" "$IMAGE" >/dev/null
    # Register qemu only if this platform cannot run as is. An ARM host runs
    # these containers natively and never needs it.
    if ! docker run --rm --platform "$platform" "$IMAGE" true 2>/dev/null; then
        echo "[*] Registering qemu binfmt handlers (arm64, arm)..."
        docker run --rm --privileged tonistiigi/binfmt:latest --install arm64,arm >/dev/null
    fi
    # ARCH_LABEL is passed explicitly so the container never has to guess its arch
    # from `uname -m` (unreliable under emulation). :z relabels the bind mount for
    # SELinux (Fedora/RHEL); externals/ is mounted read-only and copied out before
    # building.
    docker run --rm --platform "$platform" \
        -e ARCH_LABEL="$label" \
        -v "$HERE":/work:z \
        -v "$REPO/externals":/externals:ro,z \
        "$IMAGE" sh /work/build-in-container.sh

    # Stage into backend/<arch> (where prepareAssets / the app expect them).
    # Output is root-owned (built in-container); cp reads it and writes
    # user-owned copies.
    echo "[*] Staging into backend/$label ..."
    mkdir -p "$REPO/backend/$label"
    cp "$HERE/out/$label/"* "$REPO/backend/$label/"
done

echo
echo "[*] Done:"
for pair in $ARCHES; do
    label="${pair%%:*}"
    echo "--- backend/$label ---"; ls -lh "$REPO/backend/$label"
done