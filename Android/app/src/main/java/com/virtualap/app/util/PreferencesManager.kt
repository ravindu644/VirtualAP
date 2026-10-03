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

    var apWidth: String
        get() = prefs.getString(Constants.KEY_AP_WIDTH, "auto") ?: "auto"
        set(value) { prefs.edit().putString(Constants.KEY_AP_WIDTH, value).apply() }

    var apUpstream: String
        get() = prefs.getString(Constants.KEY_AP_UPSTREAM, "auto") ?: "auto"
        set(value) { prefs.edit().putString(Constants.KEY_AP_UPSTREAM, value).apply() }

    // Blank = use the default gateway (APConfig.DEFAULT_GATEWAY). Stored blank
    // so the field shows the default as a hint rather than a prefilled value.
    var apGateway: String
        get() = prefs.getString(Constants.KEY_AP_GATEWAY, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_AP_GATEWAY, value).apply() }

    var apDnsServers: String
        get() = prefs.getString(Constants.KEY_AP_DNS, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_AP_DNS, value).apply() }

    var apHidden: Boolean
        get() = prefs.getBoolean(Constants.KEY_AP_HIDDEN, false)
        set(value) { prefs.edit().putBoolean(Constants.KEY_AP_HIDDEN, value).apply() }

    // Security mode: open | wpa2 | wpa2wpa3 | wpa3.
    var apSecurity: String
        get() = prefs.getString(Constants.KEY_AP_SECURITY, "wpa2") ?: "wpa2"
        set(value) { prefs.edit().putString(Constants.KEY_AP_SECURITY, value).apply() }

    // Protected Management Frames; only meaningful in wpa2 mode.
    var apPmf: Boolean
        get() = prefs.getBoolean(Constants.KEY_AP_PMF, false)
        set(value) { prefs.edit().putBoolean(Constants.KEY_AP_PMF, value).apply() }

    var apTtlFix: Boolean
        get() = prefs.getBoolean(Constants.KEY_AP_TTL_FIX, false)
        set(value) { prefs.edit().putBoolean(Constants.KEY_AP_TTL_FIX, value).apply() }

    var apContainerMode: Boolean
        get() = prefs.getBoolean(Constants.KEY_AP_CONTAINER_MODE, false)
        set(value) { prefs.edit().putBoolean(Constants.KEY_AP_CONTAINER_MODE, value).apply() }

    var apContainer: String
        get() = prefs.getString(Constants.KEY_AP_CONTAINER, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_AP_CONTAINER, value).apply() }

    var hasSeenRootCheck: Boolean
        get() = prefs.getBoolean(Constants.KEY_HAS_SEEN_ROOT_CHECK, false)
        set(value) { prefs.edit().putBoolean(Constants.KEY_HAS_SEEN_ROOT_CHECK, value).apply() }

    /** Hash of the static-binary payload last deployed (see VirtualAPInstaller). */
    var payloadVersion: String
        get() = prefs.getString(Constants.KEY_PAYLOAD_VERSION, "") ?: ""
        set(value) { prefs.edit().putString(Constants.KEY_PAYLOAD_VERSION, value).apply() }

    /**
     * Persist the full AP config in one batched edit. The viewmodel writes the
     * whole config on every change, so this avoids 13 separate apply() calls
     * (one per field) on each keystroke.
     */
    fun saveApConfig(
        ssid: String, password: String, band: String, channel: String, width: String,
        upstream: String, gateway: String, dnsServers: String, hidden: Boolean,
        security: String, pmf: Boolean, ttlFix: Boolean, containerMode: Boolean,
        container: String
    ) {
        prefs.edit()
            .putString(Constants.KEY_AP_SSID, ssid)
            .putString(Constants.KEY_AP_PASSWORD, password)
            .putString(Constants.KEY_AP_BAND, band)
            .putString(Constants.KEY_AP_CHANNEL, channel)
            .putString(Constants.KEY_AP_WIDTH, width)
            .putString(Constants.KEY_AP_UPSTREAM, upstream)
            .putString(Constants.KEY_AP_GATEWAY, gateway)
            .putString(Constants.KEY_AP_DNS, dnsServers)
            .putBoolean(Constants.KEY_AP_HIDDEN, hidden)
            .putString(Constants.KEY_AP_SECURITY, security)
            .putBoolean(Constants.KEY_AP_PMF, pmf)
            .putBoolean(Constants.KEY_AP_TTL_FIX, ttlFix)
            .putBoolean(Constants.KEY_AP_CONTAINER_MODE, containerMode)
            .putString(Constants.KEY_AP_CONTAINER, container)
            .apply()
    }

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
