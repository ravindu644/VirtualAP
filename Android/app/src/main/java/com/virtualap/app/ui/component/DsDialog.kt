package com.virtualap.app.ui.component

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties

/**
 * The shell every dialog sits in: full width inside the 24dp gutters, bounded to the
 * screen height, and a body that scrolls. Callers do not set a width, their own padding
 * or their own scroll; [modifier] is for what is per-dialog, `imePadding()` or a
 * `heightIn` cap. The first dialog with a choice brings back Droidspaces' `footer`
 * slot, measured before the body so landscape cannot squeeze the buttons.
 */
@Composable
fun DsDialog(
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit,
) {
    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        // The activity handles configChanges itself, so a dialog that is open
        // through a rotation keeps its stale window constraints. LocalConfiguration
        // is live across rotation, so the shell bounds itself with it.
        val screenHeight = LocalConfiguration.current.screenHeightDp.dp
        Surface(
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp)
                .heightIn(max = screenHeight - 48.dp)
                .then(modifier),
            shape = RoundedCornerShape(24.dp),
            color = MaterialTheme.colorScheme.surfaceContainer,
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.4f)),
            tonalElevation = 0.dp
        ) {
            Column(
                modifier = Modifier.padding(24.dp).verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(16.dp),
                content = content
            )
        }
    }
}
