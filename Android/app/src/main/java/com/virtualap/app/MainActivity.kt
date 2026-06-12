package com.virtualap.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material3.*
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
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
import com.virtualap.app.ui.viewmodel.AppViewModel
import com.virtualap.app.ui.viewmodel.InstallStatus
import com.virtualap.app.util.PreferencesManager
import com.virtualap.app.util.RootStatus
import com.virtualap.app.util.VirtualAPInstaller

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            val appVm: AppViewModel = viewModel()
            val systemDark = isSystemInDarkTheme()
            val darkTheme = if (appVm.followSystemTheme) systemDark else appVm.darkThemeEnabled

            VirtualAPTheme(
                darkTheme = darkTheme,
                dynamicColor = appVm.dynamicColor,
                amoledMode = appVm.amoledMode,
                themePalette = appVm.themePalette
            ) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    // Compute start destination once from SharedPreferences (synchronous read)
                    val prefs = remember { PreferencesManager.getInstance(applicationContext) }
                    val startDestination = remember {
                        // An APK update can ship a newer rootfs tarball - send the
                        // user back through setup so it gets re-extracted.
                        val rootfsOutdated = VirtualAPInstaller.rootfsUpdateAvailable(applicationContext)
                        when {
                            !prefs.hasSeenRootCheck || !prefs.rootAvailable -> Screens.ROOT_CHECK
                            prefs.isInstalled && !rootfsOutdated -> Screens.MAIN
                            else -> Screens.SETUP
                        }
                    }

                    val navController = rememberNavController()

                    NavHost(navController = navController, startDestination = startDestination) {
                        composable(Screens.ROOT_CHECK) {
                            RootCheckScreen(
                                rootStatus = appVm.rootStatus,
                                onCheckRoot = { appVm.checkRoot() },
                                onNavigateNext = {
                                    if (appVm.installStatus == InstallStatus.Installed)
                                        navController.navigate(Screens.MAIN) {
                                            popUpTo(Screens.ROOT_CHECK) { inclusive = true }
                                        }
                                    else
                                        navController.navigate(Screens.SETUP) {
                                            popUpTo(Screens.ROOT_CHECK) { inclusive = true }
                                        }
                                }
                            )
                        }
                        composable(Screens.SETUP) {
                            SetupScreen(
                                onInstalled = {
                                    appVm.markInstalled()
                                    navController.navigate(Screens.MAIN) {
                                        popUpTo(Screens.SETUP) { inclusive = true }
                                    }
                                }
                            )
                        }
                        composable(Screens.MAIN) {
                            MainScreen(
                                onNavigateToSettings = { navController.navigate(Screens.SETTINGS) }
                            )
                        }
                        composable(Screens.SETTINGS) {
                            SettingsScreen(
                                appVm = appVm,
                                onBack = { navController.popBackStack() }
                            )
                        }
                    }

                    // Auto-navigate from ROOT_CHECK once root + install status are known
                    LaunchedEffect(appVm.rootStatus, appVm.installStatus) {
                        val current = navController.currentDestination?.route
                        if (current != Screens.ROOT_CHECK) return@LaunchedEffect
                        when {
                            appVm.rootStatus == RootStatus.Granted
                                && appVm.installStatus == InstallStatus.Installed ->
                                navController.navigate(Screens.MAIN) {
                                    popUpTo(Screens.ROOT_CHECK) { inclusive = true }
                                }
                            appVm.rootStatus == RootStatus.Granted
                                && appVm.installStatus == InstallStatus.NotInstalled ->
                                navController.navigate(Screens.SETUP) {
                                    popUpTo(Screens.ROOT_CHECK) { inclusive = true }
                                }
                        }
                    }

                    // Root-revoked overlay — shown when background check fails for a returning user
                    if (appVm.rootRevokedInBackground) {
                        AlertDialog(
                            onDismissRequest = {},
                            icon = {
                                Icon(
                                    imageVector = Icons.Default.Shield,
                                    contentDescription = null,
                                    tint = MaterialTheme.colorScheme.error
                                )
                            },
                            title = { Text(stringResource(R.string.root_revoked_title)) },
                            text = {
                                Text(stringResource(R.string.root_revoked_desc))
                            },
                            confirmButton = {
                                Button(onClick = { appVm.retryRootAfterRevoke() }) {
                                    Text(stringResource(R.string.retry))
                                }
                            },
                            dismissButton = {}
                        )
                    }
                }
            }
        }
    }
}
