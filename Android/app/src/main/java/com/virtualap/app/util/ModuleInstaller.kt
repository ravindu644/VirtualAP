package com.virtualap.app.util

import android.content.Context
import com.topjohnwu.superuser.Shell
import com.virtualap.app.R
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

object ModuleInstaller {
    /** The boot-module assets, staged in cacheDir, installed over a fresh module directory. */
    suspend fun install(context: Context): Result<Unit> = withContext(Dispatchers.IO) {
        runCatching {
            val staging = File(context.cacheDir, "boot-module-temp")
            try {
                staging.deleteRecursively()
                staging.mkdirs()
                context.assets.list("boot-module").orEmpty().forEach { name ->
                    context.assets.open("boot-module/$name").use { input ->
                        File(staging, name).outputStream().use { input.copyTo(it) }
                    }
                }
                val module = Backend.quote(Constants.BOOT_MODULE)
                // The test first: mkdir -p would make the directory on a phone with no root manager.
                val result = Shell.cmd(
                    "{ test -d ${Backend.quote(Constants.MODULES_DIR)} && rm -rf $module && mkdir -p $module && " +
                        "cp -f ${Backend.quote(staging.absolutePath)}/* $module/ && " +
                        "chmod 755 $module $module/service.sh && chmod 644 $module/module.prop; } 2>&1"
                ).exec()
                check(result.isSuccess) {
                    context.getString(R.string.setup_module_failed, result.out.joinToString("\n"))
                }
            } finally {
                staging.deleteRecursively()
            }
        }
    }
}
