package com.virtualap.app.util

import android.content.Context
import android.os.Build
import com.virtualap.app.R
import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Deploys the static payload (virtualap, the engine, and the hostapd and dnsmasq
 * it drives) to /data/local/virtualap/bin. Every binary is a fully-static ELF
 * (aarch64 or armhf) that runs directly on Android.
 *
 * The APK bundles both arches under assets/bin/<arch>/; install() picks the one
 * matching the device (see [deviceArch]) and deploys only that. Each arch dir
 * carries a version marker (assets/bin/<arch>/PAYLOAD_VERSION, a hash of the
 * binaries written by the prepareAssets gradle task). When the installed APK
 * ships a different marker than the one last deployed, the setup flow re-runs -
 * this is the whole "update backend" mechanism.
 */
object VirtualAPInstaller {

    /** Marker asset whose content changes whenever the binaries change. */
    private const val VERSION_MARKER = "PAYLOAD_VERSION"

    /**
     * The asset arch dir for this device. We only ship 64-bit ARM (aarch64) and
     * 32-bit ARM (armhf); anything else falls back to aarch64 (the common case).
     */
    fun deviceArch(): String {
        val abi = Build.SUPPORTED_ABIS.firstOrNull().orEmpty()
        return when {
            abi.contains("arm64") || abi.contains("aarch64") -> "aarch64"
            abi.contains("armeabi") || abi.contains("arm") -> "armhf"
            else -> "aarch64"
        }
    }

    private var bundled: String? = null

    /** Payload-version hash shipped in the installed APK for this arch, or null. Read once. */
    fun bundledPayloadVersion(context: Context): String? = bundled ?: runCatching {
        context.assets.open("bin/${deviceArch()}/$VERSION_MARKER")
            .bufferedReader().use { it.readText().trim() }
    }.getOrNull().takeUnless { it.isNullOrBlank() }.also { bundled = it }

    /** Binary names shipped under assets/bin/<arch> (everything except the marker). */
    private fun bundledBinaries(context: Context): List<String> =
        runCatching {
            context.assets.list("bin/${deviceArch()}")?.filter { it != VERSION_MARKER } ?: emptyList()
        }.getOrDefault(emptyList())

    /**
     * True when the APK ships a different binary payload than the one last
     * deployed. Drives the re-run of the setup flow after an app update.
     */
    fun payloadUpdateAvailable(context: Context): Boolean {
        val bundled = bundledPayloadVersion(context) ?: return false
        return PreferencesManager.payloadVersion != bundled
    }

    suspend fun install(context: Context): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val cacheDir = context.cacheDir

            // Step 0: A re-install (backend update) must not race a live backend.
            // Overwriting a binary some process still executes fails with
            // ETXTBSY, so stop the AP first (deployAsset also unlinks first).
            // After an update from the sh engine this is the script it extracted
            // (see Backend.startAp), which started the AP; it goes for good
            // afterwards, and every command from then on is virtualap's.
            Shell.cmd("${Backend.startAp} stop").exec()
            File(context.filesDir, "backend").deleteRecursively()

            // Step 1: Create directories, and remove what older versions left:
            // the chroot-era scripts and rootfs, and the binaries only the sh
            // engine ran.
            val dir = Backend.quote(Constants.VAP_DIR)
            Shell.cmd(
                "mkdir -p $dir/bin $dir/logs $dir/run",
                "rm -f $dir/vap.sh $dir/start-ap $dir/bin/busybox $dir/bin/iw $dir/bin/hostapd_cli",
                "rm -rf $dir/rootfs"
            ).exec()

            // Step 2: Deploy the static binaries for this device's arch.
            val arch = deviceArch()
            val binaries = bundledBinaries(context)
            if (binaries.isEmpty())
                return@withContext Result.failure(
                    Exception(context.getString(R.string.setup_no_binaries, arch))
                )
            for (name in binaries) {
                deployAsset(context, "bin/$arch/$name", "${Constants.VAP_DIR}/bin/$name", cacheDir)
                    ?.let { return@withContext Result.failure(it) }
            }
            Shell.cmd("chmod 755 $dir/bin/*").exec()

            // Step 3: Verify every binary is in place and executable.
            if (!APManager.isInstalled()) return@withContext Result.failure(
                Exception(context.getString(R.string.setup_verify_failed))
            )

            installBootModule(context).getOrElse { return@withContext Result.failure(it) }

            // Record which payload is now deployed so future APK updates with
            // changed binaries can trigger a re-install.
            bundledPayloadVersion(context)?.let { PreferencesManager.payloadVersion = it }

