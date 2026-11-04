#!/system/bin/sh
STATE_DIR=/data/local/VirtualAP
FLAG_FILE="$STATE_DIR/.boot_mode"
ARGS_FILE="$STATE_DIR/boot.args"
LOG_FILE="$STATE_DIR/boot.log"
ENGINE=/data/local/virtualap/bin/virtualap

log_msg() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOG_FILE"
}

mkdir -p "$STATE_DIR"
chmod 755 "$STATE_DIR"
log_msg "VirtualAP boot service started."

TIMEOUT=120
ELAPSED=0
while [ "$(getprop sys.boot_completed)" != 1 ]; do
    sleep 2
    ELAPSED=$((ELAPSED + 2))
    if [ "$ELAPSED" -ge "$TIMEOUT" ]; then
        log_msg "ERROR: Boot completion timed out."
        exit 1
    fi
done

sleep 5
BOOT_MODE=$(cat "$FLAG_FILE" 2>/dev/null | tr -d '[:space:]')
if [ "$BOOT_MODE" != 1 ]; then
    log_msg "Auto-start is disabled."
    exit 0
fi
if [ ! -x "$ENGINE" ]; then
    log_msg "ERROR: VirtualAP engine is missing."
    exit 1
fi

set --
if [ -r "$ARGS_FILE" ]; then
    while IFS= read -r option; do
        if ! IFS= read -r value; then
            log_msg "ERROR: Incomplete boot arguments."
            exit 1
        fi
        case "$option" in
            -s|-p|-o|-b|-c|-W|-g|-d|-H|-A|-M|-K|-T|-6|-m) ;;
            *)
                log_msg "ERROR: Invalid boot argument."
                exit 1
                ;;
        esac
        set -- "$@" "$option" "$value"
    done < "$ARGS_FILE"
else
    log_msg "Boot arguments are missing; using the backend's saved configuration."
fi

log_msg "Starting the saved VirtualAP configuration."
ATTEMPT=1
while [ "$ATTEMPT" -le 6 ]; do
    if "$ENGINE" start "$@" >> "$LOG_FILE" 2>&1; then
        log_msg "VirtualAP started."
        exit 0
    fi
    if [ "$ATTEMPT" -eq 6 ]; then
        log_msg "ERROR: VirtualAP failed to start after $ATTEMPT attempts."
        exit 1
    fi
    log_msg "VirtualAP start failed on attempt $ATTEMPT; retrying in 10 seconds."
    sleep 10
    ATTEMPT=$((ATTEMPT + 1))
done
