package com.virtualap.app.util

import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import androidx.core.content.edit

/** The app's SharedPreferences. [init] runs once from the Application, before anything reads it. */
object PreferencesManager {
    lateinit var prefs: SharedPreferences
        private set

    fun init(context: Context) {
        prefs = context.getSharedPreferences(Constants.PREFS_NAME, Context.MODE_PRIVATE)
    }

    private fun bool(key: String, default: Boolean) = prefs.getBoolean(key, default)
    private fun str(key: String, default: String) = prefs.getString(key, default) ?: default

    var rootAvailable: Boolean
        get() = bool(Constants.KEY_ROOT_AVAILABLE, false)
        set(value) = prefs.edit { putBoolean(Constants.KEY_ROOT_AVAILABLE, value) }

    var hasSeenRootCheck: Boolean
        get() = bool(Constants.KEY_HAS_SEEN_ROOT_CHECK, false)
        set(value) = prefs.edit { putBoolean(Constants.KEY_HAS_SEEN_ROOT_CHECK, value) }

    /** Hash of the static-binary payload last deployed (see VirtualAPInstaller). */
    var payloadVersion: String
        get() = str(Constants.KEY_PAYLOAD_VERSION, "")
        set(value) = prefs.edit { putString(Constants.KEY_PAYLOAD_VERSION, value) }

    var followSystemTheme: Boolean
        get() = bool(Constants.KEY_FOLLOW_SYSTEM_THEME, true)
        set(value) = prefs.edit { putBoolean(Constants.KEY_FOLLOW_SYSTEM_THEME, value) }

    var darkTheme: Boolean
        get() = bool(Constants.KEY_DARK_THEME, false)
        set(value) = prefs.edit { putBoolean(Constants.KEY_DARK_THEME, value) }

    var amoledMode: Boolean
        get() = bool(Constants.KEY_AMOLED_MODE, false)
        set(value) = prefs.edit { putBoolean(Constants.KEY_AMOLED_MODE, value) }

    /** Material You needs Android 12; below it this reads false whatever is stored. */
    var useDynamicColor: Boolean
        get() = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && bool(Constants.KEY_USE_DYNAMIC_COLOR, true)
        set(value) = prefs.edit { putBoolean(Constants.KEY_USE_DYNAMIC_COLOR, value) }

    var themePalette: String
        get() = str(Constants.KEY_THEME_PALETTE, "CATPPUCCIN")
        set(value) = prefs.edit { putString(Constants.KEY_THEME_PALETTE, value) }

    /**
     * The saved hotspot configuration, read whole and written whole: the view model
     * writes it on every change, so the setter is one batched edit. A saved channel
     * or width the saved band cannot carry comes back as auto.
     */
    var apConfig: APConfig
        get() {
            val band = str(Constants.KEY_AP_BAND, "2")
            return APConfig(
                ssid = str(Constants.KEY_AP_SSID, ""),
                password = str(Constants.KEY_AP_PASSWORD, ""),
                band = band,
                channel = APConfig.validChannelForBand(band, str(Constants.KEY_AP_CHANNEL, "")),
                width = APConfig.validWidthForBand(band, str(Constants.KEY_AP_WIDTH, "auto")),
                upstream = str(Constants.KEY_AP_UPSTREAM, "auto"),
                // Blank shows the default as a hint; a stored default from older versions reads as blank.
                gateway = str(Constants.KEY_AP_GATEWAY, "").takeUnless { it == APConfig.DEFAULT_GATEWAY } ?: "",
                dnsServers = str(Constants.KEY_AP_DNS, ""),
                hidden = bool(Constants.KEY_AP_HIDDEN, false),
                security = str(Constants.KEY_AP_SECURITY, "wpa2"),
                pmf = bool(Constants.KEY_AP_PMF, false),
                ttlFix = bool(Constants.KEY_AP_TTL_FIX, false),
                ipv6 = bool(Constants.KEY_AP_IPV6, true),
                containerMode = bool(Constants.KEY_AP_CONTAINER_MODE, false),
                containerName = str(Constants.KEY_AP_CONTAINER, ""),
                macMode = str(Constants.KEY_AP_MAC_MODE, "random"),
                gatewayMac = str(Constants.KEY_AP_GATEWAY_MAC, "")
            )
        }
        set(c) = prefs.edit {
            putString(Constants.KEY_AP_SSID, c.ssid)
            putString(Constants.KEY_AP_PASSWORD, c.password)
            putString(Constants.KEY_AP_BAND, c.band)
            putString(Constants.KEY_AP_CHANNEL, c.channel)
            putString(Constants.KEY_AP_WIDTH, c.width)
            putString(Constants.KEY_AP_UPSTREAM, c.upstream)
            putString(Constants.KEY_AP_GATEWAY, c.gateway)
            putString(Constants.KEY_AP_DNS, c.dnsServers)
            putBoolean(Constants.KEY_AP_HIDDEN, c.hidden)
            putString(Constants.KEY_AP_SECURITY, c.security)
            putBoolean(Constants.KEY_AP_PMF, c.pmf)
            putBoolean(Constants.KEY_AP_TTL_FIX, c.ttlFix)
            putBoolean(Constants.KEY_AP_IPV6, c.ipv6)
            putBoolean(Constants.KEY_AP_CONTAINER_MODE, c.containerMode)
            putString(Constants.KEY_AP_CONTAINER, c.containerName)
            putString(Constants.KEY_AP_MAC_MODE, c.macMode)
            putString(Constants.KEY_AP_GATEWAY_MAC, c.gatewayMac)
        }
}
