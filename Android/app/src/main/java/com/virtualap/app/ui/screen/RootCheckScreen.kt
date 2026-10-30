package com.virtualap.app.ui.screen

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import com.virtualap.app.R
import com.virtualap.app.ui.component.SetupActionBar
import com.virtualap.app.ui.component.SetupFrame
import com.virtualap.app.ui.component.SetupHero
import com.virtualap.app.ui.component.SetupPage
import com.virtualap.app.util.RootStatus
import kotlinx.coroutines.delay

/**
 * The root gate. Checking shows the loader; Denied shows why root is needed and
 * polls so the screen advances itself once the user grants it in their root
 * manager. Granted is never drawn: the activity moves on as soon as it lands.
 */
@Composable
fun RootCheckScreen(rootStatus: RootStatus, onPollRoot: () -> Unit) {
    val denied = rootStatus == RootStatus.Denied
    LaunchedEffect(denied) {
        while (denied) {
            delay(2500)
            onPollRoot()
        }
    }
    val scheme = MaterialTheme.colorScheme
    val motion = MaterialTheme.motionScheme

    Scaffold(
        containerColor = Color.Transparent,
        bottomBar = {
            // The title already says a check is running, so the button fades out
            // for the duration. It keeps its space so the hero stays put.
            val barAlpha by animateFloatAsState(if (denied) 1f else 0f, motion.fastEffectsSpec(), label = "root_bar")
            SetupActionBar(
                label = stringResource(R.string.check_root_again),
                icon = Icons.Default.Security,
                onClick = onPollRoot,
                modifier = Modifier.alpha(barAlpha),
                enabled = denied
            )
        }
    ) { innerPadding ->
        SetupPage(innerPadding = innerPadding) {
            SetupFrame(
                title = stringResource(if (denied) R.string.root_denied else R.string.checking_root),
                body = stringResource(R.string.root_check_body)
            ) {
                SetupHero(
                    loading = !denied,
                    glyph = Icons.Default.Close,
                    container = if (denied) scheme.errorContainer else scheme.primaryContainer,
                    onContainer = if (denied) scheme.onErrorContainer else scheme.onPrimaryContainer
                )
            }
        }
    }
}
