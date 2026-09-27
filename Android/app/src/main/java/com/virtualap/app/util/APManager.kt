package com.virtualap.app.util

import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

data class APStatus(
    val running: Boolean = false,
    val hostapd: String = "dead",
    val dnsmasq: String = "dead",
    val gateway: String = "192.168.42.1",
    val dnsServers: String? = null,
    val clients: Int = 0,
    val ssid: String? = null,
    val band: String? = null,
    val channel: String? = null,
    val width: String? = null,          // actual channel width in MHz while running
    val security: String? = null,       // open | wpa2 | wpa2wpa3 | wpa3 while running
    val upstream: String? = null,
    val upstreamIface: String? = null,
    val upstreamTable: String? = null,
    val started: String? = null,
    val mode: String = "routed",        // "routed" or "bridged" (container-managed)
    val container: String? = null       // target container name when bridged
)

data class NetworkIface(val name: String, val ip: String?)


object APManager {
    suspend fun getStatus(): APStatus = withContext(Dispatchers.IO) {
        val result = Shell.cmd("${Backend.startAp} status 2>/dev/null").exec()
        if (!result.isSuccess) return@withContext APStatus()
        parseStatus(result.out.joinToString("\n"))
    }

    private fun parseStatus(text: String): APStatus {
        val kv = mutableMapOf<String, String>()
        text.split("\n").forEach { line ->
            val i = line.indexOf('=')
            if (i > 0) kv[line.substring(0, i).trim()] = line.substring(i + 1).trim()
        }
        return APStatus(
            running = kv["running"] == "1",
            hostapd = kv["hostapd"] ?: "dead",
            dnsmasq = kv["dnsmasq"] ?: "dead",
            gateway = kv["gateway"] ?: "192.168.42.1",
            dnsServers = kv["dns_servers"],
            clients = kv["clients"]?.toIntOrNull() ?: 0,
            ssid = kv["ssid"],
            band = kv["band"],
            channel = kv["channel"],
            width = kv["width"],
            security = kv["security"],
            upstream = kv["upstream"],
            upstreamIface = kv["upstream_iface"],
            upstreamTable = kv["upstream_table"],
            started = kv["started"],
            mode = kv["mode"] ?: "routed",
            container = kv["container"]
        )
    }

    suspend fun start(cfg: APConfig, logger: BackendLogger): Boolean = withContext(Dispatchers.IO) {
        val sq = Backend::quote
        val hiddenVal = if (cfg.hidden) "1" else "0"
        val pmfVal = if (cfg.pmf) "1" else "0"
        val gateway = cfg.gateway.ifBlank { APConfig.DEFAULT_GATEWAY }
        // -K is always passed (empty clears managed mode) so a stale CONTAINER
        // in ap.conf never silently re-enables it.
        val container = if (cfg.containerMode) cfg.containerName else ""
        val cmd = "${Backend.startAp} start -s ${sq(cfg.ssid)} -p ${sq(cfg.password)} -o ${sq(cfg.upstream)} -b ${sq(cfg.band)} -c ${sq(cfg.channel)} -W ${sq(cfg.width)} -g ${sq(gateway)} -d ${sq(cfg.dnsServers)} -H $hiddenVal -A ${sq(cfg.security)} -M $pmfVal -K ${sq(container)}"

        Shell.cmd(cmd).to(logSink(logger)).exec().isSuccess
    }

    suspend fun stop(logger: BackendLogger): Boolean = withContext(Dispatchers.IO) {
        Shell.cmd("${Backend.startAp} stop").to(logSink(logger)).exec().isSuccess
    }

    /** libsu delivers each output line on the main thread, so the logger's
     *  synchronous path is the right one here. */
    private fun logSink(logger: BackendLogger) = object : com.topjohnwu.superuser.CallbackList<String>() {
        override fun onAddElement(e: String?) { e?.let { logger.logImmediate(classifyLine(it), it) } }
    }


    /**
     * Running Droidspaces containers (names). Empty when Droidspaces isn't
     * installed or nothing is running - the UI uses this to decide whether to
     * show the container-integration option at all.
     */
    suspend fun getContainers(): List<String> = withContext(Dispatchers.IO) {
        val result = Shell.cmd("${Backend.startAp} containers 2>/dev/null").exec()
        if (!result.isSuccess) return@withContext emptyList()
        result.out.map { it.trim() }.filter { it.isNotEmpty() }
    }

    suspend fun getInterfaces(): List<NetworkIface> = withContext(Dispatchers.IO) {
        val result = Shell.cmd("${Backend.startAp} interfaces 2>/dev/null").exec()
        if (!result.isSuccess) return@withContext emptyList()
        result.out.mapNotNull { line ->
            val parts = line.trim().split(":", limit = 2)
            if (parts.isEmpty() || parts[0].isBlank()) null
            else NetworkIface(parts[0].trim(), parts.getOrNull(1)?.takeIf { it != "no-ip" })
        }
    }

    suspend fun readLog(lines: Int = 150): String = withContext(Dispatchers.IO) {
        val result = Shell.cmd("${Constants.BUSYBOX} tail -n $lines ${Constants.LOG_FILE} 2>/dev/null").exec()
        result.out.joinToString("\n")
    }

    suspend fun clearLog() = withContext(Dispatchers.IO) {
        Shell.cmd(": > ${Constants.LOG_FILE}").exec()
    }



    /** Every static binary the backend needs, under VAP_DIR/bin. */
    private val REQUIRED_BINARIES = listOf("busybox", "hostapd", "hostapd_cli", "iw", "dnsmasq")

    /**
     * "Installed" means every required binary is present AND executable. If even
     * one is missing (e.g. the user wiped /data/local/virtualap) this returns
     * false so the setup flow re-deploys the payload.
     */
    suspend fun isInstalled(): Boolean = withContext(Dispatchers.IO) {
        val checks = REQUIRED_BINARIES.joinToString(" && ") { "test -x ${Constants.VAP_DIR}/bin/$it" }
        Shell.cmd("$checks && echo ok").exec().out.any { it.contains("ok") }
    }
}
