package com.virtualap.app.ui.component

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.widget.Toast
import androidx.annotation.StringRes
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
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
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Block
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.ContentCopy
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
import androidx.compose.ui.draw.clip
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

private fun copyMac(context: Context, mac: String) {
    val clipboard = context.getSystemService(ClipboardManager::class.java)
    clipboard.setPrimaryClip(ClipData.newPlainText(context.getString(R.string.mac_label), mac))
    Toast.makeText(context, R.string.mac_copied, Toast.LENGTH_SHORT).show()
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
    val context = LocalContext.current
    // The whole row copies the MAC, whose own line is too small to hit. The action inside it
    // keeps its tap to itself.
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .clickable(onClickLabel = stringResource(R.string.mac_copy)) { copyMac(context, client.mac) }
            .padding(vertical = 4.dp),
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
            val detail = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
            client.ip?.let {
                Text(text = it, style = MaterialTheme.typography.bodySmall, fontFamily = JetBrainsMono, color = detail)
            }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(text = client.mac, style = MaterialTheme.typography.bodySmall, fontFamily = JetBrainsMono, color = detail)
                Icon(Icons.Default.ContentCopy, contentDescription = null, tint = detail, modifier = Modifier.size(12.dp))
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

@Composable
private fun DeviceRows(
    list: List<ApClient>,
    empty: String,
    icon: ImageVector,
    iconTint: Color,
    actionLabel: String,
    @StringRes actionDescription: Int,
    actionColor: Color,
    onAction: (ApClient) -> Unit
) {
    if (list.isEmpty()) {
        Box(modifier = Modifier.fillMaxWidth().padding(vertical = 24.dp), contentAlignment = Alignment.Center) {
            Caption(empty)
        }
    }
    list.forEachIndexed { i, device ->
        if (i > 0) HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.3f))
        DeviceRow(device, icon, iconTint, actionLabel, actionDescription, actionColor) { onAction(device) }
    }
}

/** The blocked devices with an Unblock each, and what a block does not do. */
@Composable
fun BlockedList(list: List<ApClient>, onUnblock: (ApClient) -> Unit) {
    DeviceRows(
        list = list,
        empty = stringResource(R.string.blocked_empty),
        icon = Icons.Default.Block,
        iconTint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f),
        actionLabel = stringResource(R.string.device_unblock),
        actionDescription = R.string.device_unblock_named,
        actionColor = MaterialTheme.colorScheme.primary,
        onAction = onUnblock
    )
    if (list.isNotEmpty()) {
        Spacer(Modifier.height(12.dp))
        Caption(stringResource(R.string.blocked_note))
    }
}

/** The allowed devices with a Remove each. */
@Composable
fun AllowedList(list: List<ApClient>, onRemove: (ApClient) -> Unit) {
    DeviceRows(
        list = list,
        empty = stringResource(R.string.allowed_empty),
        icon = Icons.Default.CheckCircle,
        iconTint = MaterialTheme.colorScheme.primary.copy(alpha = 0.7f),
        actionLabel = stringResource(R.string.device_remove),
        actionDescription = R.string.device_remove_named,
        actionColor = MaterialTheme.colorScheme.error,
        onAction = onRemove
    )
}

/** The bottom sheet every device list sits in. */
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
 * One list on its own for Settings, where it can be reached with the hotspot off. The list and
 * its actions are the caller's, so a row showing its count follows a change, even one still
 * running when the sheet closes. A null list is one that could not be read.
 */
@Composable
private fun SavedDevicesSheet(
    title: String,
    idleNote: String?,
    readable: Boolean,
    onAdd: () -> Unit,
    onDismiss: () -> Unit,
    list: @Composable () -> Unit
) {
    DeviceSheet(title, onDismiss) {
        idleNote?.let {
            Spacer(Modifier.height(4.dp))
            Caption(it)
        }
        Spacer(Modifier.height(12.dp))
        if (!readable) {
            Caption(stringResource(R.string.lists_unreadable))
            return@DeviceSheet
        }
        Surface(
            onClick = onAdd,
            modifier = Modifier.fillMaxWidth().height(48.dp),
            shape = RoundedCornerShape(16.dp),
            color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.4f),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.2f)),
            tonalElevation = 0.dp
        ) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.CenterHorizontally),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(Icons.Default.Add, contentDescription = null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(20.dp))
                Text(
                    text = stringResource(R.string.device_add),
                    style = MaterialTheme.typography.labelLarge,
                    fontWeight = FontWeight.SemiBold,
                    color = MaterialTheme.colorScheme.primary
                )
            }
        }
        Spacer(Modifier.height(8.dp))
        list()
    }
}

@Composable
fun BlockedDevicesSheet(
    blocked: List<ApClient>?,
    allowOnly: Boolean,
    onAdd: () -> Unit,
    onUnblock: (ApClient) -> Unit,
    onDismiss: () -> Unit
) = SavedDevicesSheet(
    title = stringResource(R.string.blocked_title),
    idleNote = if (allowOnly) stringResource(R.string.blocked_idle) else null,
    readable = blocked != null,
    onAdd = onAdd,
    onDismiss = onDismiss
) { BlockedList(blocked.orEmpty(), onUnblock) }

@Composable
fun AllowedDevicesSheet(
    allowed: List<ApClient>?,
    allowOnly: Boolean,
    onAdd: () -> Unit,
    onRemove: (ApClient) -> Unit,
    onDismiss: () -> Unit
) = SavedDevicesSheet(
    title = stringResource(R.string.allowed_title),
    idleNote = if (allowOnly) null else stringResource(R.string.allowed_idle),
    readable = allowed != null,
    onAdd = onAdd,
    onDismiss = onDismiss
) { AllowedList(allowed.orEmpty(), onRemove) }
