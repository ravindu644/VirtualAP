package com.virtualap.app.util

object Constants {
    const val VAP_DIR = "/data/local/virtualap"
    /** The engine; Backend.startAp is how the app runs it. */
    const val ENGINE = "$VAP_DIR/bin/virtualap"
    /** Droidspaces' CLI, asked for its running containers; only ever run from
     *  this path. The engine's copy is DROIDSPACES_BIN in src/mode/internal.h. */
    const val DROIDSPACES = "/data/local/Droidspaces/bin/droidspaces"
    const val MODULES_DIR = "/data/adb/modules"
    const val BOOT_MODULE = "$MODULES_DIR/virtualap"
    const val AP_CONF = "$VAP_DIR/ap.conf"
    const val BOOT_MODE_FILE = "$VAP_DIR/.boot_mode"
    const val PREFS_NAME = "virtualap_prefs"
    const val KEY_ROOT_AVAILABLE = "root_available"
    const val KEY_FOLLOW_SYSTEM_THEME = "follow_system_theme"
    const val KEY_DARK_THEME = "dark_theme"
    const val KEY_AMOLED_MODE = "amoled_mode"
    const val KEY_USE_DYNAMIC_COLOR = "use_dynamic_color"
    const val KEY_THEME_PALETTE = "theme_palette"
    const val KEY_HAS_SEEN_WELCOME = "has_seen_welcome"
    const val KEY_HAS_SEEN_ROOT_CHECK = "has_seen_root_check"
    const val KEY_PAYLOAD_VERSION = "payload_version"
}
