package com.virtualap.app.ui.component

import android.util.Log
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.LinearOutSlowInEasing
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentWidth
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.virtualap.app.ui.theme.JetBrainsMono
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

private val LeadingSpaces = Regex("^ +")

/**
 * The console's frame: surfaceContainerHighest with a diagonal sweep of light while
 * [enabled]. Both animations are read in the draw phase only, so the sweep repaints
 * without recomposing the lines it frames; it runs while enabled and fades out over
 * 800 ms after, which is what keeps a scroll animation in flight at that moment smooth.
 */
@Composable
fun ShimmerAnimation(
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    content: @Composable () -> Unit
) {
    val shape = RoundedCornerShape(16.dp)
    val shade = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.15f)
    val alpha = remember { Animatable(if (enabled) 1f else 0f) }
    val sweep = remember { Animatable(0f) }
    LaunchedEffect(enabled) {
        launch { alpha.animateTo(if (enabled) 1f else 0f, tween(durationMillis = 800, easing = LinearOutSlowInEasing)) }
        while (enabled) {
            sweep.animateTo(1000f, tween(durationMillis = 1200, easing = FastOutSlowInEasing))
            sweep.animateTo(0f, tween(durationMillis = 1200, easing = FastOutSlowInEasing))
        }
    }

    Surface(
        modifier = modifier
            .clip(shape)
            .background(MaterialTheme.colorScheme.surfaceContainerHighest)
            .drawBehind {
                if (alpha.value == 0f) return@drawBehind
                val end = sweep.value
                drawRect(
                    Brush.linearGradient(
                        colors = listOf(Color.Transparent, shade.copy(alpha = shade.alpha * alpha.value), Color.Transparent),
                        start = Offset(10f, 10f),
                        end = Offset(end, end)
                    )
                )
            }
            .border(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.4f), shape),
        color = Color.Transparent,
        shape = shape,
        tonalElevation = 0.dp,
        shadowElevation = 0.dp
    ) {
        content()
    }
}

/**
 * Scrolling log view that follows the tail while a command runs.
 *
 * Scroll design invariants:
 * - isProcessing IS part of the snapshotFlow tuple. Reading it from closure instead causes
 *   a stale-capture race: the branch (spring vs instant) is decided on an old value, two
 *   scroll coroutines run concurrently, and the spring gets cancelled mid-animation (~500ms
 *   stall before the final log lines appear, reproducible ~1/50 on start/stop sequences).
 * - scrollJob?.cancel() before each launch guarantees at most one scroll coroutine is live.
 *   Rapid log bursts or an isProcessing flip cannot queue up concurrent animateScrollTo calls.
 * - LaunchedEffect(isProcessing) resets state only on start (isProcessing=true). Resetting
 *   on finish produced a spurious snapshotFlow re-emission that was the entry point of the race.
 * - userScrolledUp is detected via a separate read-only snapshotFlow - no scroll mutation
 *   conflicts with the write path in the scroll LaunchedEffect.
 */
@Composable
fun TerminalConsole(
    logs: List<Pair<Int, String>>,
    isProcessing: Boolean = true,
    modifier: Modifier = Modifier,
    maxHeight: Dp? = null
) {
    val orientation = LocalConfiguration.current.orientation
    val verticalScrollState = rememberScrollState()
    val horizontalScrollState = rememberScrollState()

    // After rotation, snap to bottom so auto-scroll resumes cleanly
    LaunchedEffect(orientation) {
        verticalScrollState.scrollTo(verticalScrollState.maxValue)
    }

    var userScrolledUp by remember { mutableStateOf(false) }
    var isAutoScrolling by remember { mutableStateOf(false) }

    // Read-only observer - never mutates scroll state, so zero conflict with animations
    LaunchedEffect(verticalScrollState) {
        snapshotFlow { verticalScrollState.value }
            .collect { value ->
                // Only track user scrolling if we aren't currently auto-scrolling.
                // This prevents the animation itself from triggering "userScrolledUp"
                // due to measurement delays or extreme log bursts.
                if (!isAutoScrolling) {
                    userScrolledUp = value < verticalScrollState.maxValue - 200
                }
            }
    }

    LaunchedEffect(Unit) {
        var scrollJob: Job? = null
        snapshotFlow {
            Pair(
                Triple(logs.size, verticalScrollState.maxValue, userScrolledUp),
                isProcessing
            )
        }.collect { (triple, processing) ->
            val (_, maxValue, scrolledUp) = triple
            if (!scrolledUp && verticalScrollState.value < maxValue) {
                scrollJob?.cancel()
                scrollJob = launch {
                    isAutoScrolling = true
                    try {
                        if (processing) {
                            verticalScrollState.animateScrollTo(
                                value = maxValue,
                                animationSpec = spring(
                                    dampingRatio = Spring.DampingRatioNoBouncy,
                                    stiffness = Spring.StiffnessLow
                                )
                            )
                        } else {
                            // Instant snap after processing ends: layout is stable by now
                            // and no spring is in flight (scrollJob?.cancel() above killed it).
                            verticalScrollState.scrollTo(maxValue)
                        }
                    } finally {
                        isAutoScrolling = false
                    }
                }
            }
        }
    }

    // Reset scroll state only when a NEW operation starts, never on finish.
    LaunchedEffect(isProcessing) {
        if (isProcessing) {
            userScrolledUp = false
            isAutoScrolling = false
        }
    }

    val defaultTextColor = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.9f)
    val errorColor = MaterialTheme.colorScheme.error.copy(alpha = 0.9f)
    val warnColor = MaterialTheme.colorScheme.tertiary.copy(alpha = 0.9f)

    ShimmerAnimation(
        modifier = if (maxHeight != null) modifier.heightIn(max = maxHeight) else modifier,
        enabled = isProcessing
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 16.dp)
                .verticalScroll(verticalScrollState)
                .horizontalScroll(horizontalScrollState)
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                logs.forEach { (level, message) ->
                    Text(
                        // Leading spaces are the script's indentation; non-breaking ones survive layout.
                        text = message.ifEmpty { "\u00A0" }.replace(LeadingSpaces) { it.value.replace(" ", "\u00A0") },
                        style = MaterialTheme.typography.bodySmall.copy(fontFamily = JetBrainsMono),
                        color = when (level) {
                            Log.ERROR -> errorColor
                            Log.WARN -> warnColor
                            else -> defaultTextColor
                        },
                        softWrap = false,
                        modifier = Modifier
                            .wrapContentWidth()
                            .heightIn(min = 16.dp)
                    )
                }
            }
        }
    }
}
