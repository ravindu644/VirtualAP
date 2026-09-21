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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

enum class InstallStatus { Checking, NotInstalled, Installing, Installed, Error }

class AppViewModel(application: Application) : AndroidViewModel(application) {
    private val prefs = PreferencesManager.getInstance(application)

    var rootStatus by mutableStateOf(
        if (prefs.rootAvailable) RootStatus.Granted else RootStatus.Checking
    )
        private set

    var installStatus by mutableStateOf(InstallStatus.Checking)
        private set

    init {
        if (prefs.hasSeenRootCheck && prefs.rootAvailable) {
            // Returning user: skip the root screen, jump straight to install check,
            // and verify root silently so we catch revocations without a prompt.
            checkInstalled()
            checkRootSilently()
        } else {
            // First launch or root previously denied: show RootCheckScreen and
            // immediately initiate the check so the SU manager dialog appears.
            checkRoot()
        }
    }

    /** Called from the "Grant Root Access" button on RootCheckScreen. */
    fun checkRoot() {
        viewModelScope.launch {
            rootStatus = RootStatus.Checking
            val status = withContext(Dispatchers.IO) { RootChecker.checkRootAccess() }
            rootStatus = status
            prefs.rootAvailable = status == RootStatus.Granted
            prefs.hasSeenRootCheck = true
            if (status == RootStatus.Granted) checkInstalled()
        }
    }

    /** Silent background re-verification for returning users. On failure it flips
     *  rootStatus to Denied, which routes the UI to the full-screen root-denied
     *  state (no popup). Never flickers to Checking. */
    private fun checkRootSilently() {
        viewModelScope.launch(Dispatchers.IO) {
            val status = RootChecker.checkRootAccess()
            if (status != RootStatus.Granted) {
                prefs.rootAvailable = false
                withContext(Dispatchers.Main) { rootStatus = RootStatus.Denied }
            }
        }
    }

    /** Re-check used by the full-screen root-denied state while it waits for the
     *  user to grant root. Only advances on success (no Checking flicker). */
    fun pollRoot() {
        viewModelScope.launch {
            val status = withContext(Dispatchers.IO) { RootChecker.checkRootAccess() }
            if (status == RootStatus.Granted) {
                rootStatus = RootStatus.Granted
                prefs.rootAvailable = true
                prefs.hasSeenRootCheck = true
                checkInstalled()
            }
        }
    }

    /** On-demand root re-verification (e.g. pull-to-refresh on the main screen).
     *  Silent on success; routes to the root-denied screen if access is gone. */
    fun recheckRoot() = checkRootSilently()

    fun checkInstalled() {
        viewModelScope.launch {
            installStatus = InstallStatus.Checking
            val app = getApplication<Application>()
            val installed = withContext(Dispatchers.IO) {
                // An outdated payload (APK update shipped new binaries) counts
                // as not-installed so the setup flow re-deploys them.
                APManager.isInstalled() && !VirtualAPInstaller.payloadUpdateAvailable(app)
            }
            installStatus = if (installed) InstallStatus.Installed else InstallStatus.NotInstalled
        }
    }

    fun markInstalled() {
        // The installer already recorded the deployed payload version; the
        // start-destination guess and the live re-check both key off that, so
        // there's no separate "installed" flag to set here.
        installStatus = InstallStatus.Installed
    }
}
