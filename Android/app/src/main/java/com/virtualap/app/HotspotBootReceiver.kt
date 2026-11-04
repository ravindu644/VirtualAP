package com.virtualap.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import com.virtualap.app.util.Hotspot
import com.virtualap.app.util.PreferencesManager
import com.virtualap.app.util.VirtualAPInstaller
import kotlinx.coroutines.launch

class HotspotBootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_BOOT_COMPLETED) return
        val pending = goAsync()
        Hotspot.scope.launch {
            try {
                if (VirtualAPInstaller.bootModeEnabled() && !Hotspot.refresh().running) {
                    Hotspot.start(PreferencesManager.apConfig)?.join()
                }
            } finally {
                pending.finish()
            }
        }
    }
}
