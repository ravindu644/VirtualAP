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
        // Older releases kept the hotspot config here, password included. It lives in ap.conf now.
        prefs.edit { prefs.all.keys.filter { it.startsWith("ap_") }.forEach { remove(it) } }
    }

    private fun bool(key: String, default: Boolean) = prefs.getBoolean(key, default)
    private fun str(key: String, default: String) = prefs.getString(key, default) ?: default

    var rootAvailable: Boolean
        get() = bool(Constants.KEY_ROOT_AVAILABLE, false)
        set(value) = prefs.edit { putBoolean(Constants.KEY_ROOT_AVAILABLE, value) }

    var hasSeenWelcome: Boolean
        get() = bool(Constants.KEY_HAS_SEEN_WELCOME, false)
        set(value) = prefs.edit { putBoolean(Constants.KEY_HAS_SEEN_WELCOME, value) }

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
        get() = str(Constants.KEY_THEME_PALETTE, "BROCCOLI")
        set(value) = prefs.edit { putString(Constants.KEY_THEME_PALETTE, value) }
}
