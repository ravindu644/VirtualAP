package com.virtualap.app

import android.app.Application
import com.topjohnwu.superuser.Shell
import com.virtualap.app.util.Backend
import com.virtualap.app.util.Hotspot
import com.virtualap.app.util.PreferencesManager

class VirtualAPApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        Shell.setDefaultBuilder(
            Shell.Builder.create()
                .setFlags(Shell.FLAG_REDIRECT_STDERR)
                .setTimeout(30)
        )
        // Synchronous: everything that shells out depends on these paths.
        PreferencesManager.init(this)
        Backend.install(this)
        Hotspot.init(this)
    }
}
