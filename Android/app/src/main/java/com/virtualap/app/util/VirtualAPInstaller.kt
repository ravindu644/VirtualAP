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

            // Record which payload is now deployed so future APK updates with
            // changed binaries can trigger a re-install.
            bundledPayloadVersion(context)?.let { PreferencesManager.payloadVersion = it }

            Result.success(Unit)
        } catch (e: Exception) {
            Result.failure(e)
        }
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
