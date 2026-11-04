package com.virtualap.app.ui.component

import android.widget.Toast
import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Block
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.virtualap.app.R
import com.virtualap.app.ui.theme.JetBrainsMono
import com.virtualap.app.util.APManager
import com.virtualap.app.util.ApClient
import kotlinx.coroutines.launch

/** Runs a block or an unblock, says so if it failed, and reloads what the sheet shows either way. */
@Composable
fun rememberDeviceAction(reload: suspend () -> Unit): (suspend () -> Boolean) -> Unit {
    val scope = rememberCoroutineScope()
    val context = LocalContext.current
    return { action ->
        scope.launch {
            if (!action()) Toast.makeText(context, R.string.block_failed, Toast.LENGTH_SHORT).show()
            reload()
        }
    }
}

@Composable
fun DeviceRow(
    client: ApClient,
    icon: ImageVector,
    iconTint: Color,
    actionLabel: String,
    @StringRes actionDescription: Int,
    actionColor: Color,
    onAction: () -> Unit
) {
    val name = client.name ?: stringResource(R.string.client_unnamed)
    val description = stringResource(actionDescription, name)
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Icon(imageVector = icon, contentDescription = null, tint = iconTint, modifier = Modifier.size(20.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = name,
                style = MaterialTheme.typography.bodyMedium,
                fontWeight = FontWeight.Medium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            // A line each: beside the action, the two do not fit on one on a phone.
            listOfNotNull(client.ip, client.mac).forEach {
                Text(
                    text = it,
                    style = MaterialTheme.typography.bodySmall,
                    fontFamily = JetBrainsMono,
                    color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
                )
            }
        }
        Surface(
            onClick = onAction,
            modifier = Modifier.semantics { contentDescription = description },
            shape = RoundedCornerShape(16.dp),
            color = Color.Transparent,
            tonalElevation = 0.dp
        ) {
            Box(
                modifier = Modifier.heightIn(min = 48.dp).padding(horizontal = 8.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = actionLabel,
                    style = MaterialTheme.typography.labelLarge,
                    fontWeight = FontWeight.SemiBold,
                    color = actionColor
                )
            }
        }
    }
}

/** The blocked devices with an Unblock each, and what a block does not do. */
@Composable
fun BlockedList(list: List<ApClient>, onUnblock: (ApClient) -> Unit) {
    if (list.isEmpty()) {
        Box(modifier = Modifier.fillMaxWidth().padding(vertical = 24.dp), contentAlignment = Alignment.Center) {
            Caption(stringResource(R.string.blocked_empty))
        }
        return
    }
    list.forEachIndexed { i, device ->
        if (i > 0) HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.3f))
        DeviceRow(
            client = device,
            icon = Icons.Default.Block,
            iconTint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
            actionLabel = stringResource(R.string.device_unblock),
            actionDescription = R.string.device_unblock_named,
            actionColor = MaterialTheme.colorScheme.primary,
            onAction = { onUnblock(device) }
        )
    }
    Spacer(Modifier.height(12.dp))
    Caption(stringResource(R.string.blocked_note))
}

/** The bottom sheet both device lists sit in. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DeviceSheet(title: String, onDismiss: () -> Unit, content: @Composable ColumnScope.() -> Unit) {
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        shape = RoundedCornerShape(topStart = 24.dp, topEnd = 24.dp),
        containerColor = MaterialTheme.colorScheme.surfaceContainer,
        tonalElevation = 0.dp
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 24.dp)
                .padding(bottom = 32.dp)
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleLarge,
                fontWeight = FontWeight.Bold
            )
            content()
        }
    }
}

/**
 * The list on its own for Settings, where it can be reached with the hotspot off. The list and
 * the action are the caller's, so a row showing its count follows an unblock, even one still
 * running when the sheet closes.
 */
@Composable
fun BlockedDevicesSheet(blocked: List<ApClient>?, onUnblock: (ApClient) -> Unit, onDismiss: () -> Unit) {
    DeviceSheet(stringResource(R.string.blocked_title), onDismiss) {
        Spacer(Modifier.height(12.dp))
        if (blocked == null) Caption(stringResource(R.string.blocked_unreadable))
        else BlockedList(blocked, onUnblock)
    }
}
