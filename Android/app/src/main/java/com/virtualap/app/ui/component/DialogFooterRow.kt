package com.virtualap.app.ui.component

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

/**
 * The Cancel and Confirm row a dialog with a choice ends with: two equal-weight 48dp buttons, a
 * quiet tonal dismiss and a filled confirm. Labels stay on one line and ellipsize, so a long
 * translation cannot grow one button past the other.
 */
@Composable
fun DialogFooterRow(
    dismissLabel: String,
    confirmLabel: String,
    onDismiss: () -> Unit,
    onConfirm: () -> Unit,
) {
    val shape = RoundedCornerShape(16.dp)
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Surface(
            modifier = Modifier.weight(1f).height(48.dp).clip(shape).clickable(role = Role.Button, onClick = onDismiss),
            shape = shape,
            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.06f),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.35f)),
            tonalElevation = 0.dp
        ) {
            FooterLabel(dismissLabel, MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Surface(
            modifier = Modifier.weight(1f).height(48.dp).clip(shape).clickable(role = Role.Button, onClick = onConfirm),
            shape = shape,
            color = MaterialTheme.colorScheme.primary,
            tonalElevation = 0.dp
        ) {
            FooterLabel(confirmLabel, MaterialTheme.colorScheme.onPrimary)
        }
    }
}

@Composable
private fun FooterLabel(text: String, color: Color) {
    Box(modifier = Modifier.fillMaxSize().padding(horizontal = 12.dp), contentAlignment = Alignment.Center) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelLarge,
            fontWeight = FontWeight.SemiBold,
            color = color,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}
