package com.virtualap.app

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.*
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.virtualap.app.ui.navigation.Screens
import com.virtualap.app.ui.screen.MainScreen
import com.virtualap.app.ui.screen.RootCheckScreen
import com.virtualap.app.ui.screen.SetupScreen
import com.virtualap.app.ui.screen.SettingsScreen
import com.virtualap.app.ui.theme.VirtualAPTheme
import com.virtualap.app.ui.theme.rememberThemeState
import com.virtualap.app.util.AnimationUtils
import com.virtualap.app.ui.viewmodel.APViewModel
import com.virtualap.app.ui.viewmodel.AppViewModel
import com.virtualap.app.ui.viewmodel.InstallStatus
import com.virtualap.app.util.PreferencesManager
import com.virtualap.app.util.RootStatus
import com.virtualap.app.util.VirtualAPInstaller

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
        enableEdgeToEdge()
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
                    // Compute start destination once from SharedPreferences (synchronous read)
                    val prefs = remember { PreferencesManager.getInstance(applicationContext) }
                    val startDestination = remember {
                        // !payloadOutdated means the deployed payload matches the
                        // APK's (set only by a verified install) - i.e. installed
                        // and current. An APK update with new binaries flips this
                        // and routes back through setup for re-deploy. The live
                        // wiped-binaries case is corrected by the gating effect.
                        val payloadOutdated = VirtualAPInstaller.payloadUpdateAvailable(applicationContext)
                        when {
                            !prefs.hasSeenRootCheck || !prefs.rootAvailable -> Screens.ROOT_CHECK
                            !payloadOutdated -> Screens.MAIN
                            else -> Screens.SETUP
                        }
                    }

                    val navController = rememberNavController()

                    // navigation-compose defaults to a 700ms fade between
                    // destinations, which feels sluggish entering Settings and
                    // going back. Use the standard medium fade instead.
                    val navFade = AnimationUtils.mediumSpec<Float>()
                    NavHost(
                        navController = navController,
                        startDestination = startDestination,
                        enterTransition = { fadeIn(animationSpec = navFade) },
                        exitTransition = { fadeOut(animationSpec = navFade) },
                        popEnterTransition = { fadeIn(animationSpec = navFade) },
                        popExitTransition = { fadeOut(animationSpec = navFade) }
                    ) {
                        composable(Screens.ROOT_CHECK) {
                            RootCheckScreen(
                                rootStatus = appVm.rootStatus,
                                onPollRoot = { appVm.pollRoot() }
                            )
                        }
                        composable(Screens.SETUP) {
                            // Navigation to MAIN is handled by the gating effect once
                            // markInstalled() flips installStatus to Installed.
                            SetupScreen(onInstalled = { appVm.markInstalled() })
                        }
                        composable(Screens.MAIN) {
                            LaunchedEffect(Unit) { askNotifications() }
                            MainScreen(
                                vm = apVm,
                                onNavigateToSettings = { navController.navigate(Screens.SETTINGS) },
                                onRefresh = { appVm.recheckRoot() }
                            )
                        }
                        composable(Screens.SETTINGS) {
                            SettingsScreen(onBack = { navController.popBackStack() })
                        }
                    }

                    // Central gating: live root + backend status are the single
                    // source of truth for which top-level screen is shown.
                    //  - root denied (incl. revoked in background) -> full-screen
                    //    root gate, no popup.
                    //  - backend missing (e.g. /data/local/virtualap wiped) -> SETUP
                    //    re-deploys it, even from MAIN.
                    //  - root granted + installed -> MAIN.
                    LaunchedEffect(appVm.rootStatus, appVm.installStatus) {
                        val current = navController.currentDestination?.route ?: return@LaunchedEffect
                        val target = when {
                            appVm.rootStatus == RootStatus.Denied -> Screens.ROOT_CHECK
                            appVm.rootStatus == RootStatus.Granted &&
                                appVm.installStatus == InstallStatus.NotInstalled &&
                                current != Screens.SETUP -> Screens.SETUP
                            appVm.rootStatus == RootStatus.Granted &&
                                appVm.installStatus == InstallStatus.Installed &&
                                (current == Screens.ROOT_CHECK || current == Screens.SETUP) -> Screens.MAIN
                            else -> null
                        }
                        if (target != null && target != current) {
                            navController.navigate(target) {
                                popUpTo(navController.graph.startDestinationId) { inclusive = true }
                                launchSingleTop = true
                            }
                        }
                    }
                }
            }
        }
    }
}
