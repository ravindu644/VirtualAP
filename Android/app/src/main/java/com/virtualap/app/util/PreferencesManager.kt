package com.virtualap.app.util

import android.content.Context
import android.content.SharedPreferences

/**
 * Simplified PreferencesManager for VirtualAP.
 * Singleton pattern with double-checked locking for thread safety.
 */
class PreferencesManager private constructor(context: Context) {
    val prefs: SharedPreferences = context.getSharedPreferences(
        Constants.PREFS_NAME,
        Context.MODE_PRIVATE
    )

    var rootAvailable: Boolean
        get() = prefs.getBoolean(Constants.KEY_ROOT_AVAILABLE, false)
        set(value) {
            prefs.edit().putBoolean(Constants.KEY_ROOT_AVAILABLE, value).apply()
        }

    var isInstalled: Boolean
        get() = prefs.getBoolean(Constants.KEY_INSTALLED, false)
        set(value) {
            prefs.edit().putBoolean(Constants.KEY_INSTALLED, value).apply()
        }

    var followSystemTheme: Boolean
        get() = prefs.getBoolean(Constants.KEY_FOLLOW_SYSTEM_THEME, true)
        set(value) {
            prefs.edit().putBoolean(Constants.KEY_FOLLOW_SYSTEM_THEME, value).apply()
        }

    var darkTheme: Boolean
        get() = prefs.getBoolean(Constants.KEY_DARK_THEME, false)
        set(value) {
            prefs.edit().putBoolean(Constants.KEY_DARK_THEME, value).apply()
        }

    var amoledMode: Boolean
        get() = prefs.getBoolean(Constants.KEY_AMOLED_MODE, false)
        set(value) {
            prefs.edit().putBoolean(Constants.KEY_AMOLED_MODE, value).apply()
        }

    var useDynamicColor: Boolean
        get() = prefs.getBoolean(Constants.KEY_USE_DYNAMIC_COLOR, true)
        set(value) {
            prefs.edit().putBoolean(Constants.KEY_USE_DYNAMIC_COLOR, value).apply()
        }

    var themePalette: String
        get() = prefs.getString(Constants.KEY_THEME_PALETTE, "CATPPUCCIN") ?: "CATPPUCCIN"
        set(value) {
            prefs.edit().putString(Constants.KEY_THEME_PALETTE, value).apply()
        }

    var apSsid: String
        get() = prefs.getString(Constants.KEY_AP_SSID, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_AP_SSID, value).apply() }

    var apPassword: String
        get() = prefs.getString(Constants.KEY_AP_PASSWORD, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_AP_PASSWORD, value).apply() }

    var apBand: String
        get() = prefs.getString(Constants.KEY_AP_BAND, "2") ?: "2"
        set(value) { prefs.edit().putString(Constants.KEY_AP_BAND, value).apply() }

    var apChannel: String
        get() = prefs.getString(Constants.KEY_AP_CHANNEL, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_AP_CHANNEL, value).apply() }

    var apUpstream: String
        get() = prefs.getString(Constants.KEY_AP_UPSTREAM, "auto") ?: "auto"
        set(value) { prefs.edit().putString(Constants.KEY_AP_UPSTREAM, value).apply() }

    var apGateway: String
        get() = prefs.getString(Constants.KEY_AP_GATEWAY, "192.168.42.1") ?: "192.168.42.1"
        set(value) { prefs.edit().putString(Constants.KEY_AP_GATEWAY, value).apply() }

    var apDnsServers: String
        get() = prefs.getString(Constants.KEY_AP_DNS, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_AP_DNS, value).apply() }

    var apHidden: Boolean
        get() = prefs.getBoolean(Constants.KEY_AP_HIDDEN, false)
        set(value) { prefs.edit().putBoolean(Constants.KEY_AP_HIDDEN, value).apply() }

    var hasSeenRootCheck: Boolean
        get() = prefs.getBoolean(Constants.KEY_HAS_SEEN_ROOT_CHECK, false)
        set(value) { prefs.edit().putBoolean(Constants.KEY_HAS_SEEN_ROOT_CHECK, value).apply() }

    /** Filename of the rootfs tarball that was last extracted (embeds the build date). */
    var rootfsVersion: String
        get() = prefs.getString(Constants.KEY_ROOTFS_VERSION, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_ROOTFS_VERSION, value).apply() }

    companion object {
        @Volatile
        private var INSTANCE: PreferencesManager? = null

        @JvmStatic
        fun getInstance(context: Context): PreferencesManager {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: PreferencesManager(context.applicationContext).also {
                    INSTANCE = it
                }
            }
        }
    }
}
