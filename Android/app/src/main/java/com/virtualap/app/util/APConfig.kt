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
    val gatewayMac: String = "",     // the MAC persistent and static send
    val ip6Gateway: String = "",     // managed mode: the LAN's IPv6 gateway, blank = from ula
    val ula: String = randomUla()    // managed mode: the /48 the LAN's IPv6 /64 comes from
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
            (!containerMode || (containerName.isNotBlank() && macValid() && isValidUla(ula) &&
                (ip6Gateway.isBlank() || isValidIp6Gateway(ip6Gateway))))

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
        "MAC_MODE" to macMode,
        "IP6_GW" to ip6Gateway,
        "ULA" to ula
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
                gatewayMac = mac,
                ip6Gateway = str("IP6_GW", ""),
                // A file from before ULA gets a random one, kept from the next save on.
                ula = str("ULA", "").ifEmpty { defaults.ula }
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

        /** RFC 4193: fd, then 40 random bits, so no two hotspots share a prefix. */
        fun randomUla(): String {
            val b = Random.nextBytes(5)
            return "fd%02x:%02x%02x:%02x%02x::/48".format(b[0], b[1], b[2], b[3], b[4])
        }

        private val ulaRegex = Regex("^fd[0-9a-f]{2}(:[0-9a-f]{1,4}){2}::/48$", RegexOption.IGNORE_CASE)

        /**
         * A /48 in fd00::/8 as the engine takes it, and never Droidspaces' fd64:7370::/48, which
         * the container's own uplink already uses.
         */
        fun isValidUla(ula: String): Boolean =
            ulaRegex.matches(ula) &&
                ula.substringBefore("::").split(":").map { it.toInt(16) } != listOf(0xfd64, 0x7370, 0)

        /**
         * An IPv6 address's 16 bytes, or null. inet_pton's grammar less the dotted IPv4 tail,
         * which no gateway here needs. InetAddress would send whatever it cannot parse to DNS,
         * and the numeric-only parser needs API 29.
         */
        fun parseIpv6(text: String): ByteArray? {
            val halves = text.split("::")
            if (halves.size > 2) return null
            fun groups(s: String) = if (s.isEmpty()) emptyList() else s.split(":")
            val head = groups(halves[0])
            val tail = if (halves.size == 2) groups(halves[1]) else emptyList()
            // "::" stands for at least one zero group, and without it there are exactly eight.
            val zeros = 8 - head.size - tail.size
            if (if (halves.size == 2) zeros < 1 else zeros != 0) return null
            val all = head + List(zeros) { "0" } + tail
            if (all.any { g -> g.length !in 1..4 || g.any { it !in '0'..'9' && it.lowercaseChar() !in 'a'..'f' } })
                return null
            return all.flatMap { val v = it.toInt(16); listOf((v shr 8).toByte(), v.toByte()) }.toByteArray()
        }

        private val dsPrefix = parseIpv6("fd64:7370::")!!.copyOf(6)

        /**
         * The LAN's IPv6 gateway as the engine takes it: in 2000::/3 or fc00::/7, bare or with
         * /64, and not in Droidspaces' fd64:7370::/48, which the container's uplink uses.
         */
        fun isValidIp6Gateway(gw: String): Boolean {
            val b = parseIpv6(gw.removeSuffix("/64")) ?: return false
            val first = b[0].toInt() and 0xff
            return (first shr 5 == 1 || first shr 1 == 0x7e) && !b.copyOf(6).contentEquals(dsPrefix)
        }

        /** "fd12:3456:789a:42" for a gateway in x.x.42.x: the LAN's /64 as the engine numbers it. */
        fun ulaSubnet(ula: String, gateway: String): String =
            ula.substringBefore("::") + ":" + gateway.ifBlank { DEFAULT_GATEWAY }.split(".")[2]

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
