package com.virtualap.app.ui.component

import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.RowScope
import androidx.compose.runtime.getValue
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

/**
 * Shared "primary call-to-action" bottom bar: a full-width [Surface] with a top
 * divider and a rounded primary action button (icon + label). Replaces the same
 * Surface -> Column -> HorizontalDivider -> clickable Surface -> Box -> Row block
 * that was copy-pasted across the wizard/detail screens.
 *
 * The varying bits are parameters so each screen keeps its exact look:
 * [enabled] toggles the disabled colors, and [containerColor]/[disabledContainerColor]
 * let a screen keep the button a constant color regardless of [enabled].
 */
@Composable
fun PrimaryActionBottomBar(
    label: String,
    icon: ImageVector,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    containerColor: Color = MaterialTheme.colorScheme.primary,
    contentColor: Color = MaterialTheme.colorScheme.onPrimary,
    disabledContainerColor: Color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f),
    disabledContentColor: Color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f),
    barColor: Color = MaterialTheme.colorScheme.surfaceContainer,
    dividerAlpha: Float = 0.25f,
    shape: Shape = RoundedCornerShape(20.dp),
) {
    val fg = if (enabled) contentColor else disabledContentColor
    PrimaryActionBottomBar(
        onClick = onClick,
        containerColor = if (enabled) containerColor else disabledContainerColor,
        modifier = modifier,
        enabled = enabled,
        barColor = barColor,
        dividerAlpha = dividerAlpha,
        shape = shape
    ) {
        Icon(icon, contentDescription = null, modifier = Modifier.size(18.dp), tint = fg)
        Text(
            text = label,
            style = MaterialTheme.typography.labelLarge,
            fontWeight = FontWeight.SemiBold,
            color = fg
        )
    }
}

/**
 * The same bar with the button's content supplied by the caller, for the save
 * bars that swap between an icon, a spinner and a check mark. [containerColor]
 * is animated, so a caller that changes it on a state change gets the fade for
 * free and does not have to run its own animateColorAsState.
 */
@Composable
fun PrimaryActionBottomBar(
    onClick: () -> Unit,
    containerColor: Color,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    barColor: Color = MaterialTheme.colorScheme.surfaceContainer,
    dividerAlpha: Float = 0.25f,
    shape: Shape = RoundedCornerShape(20.dp),
    content: @Composable RowScope.() -> Unit,
) {
    val bg by animateColorAsState(
        targetValue = containerColor,
        animationSpec = tween(durationMillis = 250),
        label = "bottom_bar_button"
    )
    Surface(
        modifier = modifier.fillMaxWidth(),
        color = barColor,
        tonalElevation = 0.dp
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            HorizontalDivider(
                color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = dividerAlpha),
                thickness = 1.dp
            )
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(24.dp)
                    .navigationBarsPadding(),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                Surface(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(shape)
                        .clickable(enabled = enabled, onClick = onClick),
                    shape = shape,
                    color = bg,
                    tonalElevation = 0.dp
                ) {
                    Box(
                        modifier = Modifier.padding(vertical = 16.dp).fillMaxWidth(),
                        contentAlignment = Alignment.Center
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            content = content
                        )
                    }
                }
            }
        }
    }
}
