package com.virtualap.app.ui.component

import androidx.compose.foundation.border
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.unit.dp

/**
 * Dropdown menus render on a translucent tinted surface by default, which turns
 * muddy over a card. This forces an opaque surface a step above the background
 * and rounds the menu to 20.dp.
 *
 * Wrap the menu in this and put [dsMenuBorder] on the menu itself. Both the
 * select field in [DsDropdown] and the overflow menu on init service rows use it,
 * which is the only reason it is shared: they are different kinds of menu that
 * happen to need the same surface.
 */
@Composable
fun DsMenuTheme(content: @Composable () -> Unit) {
    val isDark = MaterialTheme.colorScheme.background.luminance() < 0.5f
    val menuColor = if (isDark) {
        MaterialTheme.colorScheme.surfaceContainerHigh
    } else {
        MaterialTheme.colorScheme.surfaceContainer
    }
    MaterialTheme(
        colorScheme = MaterialTheme.colorScheme.copy(
            surface = menuColor,
            surfaceContainer = menuColor,
            surfaceTint = Color.Transparent
        ),
        shapes = MaterialTheme.shapes.copy(extraSmall = RoundedCornerShape(20.dp)),
        content = content
    )
}

/** The 1.dp outline that goes on a menu opened inside [DsMenuTheme]. */
@Composable
fun Modifier.dsMenuBorder(): Modifier = border(
    width = 1.dp,
    color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.35f),
    shape = RoundedCornerShape(20.dp)
)
