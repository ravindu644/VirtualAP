package com.virtualap.app.ui.screen

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import com.virtualap.app.R
import com.virtualap.app.ui.component.SetupActionBar
import com.virtualap.app.ui.component.SetupFrame
import com.virtualap.app.ui.component.SetupHero
import com.virtualap.app.ui.component.SetupPage
import com.virtualap.app.util.RootStatus

/**
 * The root gate. Unknown asks, with the button that brings up the SU manager;
 * Checking shows the loader; Denied says so, and its button asks the manager
 * again. A denial leaves libsu holding a non-root shell, so nothing short of
 * that button can see a later grant. Granted is never drawn: the activity moves
 * on as soon as it lands.
 */
@Composable
fun RootCheckScreen(rootStatus: RootStatus, onCheck: () -> Unit) {
    val checking = rootStatus == RootStatus.Checking
    val denied = rootStatus == RootStatus.Denied
    val scheme = MaterialTheme.colorScheme

    Scaffold(
        containerColor = Color.Transparent,
        bottomBar = {
            SetupActionBar(
                label = stringResource(if (denied) R.string.check_root_again else R.string.grant_root),
                icon = Icons.Default.Security,
                onClick = onCheck,
                visible = !checking
            )
        }
    ) { innerPadding ->
        SetupPage(innerPadding = innerPadding) {
            SetupFrame(
                title = stringResource(
                    when {
                        checking -> R.string.checking_root
                        denied -> R.string.root_denied
                        else -> R.string.root_access_title
                    }
                ),
                body = stringResource(if (denied) R.string.root_check_body else R.string.root_access_body)
            ) {
                SetupHero(
                    loading = checking,
                    glyph = if (denied) Icons.Default.Close else Icons.Default.Security,
                    container = if (denied) scheme.errorContainer else scheme.primaryContainer,
                    onContainer = if (denied) scheme.onErrorContainer else scheme.onPrimaryContainer
                )
            }
        }
    }
}
