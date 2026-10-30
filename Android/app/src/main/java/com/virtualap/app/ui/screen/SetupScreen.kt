package com.virtualap.app.ui.screen

import androidx.activity.compose.BackHandler
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import com.virtualap.app.R
import com.virtualap.app.ui.component.SetupActionBar
import com.virtualap.app.ui.component.SetupFrame
import com.virtualap.app.ui.component.SetupHero
import com.virtualap.app.ui.component.SetupPage
import com.virtualap.app.util.VirtualAPInstaller

/** Deploys the binaries on first run and after an APK update. Left only through its button. */
@Composable
fun SetupScreen(onInstalled: () -> Unit) {
    val context = LocalContext.current
    var result by remember { mutableStateOf<Result<Unit>?>(null) }
    var attempt by remember { mutableIntStateOf(0) }
    LaunchedEffect(attempt) {
        result = null
        result = VirtualAPInstaller.install(context)
    }
    val failed = result?.isFailure == true
    val done = result != null
    val scheme = MaterialTheme.colorScheme

    BackHandler(enabled = true) { if (result?.isSuccess == true) onInstalled() }

    Scaffold(
        containerColor = Color.Transparent,
        bottomBar = {
            SetupActionBar(
                label = stringResource(if (failed) R.string.retry else R.string.done),
                icon = if (failed) Icons.Default.Refresh else Icons.Default.Check,
                onClick = { if (failed) attempt++ else onInstalled() },
                visible = done
            )
        }
    ) { innerPadding ->
        SetupPage(innerPadding = innerPadding) {
            SetupFrame(
                title = stringResource(
                    when {
                        !done -> R.string.setup_title
                        failed -> R.string.installation_failed
                        else -> R.string.installation_complete
                    }
                ),
                body = when {
                    !done -> stringResource(R.string.setup_installing_body)
                    failed -> result?.exceptionOrNull()?.let { it.message ?: it.toString() }.orEmpty()
                    else -> stringResource(R.string.setup_done_body)
                },
                bodyColor = if (failed) scheme.error else scheme.onSurfaceVariant
            ) {
                SetupHero(
                    loading = !done,
                    glyph = if (failed) Icons.Default.Close else Icons.Default.Check,
                    container = if (failed) scheme.errorContainer else scheme.primaryContainer,
                    onContainer = if (failed) scheme.onErrorContainer else scheme.onPrimaryContainer
                )
            }
        }
    }
}
