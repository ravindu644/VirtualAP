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
    val dnsServers: String? = null,
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
            upstream = kv["upstream"],
            upstreamIface = kv["upstream_iface"],
            upstreamTable = kv["upstream_table"],
            started = kv["started"]
        )
    }

    suspend fun start(
        ssid: String, password: String, upstream: String,
        band: String, channel: String?, gateway: String, dnsServers: String?,
        hidden: Boolean = false,
        onLine: (Int, String) -> Unit
    ): Boolean = withContext(Dispatchers.IO) {
        val sq = { s: String -> "'" + s.replace("'", "'\\''") + "'" }
        val channelVal = channel ?: ""
        val dnsVal = dnsServers ?: ""
        val hiddenVal = if (hidden) "1" else "0"
        val cmd = "${Backend.startAp} start -s ${sq(ssid)} -p ${sq(password)} -o ${sq(upstream)} -b ${sq(band)} -c ${sq(channelVal)} -g ${sq(gateway)} -d ${sq(dnsVal)} -H $hiddenVal"

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
        Shell.cmd("${Backend.startAp} stop").to(outputList).exec().isSuccess
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
        val result = Shell.cmd("tail -n $lines ${Constants.LOG_FILE} 2>/dev/null").exec()
        result.out.joinToString("\n")
    }

    suspend fun clearLog() = withContext(Dispatchers.IO) {
        Shell.cmd(": > ${Constants.LOG_FILE}").exec()
    }



    suspend fun isInstalled(): Boolean = withContext(Dispatchers.IO) {
        // Scripts run from app files now; "installed" means the root-owned
        // payload is in place: busybox + an extracted Alpine rootfs.
        Shell.cmd(
            "test -x ${Constants.BUSYBOX} && test -f ${Constants.VAP_DIR}/rootfs/etc/alpine-release && echo ok"
        ).exec().out.any { it.contains("ok") }
    }
}
