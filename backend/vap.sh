#!/system/bin/sh

# VirtualAP chroot core
# Copyright (c) 2026 ravindu644
#
# Slim descendant of Ubuntu-Chroot's "Advanced Chroot Manager": same
# namespace-holder pattern (probe unshare flags per-kernel, park a sleeping
# busybox inside the namespaces, nsenter for every later op), but the payload
# is a tiny read-mostly Alpine directory - no sparse image, no users, no
# desktop plumbing. The network namespace is deliberately NOT unshared:
# ap0/hostapd must live in the host netns.

BASE_DIR="/data/local/virtualap"
CHROOT_PATH="${BASE_DIR}/rootfs"
HOLDER_PID_FILE="${BASE_DIR}/holder.pid"
MOUNTED_FILE="${BASE_DIR}/mount.points"
SCRIPT_NAME="$(basename "$0")"
SILENT=0

# --- Busybox resolution ---
if [ -x "$BASE_DIR/bin/busybox" ]; then
    export BUSYBOX="$BASE_DIR/bin/busybox"
elif command -v busybox >/dev/null 2>&1; then
    export BUSYBOX="busybox"
else
    echo "[ERROR] No busybox found (checked $BASE_DIR/bin/busybox and PATH). Aborting." >&2
    exit 1
fi

# Must return 0 even when silent: log is often the last statement of a
# function, and `[ ... ] && echo` returns 1 under -s, failing the caller.
log() { [ "$SILENT" -eq 0 ] && echo "[INFO] $1"; return 0; }
warn() { [ "$SILENT" -eq 0 ] && echo "[WARN] $1"; return 0; }
error() { echo "[ERROR] $1"; }

usage() {
    echo "Usage: $SCRIPT_NAME [command]"
    echo ""
    echo "Commands:"
    echo "  start         Mount and prepare the VirtualAP environment."
    echo "  stop          Kill chroot processes and unmount everything."
    echo "  status        Show the current status."
    echo "  run <command> Execute a command inside the chroot."
    echo ""
    echo "Options:"
    echo "  -s            Silent mode."
    exit 1
}

# --- Namespace handling (proven pattern from Ubuntu-Chroot) ---

_get_ns_flags() {
    # Translate saved long flags (--mount) to the short flags (-m) that
    # busybox nsenter requires.
    local flags_file="$HOLDER_PID_FILE.flags"
    if [ ! -f "$flags_file" ]; then
        echo "-m"; return
    fi

    local long_flags short_flags
    long_flags=$(cat "$flags_file")
    [ -z "$long_flags" ] && { echo "-m"; return; }

    for flag in $long_flags; do
        case "$flag" in
            --mount) short_flags="$short_flags -m" ;;
            --uts)   short_flags="$short_flags -u" ;;
            --ipc)   short_flags="$short_flags -i" ;;
            --pid)   short_flags="$short_flags -p" ;;
        esac
    done

    [ -z "$short_flags" ] && { echo "-m"; return; }
    echo "$short_flags"
}

_execute_in_ns() {
    local holder_pid
    if [ -f "$HOLDER_PID_FILE" ] && kill -0 "$(cat "$HOLDER_PID_FILE")" 2>/dev/null; then
        holder_pid=$(cat "$HOLDER_PID_FILE")
        local ns_flags
        ns_flags=$(_get_ns_flags)
        "${BUSYBOX}" nsenter --target "$holder_pid" $ns_flags -- "$@"
    else
        "$@"
    fi
}

run_in_ns() {
    _execute_in_ns "$@"
}

run_in_chroot() {
    _execute_in_ns chroot "$CHROOT_PATH" /bin/sh -c "PATH=/usr/sbin:/usr/bin:/sbin:/bin; export TMPDIR=/tmp; $*"
}

is_running() {
    [ -f "$HOLDER_PID_FILE" ] && kill -0 "$(cat "$HOLDER_PID_FILE")" 2>/dev/null
}

