#!/system/bin/sh
set -e

MOD_PATH=/data/adb/modules/virtualap
MODULES_DIR=/data/adb/modules
STATE_DIR=/data/local/virtualap
MODULE_PROP=${1:?module.prop path required}
SERVICE=${2:?service.sh path required}

if [ "$(id -u)" -ne 0 ]; then
    echo "VirtualAP module installation requires root." >&2
    exit 1
fi
[ -d "$MODULES_DIR" ] || {
    echo "Magisk/KernelSU module directory not found: $MODULES_DIR" >&2
    exit 1
}
[ -f "$MODULE_PROP" ] && [ -f "$SERVICE" ]

rm -rf "$MOD_PATH"
mkdir -p "$MOD_PATH" "$STATE_DIR"
cp "$MODULE_PROP" "$MOD_PATH/module.prop"
cp "$SERVICE" "$MOD_PATH/service.sh"
chmod 755 "$MOD_PATH" "$MOD_PATH/service.sh" "$STATE_DIR"
chmod 644 "$MOD_PATH/module.prop"

if [ ! -f "$STATE_DIR/.boot_mode" ]; then
    if [ -f /data/local/VirtualAP/.boot_mode ]; then
        cp /data/local/VirtualAP/.boot_mode "$STATE_DIR/.boot_mode"
    else
        printf '%s\n' 0 > "$STATE_DIR/.boot_mode"
    fi
fi
chmod 666 "$STATE_DIR/.boot_mode"

echo "VirtualAP boot module installed at $MOD_PATH"
