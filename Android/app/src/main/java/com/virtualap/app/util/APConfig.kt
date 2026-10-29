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
            (!containerMode || (containerName.isNotBlank() && macValid()))

    companion object {
        /** Default AP/LAN gateway when the gateway field is left blank. */
        const val DEFAULT_GATEWAY = "192.168.42.1"

        fun fromPrefs(prefs: PreferencesManager) = APConfig(
            ssid = prefs.apSsid,
            password = prefs.apPassword,
            band = prefs.apBand,
            channel = validChannelForBand(prefs.apBand, prefs.apChannel),
            width = validWidthForBand(prefs.apBand, prefs.apWidth),
            upstream = prefs.apUpstream,
            // Normalize a legacy stored default to blank so it shows as a hint.
            gateway = prefs.apGateway.takeUnless { it == DEFAULT_GATEWAY } ?: "",
            dnsServers = prefs.apDnsServers,
            hidden = prefs.apHidden,
            security = prefs.apSecurity,
            pmf = prefs.apPmf,
            ttlFix = prefs.apTtlFix,
            ipv6 = prefs.apIpv6,
            containerMode = prefs.apContainerMode,
            containerName = prefs.apContainer,
            macMode = prefs.apMacMode,
            gatewayMac = prefs.apGatewayMac
        )

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
