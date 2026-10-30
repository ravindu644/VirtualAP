package com.virtualap.app.ui.screen

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.pager.PagerState
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowForward
import androidx.compose.material.icons.filled.Router
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialShapes
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.material3.toPath
import androidx.compose.runtime.State
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.geometry.center
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Matrix
import androidx.compose.ui.graphics.Outline
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.graphics.shapes.Morph
import androidx.graphics.shapes.RoundedPolygon
import com.virtualap.app.R
import com.virtualap.app.ui.component.SetupActionBar
import com.virtualap.app.ui.component.SetupFrame
import com.virtualap.app.ui.component.SetupFrameHeight
import com.virtualap.app.ui.component.SetupGlyphSize
import com.virtualap.app.ui.component.SetupHeroSize
import com.virtualap.app.ui.component.SetupPage

private class Feature(val icon: ImageVector, val titleRes: Int, val descRes: Int)

private val Features = listOf(
    Feature(Icons.Default.Wifi, R.string.feat_upstream_title, R.string.feat_upstream_desc),
    Feature(Icons.Default.Router, R.string.feat_openwrt_title, R.string.feat_openwrt_desc),
    Feature(Icons.Default.Tune, R.string.feat_radio_title, R.string.feat_radio_desc),
)

/* One preset per page, so a swipe always morphs between two different shapes. The welcome
 * hero is a soft burst that settles into the first. */
private val PageShapes = listOf(MaterialShapes.Cookie9Sided, MaterialShapes.Clover4Leaf, MaterialShapes.Sunny)

/**
 * The hero's clip: the soft burst morphing into the first page's preset while [enter] runs,
 * then the preset of the page on the left morphing into the one on the right as the pager
 * scrolls. Both states are read here, in the draw phase, so a swipe repaints the outline
 * without recomposing the screen. The presets are normalised to a unit box, so the path is
 * scaled to the layout and re-centred, since a morph's bounds drift as it animates.
 */
private class HeroShape(private val pager: PagerState, private val enter: State<Float>) : Shape {
    private val morphs = HashMap<Pair<Int, Int>, Morph>()
    private fun morph(key: Pair<Int, Int>, a: RoundedPolygon, b: RoundedPolygon) =
        morphs.getOrPut(key) { Morph(a, b) }

    override fun createOutline(size: Size, layoutDirection: LayoutDirection, density: Density): Outline {
        val page = pager.currentPage
        val entered = enter.value.coerceIn(0f, 1f)
        val morph: Morph
        val progress: Float
        if (entered < 1f) {
            // Back keeps the page, so the burst returns from whatever shape the user left on.
            morph = morph(-1 to page, MaterialShapes.SoftBurst, PageShapes[page]); progress = entered
        } else {
            // The pair is always (lower page, higher page) and progress the position between
            // them. currentPage flips at half a page, and both sides of the flip read the same
            // pair at the same progress, so the shape never jumps.
            val offset = pager.currentPageOffsetFraction
            val lower = if (offset >= 0f) page else (page - 1).coerceAtLeast(0)
            val higher = (lower + 1).coerceAtMost(Features.lastIndex)
            morph = morph(lower to higher, PageShapes[lower], PageShapes[higher])
            progress = if (offset >= 0f) offset else if (page == 0) 0f else 1f + offset
        }
        val path = morph.toPath(progress)
        val matrix = Matrix()
        matrix.scale(size.width, size.height)
        path.transform(matrix)
        path.translate(size.center - path.getBounds().center)
        return Outline.Generic(path)
    }
}

/**
 * The headline and the feature pager are one screen on purpose: the hero stays in its
 * slot while Get started morphs the soft burst into the first cookie and fades the logo
 * into the first feature glyph. Shown once; [onDone] records that.
 */