            Result.success(Unit)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    suspend fun installBootModule(context: Context): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val files = listOf("module.prop", "service.sh", "install_module.sh").associateWith { name ->
                val tmp = File(context.cacheDir, "virtualap-$name")
                context.assets.open("bootmodule/$name").use { input ->
                    tmp.outputStream().use { input.copyTo(it) }
                }
                tmp
            }
            val result = Shell.cmd(
                "sh ${Backend.quote(files.getValue("install_module.sh").absolutePath)} " +
                    "${Backend.quote(files.getValue("module.prop").absolutePath)} " +
                    Backend.quote(files.getValue("service.sh").absolutePath)
            ).exec()
            files.values.forEach { it.delete() }
            if (!result.isSuccess) return@withContext Result.failure(
                Exception("Could not install VirtualAP boot module: ${result.out.joinToString("\n")}")
            )
            Result.success(Unit)
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    suspend fun saveBootConfig(config: APConfig): Result<Unit> = withContext(Dispatchers.IO) {
        if (!config.isValid()) return@withContext Result.failure(IllegalArgumentException("Invalid AP configuration"))
        val values = listOf(
            "-s" to config.ssid,
            "-p" to config.password,
            "-o" to config.upstream,
            "-b" to config.band,
            "-c" to config.channel,
            "-W" to config.width,
            "-g" to config.gateway,
            "-d" to config.dnsServers,
            "-H" to if (config.hidden) "1" else "0",
            "-A" to config.security,
            "-M" to if (config.pmf) "1" else "0",
            "-K" to if (config.containerMode) config.containerName else "",
            "-T" to if (config.ttlFix) "1" else "0",
            "-6" to if (config.ipv6) "1" else "0",
            "-m" to if (config.macMode == "random") "" else config.gatewayMac
        )
        if (values.any { (_, value) -> '\n' in value || '\r' in value })
            return@withContext Result.failure(IllegalArgumentException("AP values cannot contain line breaks"))

        val argsFile = "${Constants.BOOT_STATE_DIR}/boot.args"
        val tmpFile = "$argsFile.new"
        val args = values.flatMap { listOf(it.first, it.second) }
            .joinToString(" ") { Backend.quote(it) }
        val result = Shell.cmd(
            "mkdir -p ${Backend.quote(Constants.BOOT_STATE_DIR)} && " +
                "printf '%s\\n' $args > ${Backend.quote(tmpFile)} && " +
                "chmod 600 ${Backend.quote(tmpFile)} && " +
                "mv ${Backend.quote(tmpFile)} ${Backend.quote(argsFile)}"
        ).exec()
        if (result.isSuccess) Result.success(Unit)
        else Result.failure(Exception("Could not save VirtualAP boot configuration: ${result.out.joinToString("\n")}"))
    }

    suspend fun configureAutoStart(context: Context, config: APConfig, enabled: Boolean): Result<Unit> {
        if (!enabled) return setBootMode(false)
        if (!config.isValid()) return Result.failure(IllegalArgumentException("Invalid AP configuration"))
        installBootModule(context).getOrElse { return Result.failure(it) }
        saveBootConfig(config).getOrElse { return Result.failure(it) }
        return setBootMode(true)
    }

    suspend fun setBootMode(enabled: Boolean): Result<Unit> = withContext(Dispatchers.IO) {
        val mode = if (enabled) "1" else "0"
        val flag = "${Constants.BOOT_STATE_DIR}/.boot_mode"
        val result = Shell.cmd(
            "mkdir -p ${Backend.quote(Constants.BOOT_STATE_DIR)} && " +
                "printf '%s\\n' $mode > ${Backend.quote(flag)} && " +
                "chmod 755 ${Backend.quote(Constants.BOOT_STATE_DIR)} && chmod 666 ${Backend.quote(flag)}"
        ).exec()
        if (result.isSuccess) Result.success(Unit)
        else Result.failure(Exception("Could not update VirtualAP boot mode: ${result.out.joinToString("\n")}"))
    }

    suspend fun bootModeEnabled(): Boolean = withContext(Dispatchers.IO) {
        Shell.cmd("cat ${Backend.quote("${Constants.BOOT_STATE_DIR}/.boot_mode")} 2>/dev/null || echo 0")
            .exec().out.any { it.trim() == "1" }
    }

    private fun deployAsset(context: Context, assetPath: String, destPath: String, cacheDir: File): Exception? {
        return try {
            val tmpFile = File(cacheDir, File(assetPath).name)
            context.assets.open(assetPath).use { input ->
                tmpFile.outputStream().use { input.copyTo(it) }
            }
            // Unlink first: overwriting a binary some process still executes
            // fails with ETXTBSY, but unlink+create always succeeds (the
            // running process keeps the old inode).
            val dest = Backend.quote(destPath)
            val copyResult = Shell.cmd("rm -f $dest && cp ${Backend.quote(tmpFile.absolutePath)} $dest 2>&1").exec()
            tmpFile.delete()
            // FLAG_REDIRECT_STDERR sends stderr to .out - .err is always empty.
            if (!copyResult.isSuccess) Exception(context.getString(R.string.setup_deploy_failed, assetPath, copyResult.out.joinToString("\n")))
            else null
        } catch (e: Exception) {
            e
        }
    }
}
