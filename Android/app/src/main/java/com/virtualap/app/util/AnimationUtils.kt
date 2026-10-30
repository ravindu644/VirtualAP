package com.virtualap.app.util

import androidx.compose.animation.core.FastOutLinearInEasing
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.TweenSpec
import androidx.compose.animation.core.tween

/** The two timings the app animates with: screens and cards fade in on medium, out on fast. */
object AnimationUtils {
    fun <T> mediumSpec(): TweenSpec<T> = tween(durationMillis = 200, easing = FastOutSlowInEasing)
    fun <T> fadeOutSpec(): TweenSpec<T> = tween(durationMillis = 150, easing = FastOutLinearInEasing)
}
