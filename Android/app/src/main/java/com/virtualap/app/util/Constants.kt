package com.virtualap.app.util

object Constants {
    const val VAP_DIR = "/data/local/virtualap"
    const val START_AP = "sh $VAP_DIR/start-ap"
    const val VAP_SH = "sh $VAP_DIR/vap.sh"
    const val CONF_FILE = "$VAP_DIR/ap.conf"
    const val LOG_FILE = "$VAP_DIR/logs/ap.log"
    const val BUSYBOX = "$VAP_DIR/bin/busybox"
    const val PREFS_NAME = "virtualap_prefs"
    const val KEY_ROOT_AVAILABLE = "root_available"
    const val KEY_INSTALLED = "installed"
    const val KEY_FOLLOW_SYSTEM_THEME = "follow_system_theme"
    const val KEY_DARK_THEME = "dark_theme"
    const val KEY_AMOLED_MODE = "amoled_mode"
    const val KEY_USE_DYNAMIC_COLOR = "use_dynamic_color"
    const val KEY_THEME_PALETTE = "theme_palette"
    const val KEY_AP_SSID = "ap_ssid"
    const val KEY_AP_PASSWORD = "ap_password"
    const val KEY_AP_BAND = "ap_band"
    const val KEY_AP_CHANNEL = "ap_channel"
    const val KEY_AP_UPSTREAM = "ap_upstream"
    const val KEY_HAS_SEEN_ROOT_CHECK = "has_seen_root_check"
}
