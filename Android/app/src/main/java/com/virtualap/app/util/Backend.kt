package com.virtualap.app.util

import android.content.Context
import java.io.File

/**
 * Backend scripts live in the app's private files dir and are re-extracted from
 * APK assets on every launch, so the running scripts are always exactly the
 * version the installed APK shipped. An APK update can never leave stale
 * scripts behind (unlike the old copy-to-/data/local/virtualap approach, where
 * the installer only ran once and updates silently kept executing old code).
 *
 * Scripts are invoked via `sh <path>` under root, so no exec permission is
 * needed on app-data files. The heavy, rarely-changing payload (the static
 * hostapd/iw/dnsmasq/busybox binaries) and all runtime state stay in
 * /data/local/virtualap.
 */
object Backend {
    /** Every dynamic value that reaches the root shell goes through here: POSIX single-quote wrap. */
    fun quote(value: String): String = "'" + value.replace("'", "'\\''") + "'"

    /** Full root-shell command prefix, e.g. "sh /data/data/<pkg>/files/backend/start-ap" */
    lateinit var startAp: String
        private set

    fun install(context: Context) {
        val dir = File(context.filesDir, "backend").apply { mkdirs() }
        val out = File(dir, "start-ap")
        context.assets.open("backend/start-ap").use { input ->
            out.outputStream().use { input.copyTo(it) }
        }
        startAp = "sh ${out.absolutePath}"
    }
}
