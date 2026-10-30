package com.virtualap.app.util

import kotlin.random.Random

/** The saved hotspot configuration, as the UI edits it and the tile reads it back. */
data class APConfig(
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
    val ipv6: Boolean = true,        // global IPv6 for clients from the upstream's /64 (NDP proxy)
    val containerMode: Boolean = false,
    val containerName: String = "",
    val macMode: String = "random",  // random | persistent | static, managed mode only
    val gatewayMac: String = ""      // the MAC persistent and static send
) {
    /** Open needs no passphrase; WPA (WPA-PSK/SAE) passphrases are 8-63 chars. */
    fun passwordValid(): Boolean = security == "open" || password.length in 8..63

    /** Random mode sends no MAC; the other two need one the kernel will take. */
    fun macValid(): Boolean = macMode == "random" || isValidMac(gatewayMac)

    /** Everything start-ap will reject on its own, checked before we shell out. */
    fun isValid(): Boolean =
        ssid.isNotBlank() && passwordValid() &&
            (gateway.isBlank() || isValidIpv4(gateway)) && isValidDnsList(dnsServers) &&
            (!containerMode || (containerName.isNotBlank() && macValid()))

    companion object {
        /** Default AP/LAN gateway when the gateway field is left blank. */
        const val DEFAULT_GATEWAY = "192.168.42.1"

        /** A host address as start-ap takes it: four octets as inet_pton reads them, the last 1 to 254. */
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
