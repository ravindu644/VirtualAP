#!/system/bin/sh

# VirtualAP chroot core
# Copyright (c) 2026 ravindu644
#
# Holds a tiny Alpine chroot in its own namespaces: probe unshare flags per
# kernel, park a sleeping busybox inside, nsenter for every later op. The
# network namespace is NOT unshared - ap0/hostapd must stay in the host netns.

BASE_DIR="/data/local/virtualap"
CHROOT_PATH="${BASE_DIR}/rootfs"
HOLDER_PID_FILE="${BASE_DIR}/holder.pid"
MOUNTED_FILE="${BASE_DIR}/mount.points"
SCRIPT_NAME="${0##*/}"
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

# Route external commands through the bundled busybox (Android's toybox is
# unreliable across phones). Each $VAR word-splits to "busybox <applet>" - never
# quote it. getprop stays Android's; mount/umount/mkdir/chroot use "$BUSYBOX".
BB="$BUSYBOX"
CAT="$BB cat"
ECHO="$BB echo"
GREP="$BB grep"
ID="$BB id"
KILL="$BB kill"
LS="$BB ls"
PRINTF="$BB printf"
RM="$BB rm"
SED="$BB sed"
SLEEP="$BB sleep"
SORT="$BB sort"

# Must return 0 even when silent: log is often the last statement of a
# function, and `[ ... ] && echo` returns 1 under -s, failing the caller.
log() { [ "$SILENT" -eq 0 ] && $ECHO "[INFO] $1"; return 0; }
warn() { [ "$SILENT" -eq 0 ] && $ECHO "[WARN] $1"; return 0; }
error() { $ECHO "[ERROR] $1"; }

usage() {
    $ECHO "Usage: $SCRIPT_NAME [command]"
    $ECHO ""
    $ECHO "Commands:"
    $ECHO "  start         Mount and prepare the VirtualAP environment."
    $ECHO "  stop          Kill chroot processes and unmount everything."
    $ECHO "  status        Show the current status."
    $ECHO "  run <command> Execute a command inside the chroot."
    $ECHO ""
    $ECHO "Options:"
    $ECHO "  -s            Silent mode."
    exit 1
}

# --- Namespace handling (proven pattern from Ubuntu-Chroot) ---

_get_ns_flags() {
    # Translate saved long flags (--mount) to the short flags (-m) that
    # busybox nsenter requires.
    local flags_file="$HOLDER_PID_FILE.flags"
    if [ ! -f "$flags_file" ]; then
        $ECHO "-m"; return
    fi

    local long_flags short_flags
    long_flags=$($CAT "$flags_file")
    [ -z "$long_flags" ] && { $ECHO "-m"; return; }

    for flag in $long_flags; do
        case "$flag" in
            --mount) short_flags="$short_flags -m" ;;
            --uts)   short_flags="$short_flags -u" ;;
            --ipc)   short_flags="$short_flags -i" ;;
            --pid)   short_flags="$short_flags -p" ;;
        esac
    done

    [ -z "$short_flags" ] && { $ECHO "-m"; return; }
    $ECHO "$short_flags"
}

_execute_in_ns() {
    local holder_pid
    if [ -f "$HOLDER_PID_FILE" ] && $KILL -0 "$($CAT "$HOLDER_PID_FILE")" 2>/dev/null; then
        holder_pid=$($CAT "$HOLDER_PID_FILE")
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
    _execute_in_ns "$BUSYBOX" chroot "$CHROOT_PATH" /bin/sh -c "PATH=/usr/sbin:/usr/bin:/sbin:/bin; export TMPDIR=/tmp; $*"
}

is_running() {
    [ -f "$HOLDER_PID_FILE" ] && $KILL -0 "$($CAT "$HOLDER_PID_FILE")" 2>/dev/null
}

create_namespace() {
    local pid_file="$1"
    local unshare_flags=""

    # Probe each namespace individually (kernels vary). --net is absent on
    # purpose: the AP interface must stay in the host netns.
    for ns_flag in --pid --mount --uts --ipc; do
        if "${BUSYBOX}" unshare "$ns_flag" true 2>/dev/null; then
            unshare_flags="$unshare_flags $ns_flag"
        fi
    done

    if ! $ECHO "$unshare_flags" | $GREP -q -- "--mount"; then
        error "Mount namespace not supported - cannot create chroot"
        return 1
    fi

    log "using flags:$unshare_flags"
    $ECHO "$unshare_flags" > "${pid_file}.flags"

    # Background a sleeper inside the new namespaces and record its PID.
    "${BUSYBOX}" unshare $unshare_flags sh -c '"$2" sleep infinity & echo $! > "$1"' -- "$pid_file" "$BUSYBOX"

    local attempts=0
    while [ $attempts -lt 10 ]; do
        if [ -f "$pid_file" ] && $KILL -0 "$($CAT "$pid_file")" 2>/dev/null; then
            return 0
        fi
        $SLEEP 0.1
        attempts=$((attempts + 1))
    done

    error "Failed to create and capture namespace holder PID."
    $RM -f "$pid_file" "${pid_file}.flags"
    return 1
}

