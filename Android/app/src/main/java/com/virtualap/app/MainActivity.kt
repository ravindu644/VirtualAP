package com.virtualap.app

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import com.virtualap.app.ui.screen.MainScreen
import com.virtualap.app.ui.screen.RootCheckScreen
import com.virtualap.app.ui.screen.SettingsScreen
import com.virtualap.app.ui.screen.SetupScreen
import com.virtualap.app.ui.screen.WelcomeScreen
import com.virtualap.app.ui.theme.VirtualAPTheme
import com.virtualap.app.ui.theme.rememberThemeState
import com.virtualap.app.ui.util.FullScreenLoading
import com.virtualap.app.ui.viewmodel.APViewModel
import com.virtualap.app.ui.viewmodel.AppViewModel
import com.virtualap.app.ui.viewmodel.InstallStatus
import com.virtualap.app.util.AnimationUtils
import com.virtualap.app.util.RootStatus

private enum class Screen { WELCOME, ROOT_CHECK, LOADING, SETUP, MAIN, SETTINGS }

class MainActivity : ComponentActivity() {
    private val askNotifications =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) {}

    /** Android 13+ hides the running notification until this is granted. Asked
     *  once the main screen is up, so it never competes with the root prompt. */
    private fun askNotifications() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return
        if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED) return
        askNotifications.launch(Manifest.permission.POST_NOTIFICATIONS)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            val appVm: AppViewModel = viewModel()
            val apVm: APViewModel = viewModel()
            val themeState = rememberThemeState()

            VirtualAPTheme(
                darkTheme = themeState.darkTheme,
                dynamicColor = themeState.useDynamicColor,
                amoledMode = themeState.amoledMode,
                themePalette = themeState.themePalette
            ) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    // Settings is the one screen a tap opens. Everything else follows
                    // the live root and install state, so root revoked in the
                    // background or wiped binaries pull the user back to the gate
                    // from wherever they are.
                    var settings by rememberSaveable { mutableStateOf(false) }
                    val screen = when {
                        !appVm.welcomeDone -> Screen.WELCOME
                        appVm.rootStatus != RootStatus.Granted -> Screen.ROOT_CHECK
                        appVm.installStatus == InstallStatus.Checking -> Screen.LOADING
                        appVm.installStatus == InstallStatus.NotInstalled -> Screen.SETUP
                        settings -> Screen.SETTINGS
                        else -> Screen.MAIN
                    }
                    BackHandler(enabled = screen == Screen.SETTINGS) { settings = false }
                    // Pulled off Settings by the gate: come back to Main, not Settings.
                    LaunchedEffect(screen) {
                        if (screen != Screen.MAIN && screen != Screen.SETTINGS) settings = false
                    }

                    val fade = AnimationUtils.mediumSpec<Float>()
                    AnimatedContent(
                        targetState = screen,
                        transitionSpec = { fadeIn(fade) togetherWith fadeOut(fade) },
                        label = "screen"
                    ) { current ->
                        when (current) {
                            Screen.WELCOME -> WelcomeScreen(onDone = { appVm.finishWelcome() })
                            Screen.ROOT_CHECK -> RootCheckScreen(
                                rootStatus = appVm.rootStatus,
                                onCheck = { appVm.checkRoot(loud = true) }
                            )
                            Screen.LOADING -> FullScreenLoading()
                            Screen.SETUP -> SetupScreen(onInstalled = { appVm.markInstalled() })
                            Screen.MAIN -> {
                                LaunchedEffect(Unit) { askNotifications() }
                                MainScreen(
                                    vm = apVm,
                                    onNavigateToSettings = { settings = true },
                                    onRefresh = { appVm.checkRoot() }
                                )
                            }
                            Screen.SETTINGS -> SettingsScreen(onBack = { settings = false })
                        }
                    }
                }
            }
        }
    }
}
