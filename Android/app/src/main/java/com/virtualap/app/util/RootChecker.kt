package com.virtualap.app.util

import com.topjohnwu.superuser.Shell
import com.topjohnwu.superuser.ShellUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

enum class RootStatus { Checking, Granted, Denied }

object RootChecker {
    /** Granted only if the app holds root and a live `id` runs. Denied when no shell can be had at all. */
    suspend fun checkRootAccess(): RootStatus = withContext(Dispatchers.IO) {
        val granted = runCatching {
            // Not granted yet: a fresh shell is what brings up the SU manager's
            // prompt; the cached one is the non-root fallback of the last denial.
            if (Shell.isAppGrantedRoot() != true) {
                Shell.getCachedShell()?.close()
                Shell.cmd("id").exec()
            }
            Shell.isAppGrantedRoot() == true && ShellUtils.fastCmdResult("id")
        }.getOrDefault(false)
        if (granted) RootStatus.Granted else RootStatus.Denied
    }
}
