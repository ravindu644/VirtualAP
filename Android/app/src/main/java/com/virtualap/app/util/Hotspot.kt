package com.virtualap.app.util

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.service.quicksettings.TileService
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.app.NotificationCompat
import com.virtualap.app.HotspotStopReceiver
import com.virtualap.app.HotspotTileService
import com.virtualap.app.MainActivity
import com.virtualap.app.R
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * The one owner of a start/stop session. It outlives the ViewModel so a start
 * begun from a Quick Settings tile is visible to the screen and the other way
 * round. Everything that runs start-ap start/stop goes through here.
 */
object Hotspot {
    enum class Phase { IDLE, STARTING, STOPPING }

    var phase by mutableStateOf(Phase.IDLE)
        private set
    var status by mutableStateOf(APStatus())
        private set
    val actionLogs = mutableStateListOf<Pair<Int, String>>()
    val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    private lateinit var app: Context
    private val logger = ViewModelLogger { level, msg -> actionLogs.add(level to msg) }
    // The default status is a guess until the first refresh lands, so that
    // first result always fans out even when it happens to equal the default.
    private var synced = false

    private const val CHANNEL = "hotspot"
    private const val NOTIFICATION_ID = 1

    fun init(context: Context) {
        app = context.applicationContext
        app.getSystemService(NotificationManager::class.java).createNotificationChannel(
            NotificationChannel(CHANNEL, app.getString(R.string.notification_channel), NotificationManager.IMPORTANCE_LOW)
        )
    }

    fun start(cfg: APConfig) {
        if (phase != Phase.IDLE || !cfg.isValid()) return
        enterPhase(Phase.STARTING)
        actionLogs.clear()
        scope.launch {
            if (!APManager.start(cfg, logger)) logFailure()
            settle()
        }
    }

    fun stop() {
        if (phase != Phase.IDLE) return
        enterPhase(Phase.STOPPING)
        actionLogs.clear()
        scope.launch {
            if (!APManager.stop(logger)) logFailure()
            settle()
        }
    }

    /** Re-read status; only a real change fans out to the tile and the
     *  notification, so the screen's 3 s poll cannot ping-pong with the tile. */
    suspend fun refresh(): APStatus {
        val s = APManager.getStatus()
        if (s != status || !synced) {
            synced = true
            status = s
            syncTile()
            syncNotification()
        }
        return s
    }

    // The backend needs a moment after exit before status reflects the new
    // state. Fans out unconditionally: after a stop from the notification in a
    // fresh process the new status equals the default and would not count as a change.
    private suspend fun settle() {
        delay(500)
        synced = true
        status = APManager.getStatus()
        enterPhase(Phase.IDLE)
        syncNotification()
    }

    private fun enterPhase(p: Phase) {
        phase = p
        syncTile()
    }

    private fun syncTile() {
        runCatching {
            TileService.requestListeningState(app, ComponentName(app, HotspotTileService::class.java))
        }
    }

    // ponytail: posted once per change, no process stays alive to watch hostapd.
    // If it dies while nobody opens QS or the app, this stays up until the next
    // refresh; Stop on a dead AP is harmless. Android 14 lets the user swipe it
    // away, which only hides it. Promote to a foreground service if that bites.
    private fun syncNotification() {
        val nm = app.getSystemService(NotificationManager::class.java)
        if (!status.running) {
            nm.cancel(NOTIFICATION_ID)
            return
        }
        val open = PendingIntent.getActivity(
            app, 0,
            Intent(app, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            PendingIntent.FLAG_IMMUTABLE
        )
        val stop = PendingIntent.getBroadcast(
            app, 1, Intent(app, HotspotStopReceiver::class.java), PendingIntent.FLAG_IMMUTABLE
        )
        val clients = status.clients
        val notification = NotificationCompat.Builder(app, CHANNEL)
            .setSmallIcon(R.drawable.ic_wifi_tethering)
            .setContentTitle(status.ssid ?: app.getString(R.string.app_name))
            .setContentText(app.resources.getQuantityString(R.plurals.notification_clients, clients, clients))
            .setContentIntent(open)
            .addAction(0, app.getString(R.string.stop_ap), stop)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .build()
        nm.notify(NOTIFICATION_ID, notification)
    }

    /** The script's own [ERROR] lines already explain most failures; this
     *  covers the exit code with no message (killed, missing binary). */
    private fun logFailure() {
        logger.logImmediate(Log.ERROR, app.getString(R.string.command_failed))
    }
}
