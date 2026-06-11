package com.virtualap.app.ui.screen

import androidx.compose.animation.animateContentSize
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.virtualap.app.util.RootStatus
import kotlinx.coroutines.delay

@Composable
fun RootCheckScreen(
    rootStatus: RootStatus = RootStatus.Checking,
    onCheckRoot: () -> Unit = {},
    onNavigateNext: () -> Unit = {}
) {
    var titleVisible by remember { mutableStateOf(false) }
    var cardVisible by remember { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        delay(60); titleVisible = true
        delay(120); cardVisible = true
    }

    val titleAlpha by animateFloatAsState(
        if (titleVisible) 1f else 0f,
        tween(durationMillis = 400, easing = FastOutSlowInEasing),
        label = "titleAlpha"
    )
    val cardAlpha by animateFloatAsState(
        if (cardVisible) 1f else 0f,
        tween(durationMillis = 400, easing = FastOutSlowInEasing),
        label = "cardAlpha"
    )

    val btnShape = RoundedCornerShape(20.dp)

    Scaffold(
        containerColor = Color.Transparent,
        bottomBar = {
            Surface(
                modifier = Modifier.fillMaxWidth(),
                color = MaterialTheme.colorScheme.surfaceContainer,
                tonalElevation = 0.dp
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    HorizontalDivider(
                        color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.25f),
                        thickness = 1.dp
                    )
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(20.dp)
                            .navigationBarsPadding()
                            .animateContentSize(animationSpec = tween(150, easing = FastOutSlowInEasing)),
                        verticalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        // Main Action Button
                        Surface(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(btnShape)
                                .clickable(
                                    enabled = rootStatus != RootStatus.Checking,
                                    onClick = {
                                        if (rootStatus == RootStatus.Granted) onNavigateNext()
                                        else onCheckRoot()
                                    }
                                ),
                            shape = btnShape,
                            color = MaterialTheme.colorScheme.primary,
                            tonalElevation = 0.dp
                        ) {
                            Box(modifier = Modifier.padding(vertical = 16.dp), contentAlignment = Alignment.Center) {
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    if (rootStatus == RootStatus.Checking) {
                                        CircularProgressIndicator(
                                            modifier = Modifier.size(18.dp),
                                            strokeWidth = 2.dp,
                                            color = MaterialTheme.colorScheme.onPrimary
                                        )
                                    } else {
                                        Icon(
                                            imageVector = if (rootStatus == RootStatus.Granted)
                                                Icons.Default.CheckCircle else Icons.Default.Shield,
                                            contentDescription = null,
                                            modifier = Modifier.size(18.dp),
                                            tint = MaterialTheme.colorScheme.onPrimary
                                        )
                                    }
                                    Text(
                                        text = when (rootStatus) {
                                            RootStatus.Granted -> "Continue"
                                            RootStatus.Checking -> "Checking…"
                                            RootStatus.Denied -> "Grant Root Access"
                                        },
                                        style = MaterialTheme.typography.labelLarge,
                                        fontWeight = FontWeight.SemiBold,
                                        fontSize = 16.sp,
                                        color = MaterialTheme.colorScheme.onPrimary
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .padding(24.dp)
                .verticalScroll(rememberScrollState()),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(32.dp, Alignment.CenterVertically)
        ) {
            // Title
            Text(
                text = "Root Access Required",
                style = MaterialTheme.typography.headlineMedium,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center,
                modifier = Modifier.alpha(titleAlpha)
            )

            // Status Card
            val cardAccentColor = when (rootStatus) {
                RootStatus.Granted -> MaterialTheme.colorScheme.primary
                RootStatus.Denied -> MaterialTheme.colorScheme.error
                else -> MaterialTheme.colorScheme.onSurfaceVariant
            }

            Surface(
                modifier = Modifier.fillMaxWidth().alpha(cardAlpha),
                shape = RoundedCornerShape(20.dp),
                color = MaterialTheme.colorScheme.surfaceContainer,
                border = BorderStroke(1.dp, cardAccentColor.copy(alpha = 0.3f))
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth().padding(24.dp),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(16.dp)
                ) {
                    when (rootStatus) {
                        RootStatus.Checking -> {
                            CircularProgressIndicator(
                                modifier = Modifier.size(48.dp),
                                strokeWidth = 4.dp
                            )
                            Text(
                                text = "Checking root access…",
                                style = MaterialTheme.typography.bodyMedium,
                                textAlign = TextAlign.Center
                            )
                        }
                        RootStatus.Granted -> {
                            Surface(
                                shape = RoundedCornerShape(12.dp),
                                color = MaterialTheme.colorScheme.primary.copy(alpha = 0.1f),
                                tonalElevation = 0.dp
                            ) {
                                Icon(
                                    imageVector = Icons.Default.CheckCircle,
                                    contentDescription = null,
                                    modifier = Modifier.padding(12.dp).size(32.dp),
                                    tint = MaterialTheme.colorScheme.primary
                                )
                            }
                            Text(
                                text = "Root access granted",
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.Bold,
                                color = MaterialTheme.colorScheme.primary,
                                textAlign = TextAlign.Center
                            )
                            Text(
                                text = "VirtualAP can now manage network interfaces.",
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                textAlign = TextAlign.Center
                            )
                        }
                        RootStatus.Denied -> {
                            Surface(
                                shape = RoundedCornerShape(12.dp),
                                color = MaterialTheme.colorScheme.error.copy(alpha = 0.1f),
                                tonalElevation = 0.dp
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Error,
                                    contentDescription = null,
                                    modifier = Modifier.padding(12.dp).size(32.dp),
                                    tint = MaterialTheme.colorScheme.error
                                )
                            }
                            Text(
                                text = "Root access denied",
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.Bold,
                                color = MaterialTheme.colorScheme.error,
                                textAlign = TextAlign.Center
                            )
                            Text(
                                text = "VirtualAP requires root access to create virtual network interfaces and manage the AP.",
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                textAlign = TextAlign.Center
                            )
                        }
                    }
                }
            }


        }
    }
}
