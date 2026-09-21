package com.virtualap.app.ui.screen

import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import com.virtualap.app.R
import com.virtualap.app.ui.component.RootUnavailableState
import com.virtualap.app.ui.util.FullScreenLoading
import com.virtualap.app.util.RootStatus
import kotlinx.coroutines.delay

/**
 * Full-screen root gate. Only two visible states:
 *  - Checking: verifying access.
 *  - Denied:   "root required" - polls in the background and advances itself the
 *              moment the user grants root (no buttons).
 * The Granted state is never rendered: navigation moves on as soon as root is
 * confirmed, so this screen is only ever seen when root is missing.
 */
@Composable
fun RootCheckScreen(
    rootStatus: RootStatus = RootStatus.Checking,
    onPollRoot: () -> Unit = {}
) {
    // While denied, keep re-checking so the screen advances itself once the user
    // grants root in their root manager.
    LaunchedEffect(rootStatus) {
        if (rootStatus == RootStatus.Denied) {
            while (true) {
                delay(2500)
                onPollRoot()
            }
        }
    }

    Scaffold(containerColor = Color.Transparent) { innerPadding ->
        if (rootStatus == RootStatus.Denied) {
            RootUnavailableState(modifier = Modifier.padding(innerPadding))
        } else {
            FullScreenLoading(
                message = stringResource(R.string.checking_root),
                modifier = Modifier.padding(innerPadding)
            )
        }
    }
}
