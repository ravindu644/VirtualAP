package com.virtualap.app.util

import android.util.Log
import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

data class APStatus(
    val running: Boolean = false,
    val hostapd: String = "dead",
    val dnsmasq: String = "dead",
    val gateway: String = "192.168.42.1",
    val clients: Int = 0,
    val ssid: String? = null,
    val band: String? = null,
    val channel: String? = null,
    val upstream: String? = null,
    val upstreamIface: String? = null,
    val upstreamTable: String? = null,
    val started: String? = null
)

data class NetworkIface(val name: String, val ip: String?)


object APManager {
    suspend fun getStatus(): APStatus = withContext(Dispatchers.IO) {
        val result = Shell.cmd("${Constants.START_AP} status 2>/dev/null").exec()
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
            clients = kv["clients"]?.toIntOrNull() ?: 0,
            ssid = kv["ssid"],
            band = kv["band"],
            channel = kv["channel"],
            upstream = kv["upstream"],
            upstreamIface = kv["upstream_iface"],
            upstreamTable = kv["upstream_table"],
            started = kv["started"]
        )
    }

    suspend fun start(
        ssid: String, password: String, upstream: String,
        band: String, channel: String?,
        onLine: (Int, String) -> Unit
    ): Boolean = withContext(Dispatchers.IO) {
        val sq = { s: String -> "'" + s.replace("'", "'\\''") + "'" }
        var cmd = "${Constants.START_AP} start -s ${sq(ssid)} -p ${sq(password)} -o ${sq(upstream)} -b ${sq(band)}"
        if (!channel.isNullOrBlank()) cmd += " -c ${sq(channel)}"

        val outputList = object : com.topjohnwu.superuser.CallbackList<String>() {
            override fun onAddElement(e: String?) {
                e ?: return
                val level = when {
                    e.contains("[ERROR]") -> Log.ERROR
                    e.contains("[WARN]")  -> Log.WARN
                    else                  -> Log.INFO
                }
                onLine(level, e)
            }
        }
        val result = Shell.cmd(cmd).to(outputList).exec()
        result.isSuccess
    }

    suspend fun stop(onLine: (Int, String) -> Unit): Boolean = withContext(Dispatchers.IO) {
        val outputList = object : com.topjohnwu.superuser.CallbackList<String>() {
            override fun onAddElement(e: String?) { e?.let { onLine(Log.INFO, it) } }
        }
        Shell.cmd("${Constants.START_AP} stop").to(outputList).exec().isSuccess
    }


    suspend fun getInterfaces(): List<NetworkIface> = withContext(Dispatchers.IO) {
        val result = Shell.cmd("${Constants.START_AP} interfaces 2>/dev/null").exec()
        if (!result.isSuccess) return@withContext emptyList()
        result.out.mapNotNull { line ->
            val parts = line.trim().split(":", limit = 2)
            if (parts.isEmpty() || parts[0].isBlank()) null
            else NetworkIface(parts[0].trim(), parts.getOrNull(1)?.takeIf { it != "no-ip" })
        }
    }

    suspend fun readLog(lines: Int = 150): String = withContext(Dispatchers.IO) {
        val result = Shell.cmd("tail -n $lines ${Constants.LOG_FILE} 2>/dev/null").exec()
        result.out.joinToString("\n")
    }

    suspend fun clearLog() = withContext(Dispatchers.IO) {
        Shell.cmd(": > ${Constants.LOG_FILE}").exec()
    }



    suspend fun isInstalled(): Boolean = withContext(Dispatchers.IO) {
        Shell.cmd("test -x ${Constants.VAP_DIR}/start-ap && echo ok").exec().out.any { it.contains("ok") }
    }
}