create_namespace() {
    local pid_file="$1"
    local unshare_flags=""

    # Probe each namespace individually - kernels vary. --net is deliberately
    # absent: the AP interface must stay in the host network namespace.
    for ns_flag in --pid --mount --uts --ipc; do
        if "${BUSYBOX}" unshare "$ns_flag" true 2>/dev/null; then
            unshare_flags="$unshare_flags $ns_flag"
        fi
    done

    if ! echo "$unshare_flags" | grep -q -- "--mount"; then
        error "Mount namespace not supported - cannot create chroot"
        return 1
    fi

    log "using flags:$unshare_flags"
    echo "$unshare_flags" > "${pid_file}.flags"

    # Subshell inside the new namespaces backgrounds a sleeper and reports
    # its PID - guaranteed to be a process inside the namespaces.
    "${BUSYBOX}" unshare $unshare_flags sh -c '"$2" sleep infinity & echo $! > "$1"' -- "$pid_file" "$BUSYBOX"

    local attempts=0
    while [ $attempts -lt 10 ]; do
        if [ -f "$pid_file" ] && kill -0 "$(cat "$pid_file")" 2>/dev/null; then
            return 0
        fi
        sleep 0.1
        attempts=$((attempts + 1))
    done

    error "Failed to create and capture namespace holder PID."
    rm -f "$pid_file" "${pid_file}.flags"
    return 1
}

# --- Mount helpers ---

advanced_mount() {
    local src="$1" tgt="$2" type="$3" opts="$4"

    [ ! -d "$tgt" ] && run_in_ns mkdir -p "$tgt" 2>/dev/null

    if [ "$type" = "bind" ]; then
        [ -e "$src" ] || { warn "Source for bind mount does not exist: $src"; return 1; }
        run_in_ns mount --bind "$src" "$tgt"
    else
        run_in_ns mount -t "$type" $opts "$type" "$tgt"
    fi

    if [ $? -eq 0 ]; then
        log "Mounted $src -> $tgt ($type)"
        echo "$tgt" >> "$MOUNTED_FILE"
    else
        error "Failed to mount $src"
    fi
}

write_resolv_conf() {
    # dnsmasq forwards client DNS queries through this file. net.dns* props
    # are often empty on modern Android - public resolvers are the fallback.
    local dns_lines=""
    for i in 1 2 3 4; do
        local dns
        dns=$(getprop net.dns${i} 2>/dev/null)
        [ -n "$dns" ] && dns_lines="${dns_lines}nameserver ${dns}\n"
    done
    [ -z "$dns_lines" ] && dns_lines="nameserver 1.1.1.1\nnameserver 8.8.8.8\n"
    printf "$dns_lines" > "$CHROOT_PATH/etc/resolv.conf"
}

# --- Core actions ---

start_env() {
    log "Setting up VirtualAP environment..."

    (setenforce 0 && log "SELinux set to permissive mode") || warn "Failed to set SELinux to permissive mode"

    if is_running; then
        log "Namespace holder already running."
    else
        log "Creating new isolated namespace..."
        create_namespace "$HOLDER_PID_FILE" || return 1
        sleep 0.5
        log "Running in isolated namespace (PID: $(cat "$HOLDER_PID_FILE"))"
    fi

    [ -d "$CHROOT_PATH" ] || { error "Rootfs not found at $CHROOT_PATH"; exit 1; }

    rm -f "$MOUNTED_FILE"

    log "Setting up system mounts..."
    advanced_mount "proc" "$CHROOT_PATH/proc" "proc" "-o rw,nosuid,nodev,noexec,relatime"
    advanced_mount "sysfs" "$CHROOT_PATH/sys" "sysfs" "-o rw,nosuid,nodev,noexec,relatime"

    if grep -q devtmpfs /proc/filesystems; then
        advanced_mount "devtmpfs" "$CHROOT_PATH/dev" "devtmpfs" "-o mode=755"
    else
        advanced_mount "/dev" "$CHROOT_PATH/dev" "bind"
    fi

    advanced_mount "tmpfs" "$CHROOT_PATH/tmp" "tmpfs" "-o rw,nosuid,nodev,relatime,size=32M"
    advanced_mount "tmpfs" "$CHROOT_PATH/run" "tmpfs" "-o rw,nosuid,nodev,relatime,size=16M"

    write_resolv_conf

    log "VirtualAP environment ready."
}

