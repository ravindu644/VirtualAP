#!/system/bin/sh
MODDIR=${0%/*}
VAP_DIR=/data/local/virtualap
LOGS_DIR=${VAP_DIR}/logs
LOGS_FILE=${LOGS_DIR}/boot-module.log
DROIDSPACES=/data/local/Droidspaces/bin/droidspaces

mkdir -p "${LOGS_DIR}" 2>/dev/null
exec > "${LOGS_FILE}" 2>&1

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*"
}

# What the root manager shows under the module's name.
update_prop() {
    sed -i "s/^description=.*/description=$*/" "${MODDIR}/module.prop"
}

# Up to 25 seconds for what the start cannot do without. $1 names it, the rest is its test.
wait_for() {
    what=$1
    shift
    log "Waiting for ${what}..."
    count=0
    while [ ${count} -lt 25 ]; do
        if "$@" >/dev/null 2>&1; then
            log "${what} is ready (${count}s)."
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    log "ERROR: ${what} did not come up after 25s, skipping the start."
    update_prop "🔴 Start skipped, ${what} did not come up"
    exit 1
}

while [ "$(getprop sys.boot_completed 2>/dev/null)" != "1" ]; do
    sleep 1
done

if [ "$(cat "${VAP_DIR}/.boot_mode" 2>/dev/null)" != "1" ]; then
    log "Start at boot is off."
    update_prop "⚪ Start at boot is off"
    exit 0
fi

[ -f "${VAP_DIR}/ap.conf" ] && . "${VAP_DIR}/ap.conf"
PHY_IFACE=${PHY_IFACE:-wlan0}

wait_for "${PHY_IFACE}" [ -e "/sys/class/net/${PHY_IFACE}" ]
# Managed mode refuses to start without its gateway container, which Droidspaces
# starts on its own at boot. An upstream can come later, the hotspot follows it.
if [ -n "${CONTAINER}" ]; then
    wait_for "container ${CONTAINER}" "${DROIDSPACES}" -n "${CONTAINER}" pid
fi

if "${VAP_DIR}/bin/virtualap" start; then
    log "Access point started."
    update_prop "🟢 Access point started at boot"
else
    log "ERROR: Access point did not start."
    update_prop "🔴 Access point did not start at boot, see boot-module.log"
    exit 1
fi
