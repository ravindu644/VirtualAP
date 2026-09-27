package com.virtualap.app.util

import android.content.Context
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
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

    fun init(context: Context) {
        app = context.applicationContext
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

    /** Re-read status; only a real change fans out to the observers. */
    suspend fun refresh(): APStatus {
        val s = APManager.getStatus()
        if (s != status) {
            status = s
            onChanged()
        }
        return s
    }

    // The backend needs a moment after exit before status reflects the new state.
    private suspend fun settle() {
        delay(500)
        refresh()
        enterPhase(Phase.IDLE)
    }

    private fun enterPhase(p: Phase) {
        phase = p
        onChanged()
    }

    private fun onChanged() {}

    /** The script's own [ERROR] lines already explain most failures; this
     *  covers the exit code with no message (killed, missing binary). */
    private fun logFailure() {
        logger.logImmediate(Log.ERROR, app.getString(R.string.command_failed))
    }
}
