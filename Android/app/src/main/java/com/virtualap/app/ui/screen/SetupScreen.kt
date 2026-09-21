package com.virtualap.app.ui.screen

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.virtualap.app.R
import com.virtualap.app.ui.component.LogActionRow
import com.virtualap.app.ui.component.PrimaryActionBottomBar
import com.virtualap.app.ui.component.TerminalConsole
import com.virtualap.app.util.ViewModelLogger
import com.virtualap.app.util.VirtualAPInstaller

private enum class SetupState { INSTALLING, SUCCESS, ERROR }

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SetupScreen(
    onInstalled: () -> Unit
) {
    val context = LocalContext.current
    var setupState by remember { mutableStateOf(SetupState.INSTALLING) }
    val logs = remember { mutableStateListOf<Pair<Int, String>>() }
    val logger = remember { ViewModelLogger { level, msg -> logs.add(level to msg) } }
    var retryKey by remember { mutableIntStateOf(0) }

    // Back is a no-op while installing and after a failure; Done handles success.
    BackHandler(enabled = true) {
        if (setupState == SetupState.SUCCESS) onInstalled()
    }

    // retryKey increments on each retry to re-trigger this effect.
    LaunchedEffect(retryKey) {
        logs.clear()
        logger.i("Starting VirtualAP installation...")
        val result = VirtualAPInstaller.install(context, logger)
        // Surface the failure reason in the terminal: the installer's own log
        // lines do not cover every failure path (deployAsset errors are only
        // in the Result).
        result.exceptionOrNull()?.let { logger.e("[ERROR] ${it.message}") }
        setupState = if (result.isSuccess) SetupState.SUCCESS else SetupState.ERROR
    }

    Scaffold(
        containerColor = Color.Transparent,
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        text = when (setupState) {
                            SetupState.INSTALLING -> stringResource(R.string.setup_title)
                            SetupState.SUCCESS -> stringResource(R.string.installation_complete)
                            SetupState.ERROR -> stringResource(R.string.installation_failed)
                        },
                        style = MaterialTheme.typography.titleLarge,
                        fontWeight = FontWeight.Bold
                    )
                },
                navigationIcon = {
                    if (setupState == SetupState.SUCCESS) {
                        IconButton(onClick = onInstalled) {
                            Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.back))
                        }
                    }
                }
            )
        },
        bottomBar = {
            when (setupState) {
                SetupState.SUCCESS -> PrimaryActionBottomBar(
                    label = stringResource(R.string.done),
                    icon = Icons.Default.CheckCircle,
                    onClick = onInstalled
                )
                SetupState.ERROR -> PrimaryActionBottomBar(
                    label = stringResource(R.string.retry),
                    icon = Icons.Default.Refresh,
                    onClick = {
                        setupState = SetupState.INSTALLING
                        retryKey++
                    },
                    secondaryAction = { LogActionRow(logs = logs, isBlocking = false, onClear = null) }
                )
                SetupState.INSTALLING -> Unit
            }
        }
    ) { innerPadding ->
        TerminalConsole(
            logs = logs,
            isProcessing = setupState == SetupState.INSTALLING,
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .padding(24.dp)
        )
    }
}