# --- Mount helpers ---

advanced_mount() {
    local src="$1" tgt="$2" type="$3" opts="$4"

    [ ! -d "$tgt" ] && run_in_ns "$BUSYBOX" mkdir -p "$tgt" 2>/dev/null

    if [ "$type" = "bind" ]; then
        [ -e "$src" ] || { warn "Source for bind mount does not exist: $src"; return 1; }
        run_in_ns "$BUSYBOX" mount -o bind "$src" "$tgt"
    else
        run_in_ns "$BUSYBOX" mount -t "$type" $opts "$type" "$tgt"
    fi

    if [ $? -eq 0 ]; then
        log "Mounted $src -> $tgt ($type)"
        $ECHO "$tgt" >> "$MOUNTED_FILE"
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
    $PRINTF "$dns_lines" > "$CHROOT_PATH/etc/resolv.conf"
}

# --- Core actions ---

start_env() {
    log "Setting up VirtualAP environment..."

    if is_running; then
        log "Namespace holder already running."
    else
        log "Creating new isolated namespace..."
        create_namespace "$HOLDER_PID_FILE" || return 1
        $SLEEP 0.5
        log "Running in isolated namespace (PID: $($CAT "$HOLDER_PID_FILE"))"
    fi

    [ -d "$CHROOT_PATH" ] || { error "Rootfs not found at $CHROOT_PATH"; exit 1; }

    $RM -f "$MOUNTED_FILE"

    log "Setting up system mounts..."
    advanced_mount "proc" "$CHROOT_PATH/proc" "proc" "-o rw,nosuid,nodev,noexec,relatime"
    advanced_mount "sysfs" "$CHROOT_PATH/sys" "sysfs" "-o rw,nosuid,nodev,noexec,relatime"

    if $GREP -q devtmpfs /proc/filesystems; then
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

    # Find chroot procs (hostapd/dnsmasq) in one ls pass over /proc/*/root.
    # NOT lsof - a full fd scan takes 10-30s on Android.
    local pids
    pids=$($LS -l /proc/[0-9]*/root 2>/dev/null \
        | $GREP -F " -> $CHROOT_PATH" \
        | $SED -n 's|.*/proc/\([0-9]*\)/root.*|\1|p')
    if [ -n "$pids" ]; then
        $KILL -9 $pids 2>/dev/null
        log "Killed chroot processes."
    fi
}

umount_env() {
    if [ -f "$MOUNTED_FILE" ]; then
        log "Unmounting filesystems..."
        $SORT -r "$MOUNTED_FILE" | while read -r mount_point; do
            case "$mount_point" in
                "$CHROOT_PATH"/sys*) run_in_ns "$BUSYBOX" umount -l "$mount_point" 2>/dev/null ;;
                *) run_in_ns "$BUSYBOX" umount "$mount_point" 2>/dev/null || run_in_ns "$BUSYBOX" umount -l "$mount_point" 2>/dev/null ;;
            esac
        done
        $RM -f "$MOUNTED_FILE"
        log "All mounts cleaned."
    fi
}

stop_env() {
    log "Stopping VirtualAP environment..."

    kill_chroot_processes
    umount_env

    if [ -f "$HOLDER_PID_FILE" ]; then
        local holder_pid
        holder_pid=$($CAT "$HOLDER_PID_FILE")
        if $KILL -0 "$holder_pid" 2>/dev/null; then
            # SIGKILL, not TERM: the PID-ns init ignores signals it has no
            # handler for. Killing it also makes the kernel reap the namespace.
            $KILL -9 "$holder_pid" 2>/dev/null && log "Killed namespace holder." || warn "Failed to kill holder."
        fi
        $RM -f "$HOLDER_PID_FILE" "$HOLDER_PID_FILE.flags"
    fi

    log "Stopped."
}

show_status() {
    if is_running; then
        $ECHO "Status: RUNNING"
        $ECHO "Namespace Holder PID: $($CAT "$HOLDER_PID_FILE")"
        [ -f "$HOLDER_PID_FILE.flags" ] && $ECHO "Namespace Flags: $($CAT "$HOLDER_PID_FILE.flags")"
    else
        $ECHO "Status: STOPPED"
    fi
}

# --- Main ---
if [ "$($ID -u)" -ne 0 ]; then
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
                $ECHO "Unknown option: $arg"; usage
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
