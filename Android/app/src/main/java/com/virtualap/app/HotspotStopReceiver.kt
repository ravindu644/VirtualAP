package com.virtualap.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import com.virtualap.app.util.Hotspot

/** The notification's Stop action. goAsync keeps the process alive for the stop. */
class HotspotStopReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val result = goAsync()
        Hotspot.stop()?.invokeOnCompletion { result.finish() } ?: result.finish()
    }
}
