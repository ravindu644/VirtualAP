package com.virtualap.app.util

import android.content.Context
import android.os.Build
import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Deploys the static AP-stack binaries (hostapd/hostapd_cli/iw/dnsmasq/busybox)
 * to /data/local/virtualap/bin. No chroot, no rootfs extraction - every binary
 * is a fully-static ELF (aarch64 or armhf) that runs directly on Android.
 *
 * The APK bundles both arches under assets/bin/<arch>/; install() picks the one
 * matching the device (see [deviceArch]) and deploys only that. Each arch dir
 * carries a version marker (assets/bin/<arch>/PAYLOAD_VERSION, a hash of the
 * binaries written by the prepareAssets gradle task). When the installed APK
 * ships a different marker than the one last deployed, the setup flow re-runs -
 * this is the whole "update backend" mechanism. Scripts run from the app files
 * dir and can never go stale (see Backend).
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

    /** Payload-version hash shipped in the installed APK for this arch, or null. */
    fun bundledPayloadVersion(context: Context): String? =
        runCatching {
            context.assets.open("bin/${deviceArch()}/$VERSION_MARKER")
                .bufferedReader().use { it.readText().trim() }
        }.getOrNull().takeUnless { it.isNullOrBlank() }

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
        return PreferencesManager.getInstance(context).payloadVersion != bundled
    }

    suspend fun install(
        context: Context,
        logger: BackendLogger
    ): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val cacheDir = context.cacheDir

            // Step 0: A re-install (backend update) must not race a live backend.
            // Overwriting a binary some process still executes fails with
            // ETXTBSY, so stop the AP first (deployAsset also unlinks first).
            logger.i("Stopping any running AP...")
            Shell.cmd("${Backend.startAp} stop").exec()

            // Step 1: Create directories. Scripts are NOT deployed here - they run
            // straight from the app's files dir (see Backend) so APK updates always
            // take effect. Remove stale files from the old chroot-based versions.
            logger.i("Creating directories...")
            Shell.cmd(
                "mkdir -p ${Constants.VAP_DIR}/bin ${Constants.VAP_DIR}/logs ${Constants.VAP_DIR}/run",
                "rm -f ${Constants.VAP_DIR}/vap.sh ${Constants.VAP_DIR}/start-ap",
                "rm -rf ${Constants.VAP_DIR}/rootfs"
            ).exec()

            // Step 2: Deploy the static binaries for this device's arch.
            val arch = deviceArch()
            val binaries = bundledBinaries(context)
            if (binaries.isEmpty())
                return@withContext Result.failure(
                    Exception("No binaries found in assets/bin/$arch/. Run scripts/build-static.sh and rebuild the APK.")
                )
            logger.i("Installing $arch binaries...")
            for (name in binaries) {
                logger.i("Installing $name...")
                deployAsset(context, "bin/$arch/$name", "${Constants.VAP_DIR}/bin/$name", cacheDir)
                    ?.let { return@withContext Result.failure(it) }
            }
            Shell.cmd("chmod 755 ${Constants.VAP_DIR}/bin/*").exec()

            // Step 3: Verify every binary is in place and executable.
            logger.i("Verifying installation...")
            val verify = binaries.joinToString(" && ") { "test -x ${Constants.VAP_DIR}/bin/$it" }
            val ok = Shell.cmd("$verify && echo ok").exec().out.any { it.contains("ok") }
            if (!ok) return@withContext Result.failure(
                Exception("Verification failed: a binary is missing or not executable after install")
            )

            // Record which payload is now deployed so future APK updates with
            // changed binaries can trigger a re-install.
            bundledPayloadVersion(context)?.let {
                PreferencesManager.getInstance(context).payloadVersion = it
            }

            logger.i("Installation complete!")
            Result.success(Unit)
        } catch (e: Exception) {
            logger.e("Installation failed: ${e.message}")
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
            val copyResult = Shell.cmd("rm -f $destPath && cp ${tmpFile.absolutePath} $destPath 2>&1").exec()
            tmpFile.delete()
            // FLAG_REDIRECT_STDERR sends stderr to .out - .err is always empty.
            if (!copyResult.isSuccess) Exception("Failed to deploy $assetPath: ${copyResult.out.joinToString("\n")}")
            else null
        } catch (e: Exception) {
            e
        }
    }
}
