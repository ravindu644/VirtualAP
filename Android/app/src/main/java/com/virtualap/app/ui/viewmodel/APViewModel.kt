package com.virtualap.app.ui.viewmodel

import android.app.Application
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import androidx.compose.runtime.snapshotFlow
import com.virtualap.app.R
import com.virtualap.app.util.APManager
import com.virtualap.app.util.APStatus
import com.virtualap.app.util.NetworkIface
import com.virtualap.app.util.PreferencesManager
import com.virtualap.app.util.ViewModelLogger
import com.virtualap.app.util.classifyLine
import kotlinx.coroutines.Job
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

data class APConfig(
    val ssid: String = "",
    val password: String = "",
    val band: String = "2",
    val channel: String = "",
    val width: String = "auto",
    val upstream: String = "auto",
    val gateway: String = "",        // blank = APViewModel.DEFAULT_GATEWAY
    val dnsServers: String = "",
    val hidden: Boolean = false,
    val security: String = "wpa2",   // open | wpa2 | wpa2wpa3 | wpa3
    val pmf: Boolean = false,        // Protected Management Frames (wpa2 only)
    val containerMode: Boolean = false,
    val containerName: String = ""
)

class APViewModel(application: Application) : AndroidViewModel(application) {
    private val prefs = PreferencesManager.getInstance(application)

    var status by mutableStateOf(APStatus())
        private set
    var config by mutableStateOf(
        APConfig(
            ssid = prefs.apSsid,
            password = prefs.apPassword,
            band = prefs.apBand,
            channel = validChannelForBand(prefs.apBand, prefs.apChannel),
            width = prefs.apWidth,
            upstream = prefs.apUpstream,
            // Normalize a legacy stored default to blank so it shows as a hint.
            gateway = prefs.apGateway.takeUnless { it == DEFAULT_GATEWAY } ?: "",
            dnsServers = prefs.apDnsServers,
            hidden = prefs.apHidden,
            security = prefs.apSecurity,
            pmf = prefs.apPmf,
            containerMode = prefs.apContainerMode,
            containerName = prefs.apContainer
        )
    )
    var interfaces by mutableStateOf<List<NetworkIface>>(emptyList())
        private set
    /** Running Droidspaces containers; empty = hide the integration UI entirely. */
    var containers by mutableStateOf<List<String>>(emptyList())
        private set
    var isStarting by mutableStateOf(false)
        private set
    var isStopping by mutableStateOf(false)
        private set
    var logText by mutableStateOf("")
        private set
    val actionLogs = mutableStateListOf<Pair<Int, String>>()
    private val logger = ViewModelLogger { level, msg -> actionLogs.add(level to msg) }
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
            val s = async { APManager.getStatus() }
            val ifs = async { APManager.getInterfaces() }
            val cs = async { APManager.getContainers() }
            status = s.await()
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
        val s = async { APManager.getStatus() }
        val ifs = async { APManager.getInterfaces() }
        val cs = async { APManager.getContainers() }
        status = s.await()
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
        viewModelScope.launch {
            val s = APManager.getStatus()
            status = s
        }
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

    /** Open needs no passphrase; WPA (WPA-PSK/SAE) passphrases are 8-63 chars. */
    fun passwordValid(): Boolean =
        config.security == "open" || config.password.length in 8..63

    private fun refreshLog() {
        viewModelScope.launch {
            logText = APManager.readLog()
        }
    }

    fun start() {
        val cfg = config
        if (cfg.ssid.isBlank()) return
        if (!passwordValid()) return
        if (cfg.containerMode && cfg.containerName.isBlank()) return
        viewModelScope.launch {
            isStarting = true
            actionLogs.clear()
            logText = ""
            showActionLogs = true
            val ok = APManager.start(
                cfg.ssid, cfg.password, cfg.upstream, cfg.band,
                cfg.channel.takeIf { it.isNotBlank() },
                cfg.width,
                cfg.gateway.ifBlank { DEFAULT_GATEWAY }, cfg.dnsServers.takeIf { it.isNotBlank() },
                cfg.hidden,
                cfg.security, cfg.pmf,
                if (cfg.containerMode) cfg.containerName else "",
                logger
            )
            if (!ok) logFailure()
            delay(500)
            refreshStatus()
            isStarting = false
        }
    }

    fun stop() {
        viewModelScope.launch {
            isStopping = true
            actionLogs.clear()
            logText = ""
            showActionLogs = true
            if (!APManager.stop(logger)) logFailure()
            delay(500)
            refreshStatus()
            isStopping = false
        }
    }

    /** The script's own [ERROR] lines already explain most failures; this
     *  covers the exit code with no message (killed, missing binary). */
    private fun logFailure() {
        logger.logImmediate(Log.ERROR, getApplication<Application>().getString(R.string.command_failed))
    }

    fun clearLog() {
        viewModelScope.launch {
            APManager.clearLog()
            logText = ""
            actionLogs.clear()
        }
    }

    fun openLogSheet() { showActionLogs = true }
    fun dismissActionLogs() { showActionLogs = false }

    companion object {
        /** Default AP/LAN gateway when the gateway field is left blank. */
        const val DEFAULT_GATEWAY = "192.168.42.1"

        // Must be companion (not instance method): called from property initializer
        // before the instance exists.
        fun validChannelForBand(band: String, channel: String): String {
            if (channel.isBlank()) return ""
            val valid = if (band == "5") {
                setOf("36", "40", "44", "48", "149", "153", "157", "161", "165")
            } else {
                (1..11).map { "$it" }.toSet()
            }
            return if (channel in valid) channel else ""
        }
    }
}
