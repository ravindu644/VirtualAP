package com.virtualap.app.ui.screen

import android.os.Build
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.virtualap.app.R
import com.virtualap.app.ui.component.SwitchItem
import com.virtualap.app.ui.theme.ThemePalette
import com.virtualap.app.ui.viewmodel.AppViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    appVm: AppViewModel,
    onBack: () -> Unit = {}
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(stringResource(R.string.settings_title), fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.back))
                    }
                }
            )
        }
    ) { innerPadding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
            contentPadding = PaddingValues(vertical = 16.dp)
        ) {
            item { SettingsSectionHeader(stringResource(R.string.appearance_header)) }

            item {
                SettingsCard {
                    SwitchItem(
                        label = stringResource(R.string.follow_system_theme_label),
                        subtitle = stringResource(R.string.follow_system_theme_desc),
                        icon = Icons.Default.Brightness4,
                        checked = appVm.followSystemTheme,
                        onCheckedChange = { appVm.setFollowSystemTheme(it) }
                    )
                    AnimatedVisibility(
                        visible = !appVm.followSystemTheme,
                        enter = fadeIn() + expandVertically(),
                        exit = fadeOut() + shrinkVertically()
                    ) {
                        Column {
                            HorizontalDivider(modifier = Modifier.padding(start = 56.dp))
                            SwitchItem(
                                label = stringResource(R.string.dark_mode_label),
                                subtitle = stringResource(R.string.dark_mode_desc),
                                icon = Icons.Default.DarkMode,
                                checked = appVm.darkThemeEnabled,
                                onCheckedChange = { appVm.setDarkTheme(it) }
                            )
                            AnimatedVisibility(
                                visible = appVm.darkThemeEnabled,
                                enter = fadeIn() + expandVertically(),
                                exit = fadeOut() + shrinkVertically()
                            ) {
                                Column {
                                    HorizontalDivider(modifier = Modifier.padding(start = 56.dp))
                                    SwitchItem(
                                        label = stringResource(R.string.amoled_mode_label),
                                        subtitle = stringResource(R.string.amoled_mode_desc),
                                        icon = Icons.Default.PhoneAndroid,
                                        checked = appVm.amoledMode,
                                        onCheckedChange = { appVm.setAmoledMode(it) }
                                    )
                                }
                            }
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                                HorizontalDivider(modifier = Modifier.padding(start = 56.dp))
                                SwitchItem(
                                    label = stringResource(R.string.dynamic_color_label),
                                    subtitle = stringResource(R.string.dynamic_color_desc),
                                    icon = Icons.Default.Palette,
                                    checked = appVm.dynamicColor,
                                    onCheckedChange = { appVm.setDynamicColor(it) }
                                )
                            }
                        }
                    }
                }
            }

            val showPalette = !appVm.followSystemTheme &&
                (!appVm.dynamicColor || Build.VERSION.SDK_INT < Build.VERSION_CODES.S)
            item {
                AnimatedVisibility(
                    visible = showPalette,
                    enter = fadeIn() + expandVertically(),
                    exit = fadeOut() + shrinkVertically()
                ) {
                    Column(verticalArrangement = androidx.compose.foundation.layout.Arrangement.spacedBy(12.dp)) {
                        SettingsSectionHeader(stringResource(R.string.color_palette_header))
                        PalettePicker(
                            selected = appVm.themePalette,
                            onSelect = { appVm.setThemePalette(it) }
                        )
                    }
                }
            }
        }
    }
}


@Composable
private fun SettingsSectionHeader(text: String) {
    Text(
        text = text,
        style = MaterialTheme.typography.labelMedium,
        color = MaterialTheme.colorScheme.primary,
        fontWeight = FontWeight.SemiBold,
        modifier = Modifier.padding(start = 4.dp, bottom = 4.dp)
    )
}

@Composable
private fun SettingsCard(content: @Composable ColumnScope.() -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) { content() }
    }
}

@Composable
private fun PalettePicker(
    selected: ThemePalette,
    onSelect: (ThemePalette) -> Unit
) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        ThemePalette.entries.chunked(3).forEach { row ->
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                row.forEach { palette ->
                    val isSelected = palette == selected
                    Card(
                        modifier = Modifier
                            .weight(1f)
                            .clip(RoundedCornerShape(12.dp))
                            .clickable { onSelect(palette) }
                            .then(
                                if (isSelected) Modifier.border(
                                    2.dp,
                                    MaterialTheme.colorScheme.primary,
                                    RoundedCornerShape(12.dp)
                                ) else Modifier
                            ),
                        shape = RoundedCornerShape(12.dp),
                        colors = CardDefaults.cardColors(
                            containerColor = if (isSelected)
                                MaterialTheme.colorScheme.primaryContainer
                            else
                                MaterialTheme.colorScheme.surfaceContainer
                        )
                    ) {
                        Column(
                            modifier = Modifier.padding(12.dp),
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                                listOf(palette.primaryDark, palette.secondaryDark, palette.tertiaryDark)
                                    .forEach { color ->
                                        Box(modifier = Modifier.size(16.dp).clip(CircleShape)) {
                                            Surface(
                                                modifier = Modifier.fillMaxSize(),
                                                color = color
                                            ) {}
                                        }
                                    }
                            }
                            Text(
                                text = palette.displayName,
                                style = MaterialTheme.typography.labelSmall,
                                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
                                color = if (isSelected)
                                    MaterialTheme.colorScheme.onPrimaryContainer
                                else
                                    MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
                repeat(3 - row.size) { Spacer(modifier = Modifier.weight(1f)) }
            }
        }
    }
}
