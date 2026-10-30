#!/usr/bin/env bash
#
# Build the fully static aarch64 and armhf binaries (hostapd, hostapd_cli, iw,
# dnsmasq, busybox) for VirtualAP.
#
# A static musl binary carries its own libc and libnl, so it runs on any phone
# with no chroot. Each arch is built in an Alpine container of that arch, under
# qemu on an x86 host, so nothing beyond Docker is needed. The sources are the
# git submodules under externals/, our own forks.
#
# Usage:  ./scripts/build-static.sh [aarch64] [armhf]   (no argument: both)
# Output: backend/<arch>/
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
IMAGE="alpine:3.23"

# Fetches the vendored sources with the https URLs in .gitmodules, no SSH keys.
echo "[*] Initializing source submodules..."
git -C "$REPO" submodule update --init externals/hostapd externals/iw externals/dnsmasq

[ $# -eq 0 ] && set -- aarch64 armhf
for label in "$@"; do
    case "$label" in
        aarch64) platform=linux/arm64 ;;
        armhf)   platform=linux/arm/v7 ;;
        *) echo "unknown arch '$label' (expected: aarch64, armhf)" >&2; exit 1 ;;
    esac
    echo "[*] Building $label ($platform) in $IMAGE ..."
    # Pull the image for THIS platform right before running. alpine:3.23 is a
    # single local tag, so it points at whichever arch was pulled last and
    # `docker run --platform` silently reuses that wrong-arch image.
    docker pull --quiet --platform "$platform" "$IMAGE" >/dev/null
    # Register qemu only if this platform cannot run as is. An ARM host runs
    # these containers natively and never needs it.
    if ! docker run --rm --platform "$platform" "$IMAGE" true 2>/dev/null; then
        echo "[*] Registering qemu binfmt handlers (arm64, arm)..."
        docker run --rm --privileged tonistiigi/binfmt:latest --install arm64,arm >/dev/null
    fi
    # ARCH_LABEL is passed in so the container never guesses its arch from
    # `uname -m`, which lies under emulation. :z relabels the bind mounts for
    # SELinux (Fedora/RHEL). The container writes root-owned files, so it gets
    # a scratch dir and the user-owned copies in backend/<arch> are made here,
    # only once the whole build went through.
    out=$(mktemp -d)
    docker run --rm --platform "$platform" \
        -e ARCH_LABEL="$label" \
        -v "$HERE":/work:ro,z \
        -v "$REPO/externals":/externals:ro,z \
        -v "$out":/out:z \
        "$IMAGE" sh /work/build-in-container.sh
    mkdir -p "$REPO/backend/$label"
    cp "$out"/* "$REPO/backend/$label/"
    rm -rf "$out" 2>/dev/null || :   # root-owned under rootful Docker, /tmp reaps it
done
