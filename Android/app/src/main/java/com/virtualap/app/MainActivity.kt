package com.virtualap.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.LaunchedEffect
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
import com.virtualap.app.ui.viewmodel.AppViewModel
import com.virtualap.app.ui.viewmodel.InstallStatus
import com.virtualap.app.util.RootStatus

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            // ViewModel lives outside the theme so its state can drive recomposition of VirtualAPTheme
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
                    val navController = rememberNavController()

                    NavHost(navController = navController, startDestination = Screens.ROOT_CHECK) {
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

                    // Single source of auto-navigation driven by ViewModel state
                    LaunchedEffect(appVm.rootStatus, appVm.installStatus) {
                        val current = navController.currentDestination?.route
                        when {
                            appVm.rootStatus == RootStatus.Granted
                                && appVm.installStatus == InstallStatus.Installed
                                && current == Screens.ROOT_CHECK ->
                                navController.navigate(Screens.MAIN) {
                                    popUpTo(Screens.ROOT_CHECK) { inclusive = true }
                                }
                            appVm.rootStatus == RootStatus.Granted
                                && appVm.installStatus == InstallStatus.NotInstalled
                                && current == Screens.ROOT_CHECK ->
                                navController.navigate(Screens.SETUP) {
                                    popUpTo(Screens.ROOT_CHECK) { inclusive = true }
                                }
                        }
                    }
                }
            }
        }
    }
}
