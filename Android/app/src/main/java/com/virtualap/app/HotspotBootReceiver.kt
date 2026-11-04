package com.virtualap.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import com.virtualap.app.util.Hotspot
import com.virtualap.app.util.PreferencesManager

class HotspotBootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_BOOT_COMPLETED && PreferencesManager.startAtBoot) {
            Hotspot.start(PreferencesManager.apConfig)
        }
    }
}
