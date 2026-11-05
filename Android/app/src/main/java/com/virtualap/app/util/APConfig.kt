package com.virtualap.app.util

import kotlin.random.Random

/** The hotspot configuration: what the UI edits, and ap.conf once it is valid. */
data class APConfig(
    val phyIface: String = "wlan0",  // no field in the UI, kept for a hand edit
    val ssid: String = "",
    val password: String = "",
    val band: String = "2",
    val channel: String = "",
    val width: String = "auto",
    val upstream: String = "auto",
    val gateway: String = "",        // blank = DEFAULT_GATEWAY
    val dnsServers: String = "",
    val hidden: Boolean = false,
    val security: String = "wpa2",   // open | wpa2 | wpa2wpa3 | wpa3
    val pmf: Boolean = false,        // Protected Management Frames (wpa2 only)
    val ttlFix: Boolean = false,     // forwarded packets leave with TTL 64
    val ipv6: Boolean = true,        // IPv6 for clients: NAT66, or the upstream's /64
    val containerMode: Boolean = false,
    val containerName: String = "",
    val macMode: String = "random",  // random | persistent | static, managed mode only
    val gatewayMac: String = ""      // the MAC persistent and static send
) {
    /** Open needs no passphrase; WPA (WPA-PSK/SAE) passphrases are 8-63 bytes, as the engine counts. */
    fun passwordValid(): Boolean = security == "open" || password.toByteArray().size in 8..63

    /** 802.11 gives a network name 32 bytes, which Sinhala fills in about 10 letters. */
    fun ssidFits(): Boolean = ssid.toByteArray().size <= 32

    /** Random mode sends no MAC; the other two need one the kernel will take. */
    fun macValid(): Boolean = macMode == "random" || isValidMac(gatewayMac)

    /** Everything the engine will reject on its own, checked before we shell out. */
    fun isValid(): Boolean =
        ssid.isNotBlank() && ssidFits() && passwordValid() && (ssid + password).none { it == '\n' || it == '\r' } &&
            (gateway.isBlank() || isValidIpv4(gateway)) && isValidDnsList(dnsServers) &&
            (!containerMode || (containerName.isNotBlank() && macValid()))

    /**
     * ap.conf, KEY='value' per line. The engine and the boot module read it; MAC_MODE is the
     * app's own, and the engine skips a key it does not know.
     */
    fun toConf(): String = listOf(
        "PHY_IFACE" to phyIface,
        "UPSTREAM" to upstream,
        "SSID" to ssid,
        "PASSWORD" to password,
        "BAND" to band,
        "CHANNEL" to channel,
        "WIDTH" to width,
        "IP_GW" to gateway,
        "DNS_SERVERS" to dnsServers,
        "HIDDEN" to hidden.flag(),
        "SECURITY" to security,
        "PMF" to pmf.flag(),
        "TTL_FIX" to ttlFix.flag(),
        "IPV6" to ipv6.flag(),
        "CONTAINER" to if (containerMode) containerName else "",
        "GW_MAC" to if (macMode == "random") "" else gatewayMac,
        "MAC_MODE" to macMode
    ).joinToString("") { (key, value) -> "$key=${Backend.quote(value)}\n" }

    companion object {
        private fun Boolean.flag() = if (this) "1" else "0"

        /** The reverse of [toConf]. A channel or width the band cannot carry comes back as auto. */
        fun fromConf(lines: List<String>): APConfig {
            val conf = lines.filter { '=' in it }.associate {
                it.substringBefore('=') to it.substringAfter('=').removeSurrounding("'").replace("'\\''", "'")
            }
            val defaults = APConfig()
            fun str(key: String, default: String) = conf[key] ?: default
            fun bool(key: String, default: Boolean) = conf[key]?.let { it == "1" } ?: default
            val band = str("BAND", defaults.band)
            val container = str("CONTAINER", "")
            val mac = str("GW_MAC", "")
            return APConfig(
                phyIface = str("PHY_IFACE", defaults.phyIface),
                upstream = str("UPSTREAM", defaults.upstream),
                ssid = str("SSID", ""),
                password = str("PASSWORD", ""),
                band = band,
                channel = validChannelForBand(band, str("CHANNEL", "")),
                width = validWidthForBand(band, str("WIDTH", defaults.width)),
                // Blank shows the default as a hint, and the engine writes the default out.
                gateway = str("IP_GW", "").takeUnless { it == DEFAULT_GATEWAY } ?: "",
                dnsServers = str("DNS_SERVERS", ""),
                hidden = bool("HIDDEN", defaults.hidden),
                security = str("SECURITY", defaults.security),
                pmf = bool("PMF", defaults.pmf),
                ttlFix = bool("TTL_FIX", defaults.ttlFix),
                ipv6 = bool("IPV6", defaults.ipv6),
                containerMode = container.isNotEmpty(),
                containerName = container,
                // A file from before MAC_MODE: a kept MAC is what persistent means.
                macMode = str("MAC_MODE", if (mac.isEmpty()) "random" else "persistent"),
                gatewayMac = mac
            )
        }

        /** Default AP/LAN gateway when the gateway field is left blank. */
        const val DEFAULT_GATEWAY = "192.168.42.1"

        /** A host address as the engine takes it: four octets as inet_pton reads them, the last 1 to 254. */
        fun isValidIpv4(ip: String): Boolean {
            val octets = ip.trim().split(".")
            return octets.size == 4 &&
                octets.all { it.isNotEmpty() && it.all { c -> c in '0'..'9' } && it.length <= 3 && it.toInt() <= 255 && (it.length == 1 || it[0] != '0') } &&
                octets[3].toInt() in 1..254
        }

        /** Blank means the backend's default resolvers; otherwise every comma-separated entry is an address. */
        fun isValidDnsList(dns: String): Boolean =
            dns.isBlank() || dns.split(",").all { isValidIpv4(it) }

        private val macRegex = Regex("^[0-9A-Fa-f]{2}(:[0-9A-Fa-f]{2}){5}$")

        /** Unicast and not all zeros, the two the kernel refuses on a veth. */
        fun isValidMac(mac: String): Boolean =
            macRegex.matches(mac) && (mac.substring(0, 2).toInt(16) and 1) == 0 &&
                mac.any { it != '0' && it != ':' }

        /**
         * Locally administered and unicast: the 02 bit keeps it out of every
         * vendor's range, and the kernel refuses a multicast one.
         */
        fun randomMac(): String {
            val b = Random.nextBytes(6)
            b[0] = (b[0].toInt() and 0xfc or 0x02).toByte()
            return b.joinToString(":") { "%02x".format(it) }
        }

        /** Widths the band can carry. 80 MHz only exists on 5 GHz. */
        fun widthsForBand(band: String): List<String> =
            if (band == "5") listOf("auto", "20", "40", "80") else listOf("auto", "20", "40")

        fun validWidthForBand(band: String, width: String): String =
            if (width in widthsForBand(band)) width else "auto"

        /** The channels the app offers per band; the backend follows the station anyway. */
        fun channelsForBand(band: String): List<String> =
            if (band == "5") listOf("36", "40", "44", "48", "149", "153", "157", "161", "165")
            else (1..11).map { "$it" }

        fun validChannelForBand(band: String, channel: String): String =
            if (channel in channelsForBand(band)) channel else ""
    }
}
