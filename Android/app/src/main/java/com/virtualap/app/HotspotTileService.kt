package com.virtualap.app

import android.app.PendingIntent
import android.content.Intent
import android.os.Build
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import android.widget.Toast
import com.virtualap.app.util.APConfig
import com.virtualap.app.util.Hotspot
import com.virtualap.app.util.PreferencesManager
import com.virtualap.app.util.VirtualAPInstaller
import kotlinx.coroutines.launch

/**
 * Quick Settings toggle. Optimistic like AOSP's HotspotTile: the tap paints
 * "Starting" before the shell runs and reverts if it fails. All state lives
 * in [Hotspot]; SystemUI may unbind this instance mid-start.
 */
class HotspotTileService : TileService() {

    override fun onStartListening() {
        render()
        Hotspot.scope.launch {
            Hotspot.refresh()
            render()
        }
    }

    override fun onClick() {
        if (Hotspot.phase != Hotspot.Phase.IDLE) return
        if (Hotspot.status.running) {
            Hotspot.stop()
            render()
            return
        }
        val prefs = PreferencesManager.getInstance(this)
        if (!prefs.rootAvailable || VirtualAPInstaller.payloadUpdateAvailable(this)) {
            openApp()
            return
        }
        val cfg = APConfig.fromPrefs(prefs)
        if (!cfg.isValid()) {
            Toast.makeText(this, R.string.tile_configure_first, Toast.LENGTH_LONG).show()
            openApp()
            return
        }
        Hotspot.start(cfg)
        render()
    }

    private fun render() {
        val tile = qsTile ?: return
        val (state, subtitle) = when {
            Hotspot.phase == Hotspot.Phase.STARTING -> Tile.STATE_ACTIVE to getString(R.string.starting)
            Hotspot.phase == Hotspot.Phase.STOPPING -> Tile.STATE_INACTIVE to getString(R.string.stopping)
            Hotspot.status.running -> Tile.STATE_ACTIVE to Hotspot.status.ssid
            else -> Tile.STATE_INACTIVE to null
        }
        tile.state = state
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) tile.subtitle = subtitle
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) tile.stateDescription = subtitle
        tile.updateTile()
    }

    private fun openApp() {
        val intent = Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            startActivityAndCollapse(PendingIntent.getActivity(this, 0, intent, PendingIntent.FLAG_IMMUTABLE))
        } else {
            @Suppress("DEPRECATION")
            startActivityAndCollapse(intent)
        }
    }
}
