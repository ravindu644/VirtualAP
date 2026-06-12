package com.virtualap.app.ui.screen

import android.util.Log
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.virtualap.app.R
import com.virtualap.app.ui.component.TerminalConsole
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
    var retryKey by remember { mutableStateOf(0) }

    // Handle back button
    BackHandler(enabled = true) {
        when (setupState) {
            SetupState.SUCCESS -> onInstalled()
            SetupState.ERROR -> { /* allow going back on error if needed */ }
            SetupState.INSTALLING -> { /* block during install */ }
        }
    }

    // Run installation; retryKey increments on each retry to re-trigger this effect
    LaunchedEffect(retryKey) {
        logs.clear()
        logs.add(Log.INFO to "Starting VirtualAP installation...")
        val result = VirtualAPInstaller.install(context) { level, message ->
            logs.add(level to message)
        }
        // Surface the failure reason in the terminal - onProgress doesn't cover
        // every failure path (e.g. deployAsset errors are only in the Result).
        result.exceptionOrNull()?.let { logs.add(Log.ERROR to "[ERROR] ${it.message}") }
        setupState = if (result.isSuccess) SetupState.SUCCESS else SetupState.ERROR
    }

    val installLogsLabel = stringResource(R.string.install_logs_label)
    val logsCopiedMsg = stringResource(R.string.logs_copied)

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        text = when (setupState) {
                            SetupState.INSTALLING -> stringResource(R.string.setup_title)
                            SetupState.SUCCESS -> stringResource(R.string.installation_complete)
                            SetupState.ERROR -> stringResource(R.string.installation_failed)
                        },
                        fontWeight = FontWeight.SemiBold
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
        }
    ) { innerPadding ->
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .padding(horizontal = 24.dp, vertical = 24.dp)
        ) {
            val consoleMaxHeight = if (setupState == SetupState.INSTALLING) {
                maxHeight
            } else {
                maxHeight - ButtonDefaults.MinHeight - 12.dp
            }

            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .animateContentSize(spring(stiffness = Spring.StiffnessLow))
            ) {
                TerminalConsole(
                    logs = logs,
                    isProcessing = setupState == SetupState.INSTALLING,
                    modifier = Modifier.fillMaxWidth(),
                    maxHeight = consoleMaxHeight
                )

                when (setupState) {
                    SetupState.SUCCESS -> {
                        Spacer(modifier = Modifier.height(12.dp))
                        Button(
                            onClick = onInstalled,
                            modifier = Modifier.fillMaxWidth().height(56.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.primary
                            )
                        ) {
                            Icon(Icons.Default.CheckCircle, contentDescription = null)
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(stringResource(R.string.done), style = MaterialTheme.typography.labelLarge)
                        }
                    }
                    SetupState.ERROR -> {
                        Spacer(modifier = Modifier.height(12.dp))
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(12.dp)
                        ) {
                            OutlinedButton(
                                onClick = {
                                    // Retry: increment key to re-trigger LaunchedEffect
                                    setupState = SetupState.INSTALLING
                                    retryKey++
                                },
                                modifier = Modifier.weight(1f).height(56.dp)
                            ) {
                                Text(stringResource(R.string.retry), style = MaterialTheme.typography.labelLarge)
                            }
                            Button(
                                onClick = {
                                    val logText = logs.joinToString("\n") { it.second }
                                    val clipboard = context.getSystemService(android.content.Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
                                    val clip = android.content.ClipData.newPlainText(installLogsLabel, logText)
                                    clipboard.setPrimaryClip(clip)
                                    Toast.makeText(context, logsCopiedMsg, Toast.LENGTH_SHORT).show()
                                },
                                modifier = Modifier.weight(1f).height(56.dp)
                            ) {
                                Icon(Icons.Default.ContentCopy, contentDescription = null)
                                Spacer(modifier = Modifier.width(8.dp))
                                Text(stringResource(R.string.copy_logs), style = MaterialTheme.typography.labelLarge)
                            }
                        }
                    }
                    SetupState.INSTALLING -> {
                        // No buttons during installation
                    }
                }
            }
        }
    }
}

