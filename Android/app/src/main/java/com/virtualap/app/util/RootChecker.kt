package com.virtualap.app.util

import com.topjohnwu.superuser.Shell
import com.topjohnwu.superuser.ShellUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

enum class RootStatus { Checking, Granted, Denied }

object RootChecker {
    /**
     * Granted only if the app holds root and a live `id` runs; Denied when no shell
     * can be had at all. After a denial libsu keeps a non-root shell, so only a
     * [reprompt] (the gate's button, the first launch) opens a fresh one, which is
     * what brings the SU manager's prompt back; the quiet checks reuse the cached one.
     */
    suspend fun checkRootAccess(reprompt: Boolean = false): RootStatus = withContext(Dispatchers.IO) {
        val granted = runCatching {
            if (Shell.isAppGrantedRoot() != true) {
                if (reprompt) Shell.getCachedShell()?.close()
                Shell.cmd("id").exec()
            }
            Shell.isAppGrantedRoot() == true && ShellUtils.fastCmdResult("id")
        }.getOrDefault(false)
        if (granted) RootStatus.Granted else RootStatus.Denied
    }
}
