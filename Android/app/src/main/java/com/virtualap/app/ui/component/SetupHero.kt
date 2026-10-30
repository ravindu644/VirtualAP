package com.virtualap.app.ui.component

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialShapes
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.toShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.virtualap.app.ui.util.LoadingIndicator
import com.virtualap.app.ui.util.LoadingSize

/* One geometry for the root check and the installer, so the hero, the title and the copy
 * land on the same rows on both. The text block is a fixed box so the frame has one height
 * whatever the copy does. */
private val HeroSize = 240.dp
private val GlyphSize = 96.dp
private val TextBlockHeight = 200.dp

/** The slot under a setup screen's Scaffold: centres [content] when it fits, scrolls when not. */
@Composable
fun SetupPage(innerPadding: PaddingValues, content: @Composable BoxScope.() -> Unit) {
    BoxWithConstraints(
        modifier = Modifier
            .fillMaxSize()
            .padding(innerPadding)
            .padding(horizontal = 24.dp)
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .heightIn(min = maxHeight)
                .verticalScroll(rememberScrollState()),
            contentAlignment = Alignment.Center,
            content = content
        )
    }
}

/**
 * The frame inside the slot: [hero] in the 240dp hero box, then [title] and [body] in a fixed
 * text block that scrolls if the copy ever outgrows it. Title, copy and its colour fade
 * together on the hero's spec, so a state change lands as one beat.
 */
@Composable
fun SetupFrame(
    title: String,
    body: String,
    bodyColor: Color = MaterialTheme.colorScheme.onSurfaceVariant,
    hero: @Composable BoxScope.() -> Unit
) {
    Column(
        modifier = Modifier.fillMaxWidth().height(HeroSize + 32.dp + TextBlockHeight),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(modifier = Modifier.size(HeroSize), contentAlignment = Alignment.Center, content = hero)
        Spacer(modifier = Modifier.height(32.dp))
        val motion = MaterialTheme.motionScheme
        AnimatedContent(
            targetState = Triple(title, body, bodyColor),
            transitionSpec = {
                fadeIn(motion.fastEffectsSpec()).togetherWith(fadeOut(motion.fastEffectsSpec()))
            },
            contentAlignment = Alignment.TopCenter,
            label = "setup_copy"
        ) { (currentTitle, currentBody, currentColor) ->
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(TextBlockHeight)
                    .verticalScroll(rememberScrollState()),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text(
                    text = currentTitle,
                    style = MaterialTheme.typography.headlineMedium,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center
                )
                Spacer(modifier = Modifier.height(12.dp))
                Text(
                    text = currentBody,
                    style = MaterialTheme.typography.bodyLarge,
                    color = currentColor,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }
    }
}

/**
 * The hero of a screen that does work: a twelve-sided cookie tinted [container] holding
 * the loader while [loading], otherwise [glyph]. The container stays put through every
 * state and only its colour and its content change, so a result reads as the same thing
 * settling rather than a swap. State lives in the colour: a failure is red, success and
 * idle are the primary container.
 */
@Composable
fun SetupHero(
    loading: Boolean,
    glyph: ImageVector,
    container: Color,
    onContainer: Color
) {
    val motion = MaterialTheme.motionScheme
    val tint by animateColorAsState(container, motion.fastEffectsSpec(), label = "hero_tint")
    Box(
        modifier = Modifier
            .size(HeroSize)
            .clip(MaterialShapes.Cookie12Sided.toShape())
            .background(tint),
        contentAlignment = Alignment.Center
    ) {
        // A plain fade in a box of one size. Letting the switch animate between the
        // loader's box and the glyph's box slides the glyph into place from the top.
        AnimatedContent(
            targetState = if (loading) null else glyph,
            transitionSpec = {
                fadeIn(motion.fastEffectsSpec()).togetherWith(fadeOut(motion.fastEffectsSpec()))
            },
            contentAlignment = Alignment.Center,
            label = "setup_hero"
        ) { icon ->
            Box(modifier = Modifier.size(HeroSize), contentAlignment = Alignment.Center) {
                if (icon == null) {
                    LoadingIndicator(size = LoadingSize.Hero, color = onContainer)
                } else {
                    Icon(
                        imageVector = icon,
                        contentDescription = null,
                        modifier = Modifier.size(GlyphSize),
                        tint = onContainer
                    )
                }
            }
        }
    }
}

/**
 * The setup flow's one button. DESIGN.md's 20dp bar button with a divider is for screens
 * with content scrolling under it; here the button sits alone on the background under a
 * hero, so it is a full pill with no bar surface. Disabled keeps the enabled colours: a
 * setup button is only disabled for the beat a check or install is in flight, and greying
 * it reads as a flicker. A decided exception, listed in DESIGN.md.
 */
@Composable
fun SetupActionBar(
    label: String,
    icon: ImageVector,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true
) {
    PrimaryActionBottomBar(
        label = label,
        icon = icon,
        onClick = onClick,
        modifier = modifier,
        enabled = enabled,
        disabledContainerColor = MaterialTheme.colorScheme.primary,
        disabledContentColor = MaterialTheme.colorScheme.onPrimary,
        barColor = Color.Transparent,
        dividerAlpha = 0f,
        shape = CircleShape
    )
}
