package com.virtualap.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import com.virtualap.app.util.Hotspot
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/** The notification's Stop action. goAsync keeps the process alive for the stop. */
class HotspotStopReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val result = goAsync()
        Hotspot.stop()
        Hotspot.scope.launch {
            while (Hotspot.phase != Hotspot.Phase.IDLE) delay(200)
            result.finish()
        }
    }
}
