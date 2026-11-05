package com.virtualap.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import com.virtualap.app.util.Hotspot
import kotlinx.coroutines.launch

/**
 * Sent by the boot module once a hotspot it started can be told about. The first refresh
 * in a new process always fans out, so this puts up the notification and the tile.
 */
class HotspotBootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val result = goAsync()
        Hotspot.scope.launch { Hotspot.refresh() }.invokeOnCompletion { result.finish() }
    }
}
