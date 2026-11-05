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
import com.virtualap.app.util.ApClient
import com.virtualap.app.util.Hotspot
import com.virtualap.app.util.NetworkIface
import com.virtualap.app.util.VirtualAPInstaller
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.launch

class APViewModel(application: Application) : AndroidViewModel(application) {
    /** Session state lives in [Hotspot] so the tile and the screen agree. */
    val status get() = Hotspot.status
    val phase get() = Hotspot.phase
    val actionLogs get() = Hotspot.actionLogs
    var config by mutableStateOf(APConfig())
    var interfaces by mutableStateOf<List<NetworkIface>>(emptyList())
        private set
    /** Running Droidspaces containers. A saved one that is not among them stays saved. */
    var containers by mutableStateOf<List<String>>(emptyList())
        private set
    /** Live answer from the backend; the toggle is greyed out while false. */
    var ttlFixSupported by mutableStateOf(false)
        private set
    /** The Wi-Fi's own MAC from the backend, what Static MAC starts from. Empty if unreadable. */
    var deviceMac by mutableStateOf("")
        private set
    /** Null until first read. Kept here so Settings opens already showing them, not animating to them. */
    var blocked by mutableStateOf<List<ApClient>?>(null)
        private set
    var allowed by mutableStateOf<List<ApClient>?>(null)
        private set
    var allowOnly by mutableStateOf<Boolean?>(null)
        private set
    var startAtBoot by mutableStateOf(false)
        private set
    var showActionLogs by mutableStateOf(false)
        private set
    /** False until the first status/interfaces/containers fetch completes, so
     *  the UI can show a spinner instead of flashing stale "stopped" state. */
    var isReady by mutableStateOf(false)
        private set

    private var loaded = false

    /**
     * Once the root and install gates have passed, and not from init: this view model exists
     * before them, where a read fails and leaves defaults, and the next valid edit would write
     * those over the real ap.conf.
     */
    fun load() {
        if (loaded) return
        loaded = true
        // Not part of the gate below: the main screen does not show them.
        viewModelScope.launch { refreshDeviceLists() }
        viewModelScope.launch { startAtBoot = VirtualAPInstaller.bootModeEnabled() }
        // One parallel initial load, gate the UI on it so everything appears at
        // once (no status flicker, no late-popping container toggle).
        viewModelScope.launch {
            val caps = async { APManager.getCaps() }
            APManager.loadConfig()?.let { config = it }
            // After the load, so the defaults never reach ap.conf. A newer value cancels the
            // wait, so typing saves once, after the last key, and only what is valid is saved.
            launch {
                snapshotFlow { config }.drop(1).collectLatest {
                    delay(500)
                    APManager.saveConfig(it)
                }
            }
            refreshAllNow()
            val c = caps.await()
            ttlFixSupported = c["ttl"] == "1"
            deviceMac = c["mac"].orEmpty()
            // A saved "on" from another kernel must not linger behind a greyed-out toggle.
            if (!ttlFixSupported && config.ttlFix) config = config.copy(ttlFix = false)
            isReady = true
        }
    }

    /** A failed call keeps what it has. */
    suspend fun refreshDeviceLists() {
        APManager.getBlocked()?.let { blocked = it }
        APManager.getAllowed()?.let { allowed = it }
        APManager.getAllowOnly()?.let { allowOnly = it }
    }

    suspend fun updateStartAtBoot(enabled: Boolean): Boolean {
        val written = VirtualAPInstaller.setBootMode(enabled)
        if (written) startAtBoot = enabled
        return written
    }

    /** Re-fetch status, interfaces and containers in parallel and suspend until
     *  they all land (so a pull-to-refresh spinner reflects real work). Root
     *  status is refreshed separately by the caller. */
    suspend fun refreshAllNow() = coroutineScope {
        val s = async { Hotspot.refresh() }
        val ifs = async { APManager.getInterfaces() }
        val cs = async { APManager.getContainers() }
        s.await()
        interfaces = ifs.await()
        // null: Droidspaces could not be asked this time; keep what we had.
        cs.await()?.let { containers = it }
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
