package com.virtualap.app.ui.viewmodel

import android.app.Application
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.virtualap.app.util.APManager
import com.virtualap.app.util.PreferencesManager
import com.virtualap.app.util.RootChecker
import com.virtualap.app.util.RootStatus
import com.virtualap.app.util.VirtualAPInstaller
import kotlinx.coroutines.launch

enum class InstallStatus { Checking, NotInstalled, Installed }

class AppViewModel(application: Application) : AndroidViewModel(application) {
    private val prefs = PreferencesManager
    private val returning = prefs.hasSeenRootCheck && prefs.rootAvailable

    var rootStatus by mutableStateOf(if (returning) RootStatus.Granted else RootStatus.Unknown)
        private set

    var welcomeDone by mutableStateOf(prefs.hasSeenWelcome)
        private set

    // Seeded from what the last run recorded, so a returning user lands on the
    // main screen at once; checkInstalled() corrects it when the binaries are gone.
    var installStatus by mutableStateOf(
        when {
            !returning -> InstallStatus.Checking
            VirtualAPInstaller.payloadUpdateAvailable(application) -> InstallStatus.NotInstalled
            else -> InstallStatus.Installed
        }
    )
        private set

    init {
        // A returning user is rechecked quietly, so a revoked root routes to the
        // gate without a prompt. Nothing runs su under the welcome pager: a first
        // launch waits for the gate's button, an upgrade for finishWelcome().
        if (returning && welcomeDone) checkRoot()
    }

    fun finishWelcome() {
        prefs.hasSeenWelcome = true
        welcomeDone = true
        if (returning) checkRoot()
    }

    /**
     * Root first, then the payload. [loud] shows the Checking state and asks the
     * SU manager again (the first launch, the gate's button); the quiet form (pull
     * to refresh, the returning-user recheck) only moves the screen when an answer
     * changed.
     */
    fun checkRoot(loud: Boolean = false) {
        viewModelScope.launch {
            if (loud) rootStatus = RootStatus.Checking
            val status = RootChecker.checkRootAccess(reprompt = loud)
            prefs.rootAvailable = status == RootStatus.Granted
            prefs.hasSeenRootCheck = true
            rootStatus = status
            if (status == RootStatus.Granted) checkInstalled()
        }
    }

    private fun checkInstalled() {
        viewModelScope.launch {
            // An outdated payload (APK update shipped new binaries) counts as
            // not installed so the setup flow re-deploys them.
            val installed = APManager.isInstalled() &&
                !VirtualAPInstaller.payloadUpdateAvailable(getApplication())
            installStatus = if (installed) InstallStatus.Installed else InstallStatus.NotInstalled
        }
    }

    /** The installer recorded the deployed payload version; nothing else to flip. */
    fun markInstalled() {
        installStatus = InstallStatus.Installed
    }
}