kill_chroot_processes() {
    log "Killing chroot processes..."

    # Every process whose root is inside the chroot (hostapd, dnsmasq),
    # found in a single ls pass over /proc/*/root. Deliberately NOT lsof:
    # a full lsof scan walks every fd of every process and takes 10-30s on
    # Android - it stalled the app's start/stop for half a minute. A
    # per-PID readlink loop is no good either (~700 forks).
    local pids
    pids=$(ls -l /proc/[0-9]*/root 2>/dev/null \
        | grep -F " -> $CHROOT_PATH" \
        | sed -n 's|.*/proc/\([0-9]*\)/root.*|\1|p')
    if [ -n "$pids" ]; then
        kill -9 $pids 2>/dev/null
        log "Killed chroot processes."
    fi
}

umount_env() {
    if [ -f "$MOUNTED_FILE" ]; then
        log "Unmounting filesystems..."
        sort -r "$MOUNTED_FILE" | while read -r mount_point; do
            case "$mount_point" in
                "$CHROOT_PATH"/sys*) run_in_ns umount -l "$mount_point" 2>/dev/null ;;
                *) run_in_ns umount "$mount_point" 2>/dev/null || run_in_ns umount -l "$mount_point" 2>/dev/null ;;
            esac
        done
        rm -f "$MOUNTED_FILE"
        log "All mounts cleaned."
    fi
}

stop_env() {
    log "Stopping VirtualAP environment..."

    kill_chroot_processes
    umount_env

    if [ -f "$HOLDER_PID_FILE" ]; then
        local holder_pid
        holder_pid=$(cat "$HOLDER_PID_FILE")
        if kill -0 "$holder_pid" 2>/dev/null; then
            # SIGKILL, not SIGTERM: as init of the PID namespace the holder
            # ignores every signal it has no handler for (kernel rule), and
            # busybox sleep handles nothing - plain kill is silently ignored.
            # Killing the ns-init also makes the kernel reap every process in
            # the namespace, including the zombies 'sleep' never wait()ed for.
            kill -9 "$holder_pid" 2>/dev/null && log "Killed namespace holder." || warn "Failed to kill holder."
        fi
        rm -f "$HOLDER_PID_FILE" "$HOLDER_PID_FILE.flags"
    fi

    log "Stopped."
}

show_status() {
    if is_running; then
        echo "Status: RUNNING"
        echo "Namespace Holder PID: $(cat "$HOLDER_PID_FILE")"
        [ -f "$HOLDER_PID_FILE.flags" ] && echo "Namespace Flags: $(cat "$HOLDER_PID_FILE.flags")"
    else
        echo "Status: STOPPED"
    fi
}

# --- Main ---
if [ "$(id -u)" -ne 0 ]; then
    error "This script must be run as root."; exit 1
fi

[ $# -eq 0 ] && usage

COMMAND=""
RUN_COMMAND=""

for arg in "$@"; do
    case "$arg" in
        start|stop|status|run) [ -z "$COMMAND" ] && COMMAND="$arg" || RUN_COMMAND="$RUN_COMMAND $arg" ;;
        -s) SILENT=1 ;;
        -h|--help) usage ;;
        *)
            if [ "$COMMAND" = "run" ]; then
                RUN_COMMAND="$RUN_COMMAND $arg"
            else
                echo "Unknown option: $arg"; usage
            fi
            ;;
    esac
done

case "$COMMAND" in
    start)
        if is_running; then log "Already running."; else start_env; fi ;;
    stop) stop_env ;;
    status) show_status ;;
    run)
        [ -z "$RUN_COMMAND" ] && { error "No command specified for run"; usage; }
        is_running || { error "Environment not running - use '$SCRIPT_NAME start' first"; exit 1; }
        run_in_chroot "$RUN_COMMAND" ;;
    *) usage ;;
esac
