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
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch

class APViewModel(application: Application) : AndroidViewModel(application) {
    private val prefs = PreferencesManager

    /** Session state lives in [Hotspot] so the tile and the screen agree. */
    val status get() = Hotspot.status
    val phase get() = Hotspot.phase
    val actionLogs get() = Hotspot.actionLogs
    var config by mutableStateOf(prefs.apConfig)
    var interfaces by mutableStateOf<List<NetworkIface>>(emptyList())
        private set
    /** Running Droidspaces containers; empty = hide the integration UI entirely. */
    var containers by mutableStateOf<List<String>>(emptyList())
        private set
    /** Live answer from the backend; the toggle is greyed out while false. */
    var ttlFixSupported by mutableStateOf(false)
        private set
    /** The Wi-Fi's own MAC from the backend, what Static MAC starts from. Empty if unreadable. */
    var deviceMac by mutableStateOf("")
        private set
    var showActionLogs by mutableStateOf(false)
        private set
    /** False until the first status/interfaces/containers fetch completes, so
     *  the UI can show a spinner instead of flashing stale "stopped" state. */
    var isReady by mutableStateOf(false)
        private set

    init {
        viewModelScope.launch {
            snapshotFlow { config }.collect { prefs.apConfig = it }
        }
        // One parallel initial load, gate the UI on it so everything appears at
        // once (no status flicker, no late-popping container toggle).
        viewModelScope.launch {
            val caps = async { APManager.getCaps() }
            refreshAllNow()
            val c = caps.await()
            ttlFixSupported = c["ttl"] == "1"
            deviceMac = c["mac"].orEmpty()
            // A saved "on" from another kernel must not linger behind a greyed-out toggle.
            if (!ttlFixSupported && config.ttlFix) config = config.copy(ttlFix = false)
            isReady = true
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

    /** Re-fetch status, interfaces and containers in parallel and suspend until
     *  they all land (so a pull-to-refresh spinner reflects real work). Root
     *  status is refreshed separately by the caller. */
    suspend fun refreshAllNow() = coroutineScope {
        val s = async { Hotspot.refresh() }
        val ifs = async { APManager.getInterfaces() }
        val cs = async { APManager.getContainers() }
        s.await()
        applyInterfaceList(ifs.await())
        applyContainerList(cs.await())
    }

    /** Switch band: valid channels differ per band, so reset to Auto. A width
     *  the new band cannot carry (80 MHz on 2.4 GHz) goes back to Auto too. */
    fun selectBand(value: String) {
        config = config.copy(
            band = value, channel = "",
            width = APConfig.validWidthForBand(value, config.width)
        )
    }

    /** Picking a mode fills the MAC box: a fresh random MAC for Random once, the
     *  Wi-Fi's own for Static MAC. Picking the current mode again does nothing,
     *  so a kept MAC never changes by accident. */
    fun selectMacMode(mode: String) {
        if (mode == config.macMode) return
        config = config.copy(
            macMode = mode,
            gatewayMac = when (mode) {
                "persistent" -> APConfig.randomMac()
                "static" -> deviceMac
                else -> ""
            }
        )
    }

    fun start() {
        if (Hotspot.start(config) != null) showActionLogs = true
    }

    fun stop() {
        if (Hotspot.stop() != null) showActionLogs = true
    }

    fun clearLog() {
        Hotspot.actionLogs.clear()
    }

    fun openLogSheet() { showActionLogs = true }
    fun dismissActionLogs() { showActionLogs = false }
}