@Composable
fun WelcomeScreen(onDone: () -> Unit) {
    var started by rememberSaveable { mutableStateOf(false) }
    BackHandler(enabled = started) { started = false }

    val motion = MaterialTheme.motionScheme
    val pagerState = rememberPagerState { Features.size }
    val page = pagerState.currentPage

    // The spatial spring overshoots; HeroShape clamps it, since Morph only means anything in 0..1.
    val enter = animateFloatAsState(
        targetValue = if (started) 1f else 0f,
        animationSpec = motion.defaultSpatialSpec(),
        label = "hero_enter"
    )
    val heroShape = remember(pagerState) { HeroShape(pagerState, enter) }

    Scaffold(
        containerColor = Color.Transparent,
        bottomBar = {
            SetupActionBar(
                label = stringResource(if (started) R.string.next else R.string.get_started),
                icon = Icons.AutoMirrored.Filled.ArrowForward,
                onClick = { if (started) onDone() else started = true }
            )
        }
    ) { innerPadding ->
        SetupPage(
            innerPadding = innerPadding,
            above = {
                AnimatedVisibility(
                    visible = started,
                    enter = fadeIn(motion.defaultEffectsSpec()),
                    exit = fadeOut(motion.defaultEffectsSpec())
                ) {
                    Text(
                        text = stringResource(R.string.page_counter, page + 1, Features.size),
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            },
            below = {
                AnimatedVisibility(
                    visible = started,
                    enter = fadeIn(motion.defaultEffectsSpec()),
                    exit = fadeOut(motion.defaultEffectsSpec())
                ) {
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalAlignment = Alignment.CenterVertically) {
                        Features.indices.forEach { i ->
                            val selected = i == page
                            val width by animateDpAsState(
                                targetValue = if (selected) 24.dp else 8.dp,
                                animationSpec = motion.defaultSpatialSpec(),
                                label = "dot"
                            )
                            Box(
                                modifier = Modifier
                                    .size(width = width, height = 8.dp)
                                    .clip(CircleShape)
                                    .background(
                                        if (selected) MaterialTheme.colorScheme.primary
                                        else MaterialTheme.colorScheme.outlineVariant
                                    )
                            )
                        }
                    }
                }
            }
        ) {
            // The shape sits behind the frame and never moves. The pages slide over it.
            Column(modifier = Modifier.height(SetupFrameHeight)) {
                Box(
                    modifier = Modifier
                        .size(SetupHeroSize)
                        .clip(heroShape)
                        .background(MaterialTheme.colorScheme.primaryContainer)
                )
            }
            AnimatedContent(
                targetState = started,
                transitionSpec = {
                    fadeIn(motion.defaultEffectsSpec()).togetherWith(fadeOut(motion.defaultEffectsSpec()))
                },
                contentAlignment = Alignment.Center,
                label = "welcome_content"
            ) { pager ->
                if (pager) {
                    HorizontalPager(state = pagerState, modifier = Modifier.fillMaxWidth().height(SetupFrameHeight)) { i ->
                        val feature = Features[i]
                        SetupFrame(
                            title = stringResource(feature.titleRes),
                            body = stringResource(feature.descRes)
                        ) {
                            Icon(
                                imageVector = feature.icon,
                                contentDescription = null,
                                modifier = Modifier.size(SetupGlyphSize),
                                tint = MaterialTheme.colorScheme.onPrimaryContainer
                            )
                        }
                    }
                } else {
                    SetupFrame(
                        title = stringResource(R.string.app_subtitle),
                        body = stringResource(R.string.welcome_body),
                        titleStyle = MaterialTheme.typography.displaySmall
                    ) {
                        // The adaptive icon keeps its artwork inside the safe zone, so
                        // drawing it at the hero size lands the glyph near the glyph size.
                        Icon(
                            painter = painterResource(R.drawable.ic_launcher_foreground),
                            contentDescription = null,
                            modifier = Modifier.size(SetupHeroSize),
                            tint = MaterialTheme.colorScheme.onPrimaryContainer
                        )
                    }
                }
            }
        }
    }
}
