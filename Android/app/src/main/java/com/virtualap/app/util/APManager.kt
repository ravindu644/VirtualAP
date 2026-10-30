package com.virtualap.app.util

import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

data class APStatus(
    val running: Boolean = false,
    val gateway: String = APConfig.DEFAULT_GATEWAY,
    val dnsServers: String? = null,
    val clients: Int = 0,
    val ssid: String? = null,
    val band: String? = null,
    val channel: String? = null,
    val width: String? = null,          // actual channel width in MHz while running
    val security: String? = null,       // open | wpa2 | wpa2wpa3 | wpa3 while running
    val upstream: String? = null,
    val started: String? = null,
    val mode: String = "routed",        // "routed" or "bridged" (container-managed)
    val container: String? = null,      // target container name when bridged
    val gatewayMac: String? = null      // the MAC the clients' gateway has, when bridged
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
            gateway = kv["gateway"] ?: APConfig.DEFAULT_GATEWAY,
            dnsServers = kv["dns_servers"],
            clients = kv["clients"]?.toIntOrNull() ?: 0,
            ssid = kv["ssid"],
            band = kv["band"],
            channel = kv["channel"],
            width = kv["width"],
            security = kv["security"],
            upstream = kv["upstream"],
            started = kv["started"],
            mode = kv["mode"] ?: "routed",
            container = kv["container"],
            gatewayMac = kv["gw_mac"]?.takeIf { it.isNotEmpty() }
        )
    }

    private fun Boolean.flag() = if (this) "1" else "0"

    suspend fun start(cfg: APConfig, logger: BackendLogger): Boolean = withContext(Dispatchers.IO) {
        val sq = Backend::quote
        // -K and -m are always passed (empty clears them) so a stale CONTAINER
        // or GW_MAC in ap.conf never silently comes back. An empty -g is the
        // script's default gateway.
        val container = if (cfg.containerMode) cfg.containerName else ""
        val mac = if (cfg.macMode == "random") "" else cfg.gatewayMac
        val cmd = "${Backend.startAp} start -s ${sq(cfg.ssid)} -p ${sq(cfg.password)} -o ${sq(cfg.upstream)} -b ${sq(cfg.band)} -c ${sq(cfg.channel)} -W ${sq(cfg.width)} -g ${sq(cfg.gateway)} -d ${sq(cfg.dnsServers)} -H ${cfg.hidden.flag()} -A ${sq(cfg.security)} -M ${cfg.pmf.flag()} -K ${sq(container)} -T ${cfg.ttlFix.flag()} -6 ${cfg.ipv6.flag()} -m ${sq(mac)}"

        Shell.cmd(cmd).to(logSink(logger)).exec().isSuccess
    }

    suspend fun stop(logger: BackendLogger): Boolean = withContext(Dispatchers.IO) {
        Shell.cmd("${Backend.startAp} stop").to(logSink(logger)).exec().isSuccess
    }

    /** libsu delivers each output line on the main thread, so the logger's
     *  synchronous path is the right one here. */
    private fun logSink(logger: BackendLogger) = object : com.topjohnwu.superuser.CallbackList<String>() {
        override fun onAddElement(e: String?) { e?.let { logger.log(classifyLine(it), it) } }
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

    /**
     * The backend's `caps` lines as a map. ttl=1 when this kernel and its
     * iptables can rewrite the TTL of forwarded packets; the backend finds out
     * by trying the rule, so it is the live answer and not something read from
     * a kernel config. mac is the Wi-Fi's own MAC, empty when it cannot be read.
     */
    suspend fun getCaps(): Map<String, String> = withContext(Dispatchers.IO) {
        Shell.cmd("${Backend.startAp} caps 2>/dev/null").exec().out
            .map { it.trim().split("=", limit = 2) }
            .filter { it.size == 2 }
            .associate { it[0] to it[1] }
    }

    suspend fun getInterfaces(): List<NetworkIface> = withContext(Dispatchers.IO) {
        val result = Shell.cmd("${Backend.startAp} interfaces 2>/dev/null").exec()
        if (!result.isSuccess) return@withContext emptyList()
        result.out.mapNotNull { line ->
            val parts = line.trim().split(":", limit = 2)
            if (parts[0].isBlank()) null
            else NetworkIface(parts[0].trim(), parts.getOrNull(1)?.takeIf { it != "no-ip" })
        }
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
