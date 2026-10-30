package com.virtualap.app.ui.theme

import androidx.compose.material3.Typography
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import com.virtualap.app.R

/** Material 3's own scale; every site that wants a heavier title passes the weight itself. */
val Typography = Typography()

/** For machine output: log lines, config values, paths. One weight, the console never needs another. */
val JetBrainsMono = FontFamily(Font(R.font.jetbrains_mono_regular))
