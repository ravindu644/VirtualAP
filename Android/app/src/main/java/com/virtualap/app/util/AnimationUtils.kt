package com.virtualap.app.util

import androidx.compose.animation.core.*
import androidx.compose.animation.core.tween

/**
 * Centralized animation utilities for consistent app feel.
 * Implements "Premium & Instant" animation system.
 */
object AnimationUtils {
    // Standard durations per user request
    const val DURATION_FAST = 150
    const val DURATION_MEDIUM = 200
    const val DURATION_SLOW = 300
    const val DURATION_SCREEN_TRANSITION = 400
    const val DURATION_CARD_FADE = 180

    // Standard Easing
    val STANDARD_EASING = FastOutSlowInEasing

    // Pre-computed Specs
    fun <T> fastSpec(): TweenSpec<T> = tween(
        durationMillis = DURATION_FAST,
        easing = STANDARD_EASING
    )

    fun <T> mediumSpec(): TweenSpec<T> = tween(
        durationMillis = DURATION_MEDIUM,
        easing = STANDARD_EASING
    )

    fun <T> slowSpec(): TweenSpec<T> = tween(
        durationMillis = DURATION_SLOW,
        easing = STANDARD_EASING
    )

    fun <T> screenTransitionSpec(): TweenSpec<T> = tween(
        durationMillis = DURATION_SCREEN_TRANSITION,
        easing = STANDARD_EASING
    )

    fun <T> cardFadeSpec(): TweenSpec<T> = tween(
        durationMillis = DURATION_CARD_FADE,
        easing = LinearOutSlowInEasing
    )

    fun <T> fadeInSpec(): TweenSpec<T> = tween(
        durationMillis = DURATION_SCREEN_TRANSITION,
        easing = STANDARD_EASING
    )

    fun <T> fadeOutSpec(): TweenSpec<T> = tween(
        durationMillis = DURATION_FAST,
        easing = FastOutLinearInEasing
    )

    // Specific use cases
    const val SCREEN_TRANSITION_DURATION = DURATION_FAST
    const val ELEMENT_EXPANSION_DURATION = DURATION_MEDIUM
    const val CROSSFADE_DURATION = DURATION_FAST
}

