package com.virtualap.app.ui.theme

import androidx.compose.ui.graphics.Color

val AMOLED_BLACK = Color(0xFF000000)

/**
 * Pre-defined accent color palettes for the app, mimicking Android's
 * "Wallpaper & style" palette picker.
 *
 * Each palette defines primary, secondary, and tertiary colors for both
 * light and dark modes.
 */
enum class ThemePalette(
    val displayName: String,
    val primaryLight: Color,
    val secondaryLight: Color,
    val tertiaryLight: Color,
    val primaryDark: Color,
    val secondaryDark: Color,
    val tertiaryDark: Color
) {
    CATPPUCCIN(
        displayName = "Catppuccin",
        primaryLight = Color(0xFF8AADF4),   // Blue
        secondaryLight = Color(0xFFB7BDF8), // Lavender
        tertiaryLight = Color(0xFFA6DA95),  // Green
        primaryDark = Color(0xFF7DC4E4),    // Sky
        secondaryDark = Color(0xFFF5BDE6),  // Pink
        tertiaryDark = Color(0xFFA6DA95)    // Green
    ),
    OCEAN(
        displayName = "Ocean",
        primaryLight = Color(0xFF0277BD),   // Deep Blue
        secondaryLight = Color(0xFF00ACC1), // Cyan
        tertiaryLight = Color(0xFF26A69A),  // Teal
        primaryDark = Color(0xFF4FC3F7),    // Light Blue
        secondaryDark = Color(0xFF4DD0E1),  // Light Cyan
        tertiaryDark = Color(0xFF80CBC4)    // Light Teal
    ),
    FOREST(
        displayName = "Forest",
        primaryLight = Color(0xFF2E7D32),   // Deep Green
        secondaryLight = Color(0xFF558B2F), // Olive Green
        tertiaryLight = Color(0xFF8D6E63),  // Brown
        primaryDark = Color(0xFF81C784),    // Light Green
        secondaryDark = Color(0xFFA5D6A7),  // Pale Green
        tertiaryDark = Color(0xFFBCAAA4)    // Light Brown
    ),
    SUNSET(
        displayName = "Sunset",
        primaryLight = Color(0xFFD84315),   // Deep Orange
        secondaryLight = Color(0xFFF4511E), // Orange-Red
        tertiaryLight = Color(0xFFFFB300),  // Amber
        primaryDark = Color(0xFFFF8A65),    // Light Orange
        secondaryDark = Color(0xFFFF8A80),  // Light Coral
        tertiaryDark = Color(0xFFFFD54F)    // Light Amber
    ),
    AMETHYST(
        displayName = "Amethyst",
        primaryLight = Color(0xFF6A1B9A),   // Deep Purple
        secondaryLight = Color(0xFF8E24AA), // Purple
        tertiaryLight = Color(0xFFAD1457),  // Deep Pink
        primaryDark = Color(0xFFCE93D8),    // Light Purple
        secondaryDark = Color(0xFFBA68C8),  // Medium Purple
        tertiaryDark = Color(0xFFF48FB1)    // Light Pink
    ),
    SAKURA(
        displayName = "Sakura",
        primaryLight = Color(0xFFD81B60),   // Pink
        secondaryLight = Color(0xFFEC407A), // Rose
        tertiaryLight = Color(0xFF7E57C2),  // Violet
        primaryDark = Color(0xFFF48FB1),    // Light Pink
        secondaryDark = Color(0xFFF8BBD0),  // Pale Pink
        tertiaryDark = Color(0xFFB39DDB)    // Light Violet
    );

    companion object {
        fun fromName(name: String): ThemePalette =
            entries.find { it.name == name } ?: CATPPUCCIN
    }
}
