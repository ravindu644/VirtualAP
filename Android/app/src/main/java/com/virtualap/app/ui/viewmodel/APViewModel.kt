package com.virtualap.app.ui.viewmodel

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import androidx.compose.runtime.snapshotFlow
import com.virtualap.app.util.APConfig
import com.virtualap.app.util.APManager
import com.virtualap.app.util.Hotspot
import com.virtualap.app.util.NetworkIface
import com.virtualap.app.util.PreferencesManager
import com.virtualap.app.util.classifyLine
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

class APViewModel(application: Application) : AndroidViewModel(application) {
    private val prefs = PreferencesManager.getInstance(application)

    /** Session state lives in [Hotspot] so the tile and the screen agree. */
    val status get() = Hotspot.status
    val phase get() = Hotspot.phase
    val actionLogs get() = Hotspot.actionLogs
    var config by mutableStateOf(APConfig.fromPrefs(prefs))
    var interfaces by mutableStateOf<List<NetworkIface>>(emptyList())
        private set
    /** Running Droidspaces containers; empty = hide the integration UI entirely. */
    var containers by mutableStateOf<List<String>>(emptyList())
        private set
    var logText by mutableStateOf("")
        private set
    /** The tailed ap.log as log lines, for the sheet when no command output is live. */
    val fallbackLogs: List<Pair<Int, String>>
        get() = if (logText.isBlank()) emptyList() else logText.lines().map { classifyLine(it) to it }
    var showActionLogs by mutableStateOf(false)
        private set
    /** False until the first status/interfaces/containers fetch completes, so
     *  the UI can show a spinner instead of flashing stale "stopped" state. */
    var isReady by mutableStateOf(false)
        private set

    private var pollJob: Job? = null

    init {
        viewModelScope.launch {
            snapshotFlow { config }.collect { cfg ->
                prefs.saveApConfig(
                    cfg.ssid, cfg.password, cfg.band, cfg.channel, cfg.width,
                    cfg.upstream, cfg.gateway, cfg.dnsServers, cfg.hidden,
                    cfg.security, cfg.pmf, cfg.containerMode, cfg.containerName
                )
            }
        }
        // One parallel initial load, gate the UI on it so everything appears at
        // once (no status flicker, no late-popping container toggle).
        viewModelScope.launch {
            val s = async { Hotspot.refresh() }
            val ifs = async { APManager.getInterfaces() }
            val cs = async { APManager.getContainers() }
            s.await()
            applyInterfaceList(ifs.await())
            applyContainerList(cs.await())
            logText = APManager.readLog()
            isReady = true
            startPolling()
        }
    }

    private fun applyContainerList(list: List<String>) {
        containers = list
        // If the saved container vanished (stopped / Droidspaces gone), drop
        // managed mode so we never send a stale -K to the backend.
        val cfg = config
        if (cfg.containerMode && cfg.containerName !in list) {
            config = cfg.copy(containerMode = false, containerName = "")
        }
    }

    private fun applyInterfaceList(ifaces: List<NetworkIface>) {
        interfaces = ifaces
        // If the saved upstream is gone (e.g. WireGuard tunnel stopped), reset to
        // auto so a stale iface name isn't sent to the backend.
        if (config.upstream != "auto" && ifaces.none { it.name == config.upstream }) {
            config = config.copy(upstream = "auto")
        }
    }

    /** Pull-to-refresh: re-fetch status, interfaces, containers and log in
     *  parallel and suspend until they all land (so the spinner reflects real
     *  work). Root status is refreshed separately by the caller. */
    suspend fun refreshAllNow() = coroutineScope {
        val s = async { Hotspot.refresh() }
        val ifs = async { APManager.getInterfaces() }
        val cs = async { APManager.getContainers() }
        s.await()
        applyInterfaceList(ifs.await())
        applyContainerList(cs.await())
        logText = APManager.readLog()
    }

    private fun startPolling() {
        pollJob?.cancel()
        pollJob = viewModelScope.launch {
            while (isActive) {
                delay(3000)   // initial state already loaded; poll afterwards
                refreshStatus()
                refreshLog()
            }
        }
    }

    fun refreshStatus() {
        viewModelScope.launch { Hotspot.refresh() }
    }

    /** Switch band: valid channels differ per band, so reset to Auto. Width is
     *  left to the user; the backend downgrades any unsupported width safely. */
    fun selectBand(value: String) {
        config = config.copy(band = value, channel = "")
    }

    fun selectChannel(value: String) {
        config = config.copy(channel = value)
    }

    fun selectWidth(value: String) {
        config = config.copy(width = value)
    }

    fun selectSecurity(value: String) {
        config = config.copy(security = value)
    }

    fun setPmf(value: Boolean) {
        config = config.copy(pmf = value)
    }

    /** Open networks have no passphrase field; WPA modes show one. */
    fun passwordRequired(): Boolean = config.security != "open"

    private fun refreshLog() {
        viewModelScope.launch {
            logText = APManager.readLog()
        }
    }

    fun start() {
        logText = ""
        showActionLogs = true
        Hotspot.start(config)
    }

    fun stop() {
        logText = ""
        showActionLogs = true
        Hotspot.stop()
    }

    fun clearLog() {
        viewModelScope.launch {
            APManager.clearLog()
            logText = ""
            Hotspot.actionLogs.clear()
        }
    }

    fun openLogSheet() { showActionLogs = true }
    fun dismissActionLogs() { showActionLogs = false }
}
