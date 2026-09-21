package com.virtualap.app.ui.component

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.widget.Toast
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.ripple.rememberRipple
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.virtualap.app.R
import com.virtualap.app.util.AnsiColorParser

/**
 * The Clear / Copy row that sits above a log console. Clear only appears when
 * [onClear] is given; Copy alone fills the row otherwise. Both are inert while
 * [isBlocking] (a command is still streaming) or when there is nothing to act on.
 */
@Composable
fun LogActionRow(
    logs: List<Pair<Int, String>>,
    isBlocking: Boolean,
    onClear: (() -> Unit)?,
    modifier: Modifier = Modifier,
) {
    val context = LocalContext.current
    val canAct = logs.isNotEmpty() && !isBlocking
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        if (onClear != null) {
            LogActionButton(
                modifier = Modifier.weight(1f),
                icon = Icons.Default.Delete,
                label = context.getString(R.string.clear_logs),
                enabled = canAct,
                fill = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.06f),
                accent = MaterialTheme.colorScheme.error.copy(alpha = 0.4f),
                content = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.8f),
                onClick = onClear
            )
        }
        LogActionButton(
            modifier = Modifier.weight(1f),
            icon = Icons.Default.ContentCopy,
            label = context.getString(R.string.copy_logs),
            enabled = canAct,
            fill = MaterialTheme.colorScheme.primary.copy(alpha = 0.08f),
            accent = MaterialTheme.colorScheme.primary.copy(alpha = 0.3f),
            content = MaterialTheme.colorScheme.primary,
            onClick = { copyLogsToClipboard(context, logs) }
        )
    }
}

/** Copies the log lines as plain text, ANSI escapes stripped. */
fun copyLogsToClipboard(context: Context, logs: List<Pair<Int, String>>) {
    val text = logs.joinToString("\n") { AnsiColorParser.stripAnsi(it.second) }
    val clipboard = context.getSystemService(ClipboardManager::class.java)
    clipboard.setPrimaryClip(ClipData.newPlainText(context.getString(R.string.terminal_logs), text))
    Toast.makeText(context, R.string.logs_copied, Toast.LENGTH_SHORT).show()
}

@Composable
private fun LogActionButton(
    modifier: Modifier,
    icon: ImageVector,
    label: String,
    enabled: Boolean,
    fill: Color,
    accent: Color,
    content: Color,
    onClick: () -> Unit,
) {
    val shape = RoundedCornerShape(16.dp)
    val disabledContent = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.38f)
    Surface(
        modifier = modifier
            .height(38.dp)
            .clip(shape)
            .clickable(
                enabled = enabled,
                onClick = onClick,
                indication = rememberRipple(bounded = true),
                interactionSource = remember { MutableInteractionSource() }
            ),
        shape = shape,
        color = if (enabled) fill else MaterialTheme.colorScheme.onSurface.copy(alpha = 0.03f),
        border = BorderStroke(1.dp, if (enabled) accent else MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.2f)),
        tonalElevation = 0.dp
    ) {
        Row(
            modifier = Modifier.fillMaxSize().padding(horizontal = 14.dp),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                imageVector = icon,
                contentDescription = label,
                modifier = Modifier.size(16.dp),
                tint = if (enabled) content else disabledContent
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = label,
                style = MaterialTheme.typography.labelMedium,
                fontWeight = FontWeight.SemiBold,
                color = if (enabled) content else disabledContent
            )
        }
    }
}
