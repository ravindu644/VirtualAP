package com.virtualap.app.ui.screen

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.OpenInNew
import androidx.compose.material.icons.filled.BrightnessAuto
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.ColorLens
import androidx.compose.material.icons.filled.DarkMode
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.RadioButtonUnchecked
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.virtualap.app.R
import com.virtualap.app.ui.component.AccentColorPicker
import com.virtualap.app.ui.component.DialogCloseButton
import com.virtualap.app.ui.component.DsDialog
import com.virtualap.app.ui.component.SectionHeader
import com.virtualap.app.ui.component.SwitchItem
import com.virtualap.app.ui.theme.rememberThemeState
import com.virtualap.app.util.PreferencesManager

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    onBack: () -> Unit = {}
) {
    val context = LocalContext.current
    // Writes go straight to prefs; the SharedPreferences listener behind
    // rememberThemeState() feeds them back into the theme, no restart needed.
    val prefs = remember { PreferencesManager.getInstance(context) }
    val themeState = rememberThemeState()
    var showAboutDialog by remember { mutableStateOf(false) }

    Scaffold(
        containerColor = Color.Transparent,
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        text = stringResource(R.string.settings_title),
                        style = MaterialTheme.typography.titleLarge,
                        fontWeight = FontWeight.Bold
                    )
                },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = stringResource(R.string.back))
                    }
                }
            )
        }
    ) { innerPadding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .verticalScroll(rememberScrollState())
        ) {
            SectionHeader(
                text = stringResource(R.string.appearance_header),
                modifier = Modifier.padding(start = 24.dp, top = 8.dp, bottom = 8.dp)
            )
            SettingsGroup(raised = themeState.darkTheme, modifier = Modifier.padding(horizontal = 16.dp)) {
                SwitchItem(
                    icon = Icons.Default.BrightnessAuto,
                    title = stringResource(R.string.follow_system_theme_label),
                    summary = stringResource(R.string.follow_system_theme_desc),
                    checked = themeState.followSystemTheme,
                    onCheckedChange = { prefs.followSystemTheme = it }
                )
                if (!themeState.followSystemTheme) {
                    GroupDivider()
                    SwitchItem(
                        icon = Icons.Default.DarkMode,
                        title = stringResource(R.string.dark_mode_label),
                        summary = stringResource(R.string.dark_mode_desc),
                        checked = themeState.darkTheme,
                        onCheckedChange = { prefs.darkTheme = it }
                    )
                }
                if (themeState.followSystemTheme || themeState.darkTheme) {
                    GroupDivider()
                    SwitchItem(
                        icon = Icons.Default.RadioButtonUnchecked,
                        title = stringResource(R.string.amoled_mode_label),
                        summary = stringResource(R.string.amoled_mode_desc),
                        checked = themeState.amoledMode,
                        onCheckedChange = { prefs.amoledMode = it }
                    )
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    GroupDivider()
                    SwitchItem(
                        icon = Icons.Default.ColorLens,
                        title = stringResource(R.string.dynamic_color_label),
                        summary = stringResource(R.string.dynamic_color_desc),
                        checked = themeState.useDynamicColor,
                        onCheckedChange = { prefs.useDynamicColor = it }
                    )
                }
                if (!themeState.useDynamicColor || Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                    GroupDivider()
                    AccentColorPicker(
                        selectedPalette = themeState.themePalette,
                        isDarkTheme = themeState.darkTheme,
                        onPaletteSelected = { prefs.themePalette = it.name }
                    )
                }
            }

            Spacer(modifier = Modifier.height(8.dp))
            SectionHeader(
                text = stringResource(R.string.about_header),
                modifier = Modifier.padding(start = 24.dp, top = 8.dp, bottom = 8.dp)
            )
            SettingsGroup(raised = themeState.darkTheme, modifier = Modifier.padding(horizontal = 16.dp)) {
                ListItem(
                    colors = ListItemDefaults.colors(containerColor = Color.Transparent),
                    leadingContent = { Icon(imageVector = Icons.Default.Info, contentDescription = null) },
                    headlineContent = {
                        Text(
                            text = stringResource(R.string.about_virtualap),
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.SemiBold
                        )
                    },
                    supportingContent = { Text(getAppVersion(context)) },
                    modifier = Modifier.clickable { showAboutDialog = true }
                )
            }
            Spacer(modifier = Modifier.height(24.dp))
        }
    }

    if (showAboutDialog) {
        AboutDialog(onDismiss = { showAboutDialog = false })
    }
}

/** The grouped surface every settings section sits in (mirrors Droidspaces). */
@Composable
private fun SettingsGroup(raised: Boolean, modifier: Modifier = Modifier, content: @Composable () -> Unit) {
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(24.dp),
        color = if (raised) MaterialTheme.colorScheme.surfaceContainerHigh else MaterialTheme.colorScheme.surfaceContainer,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.35f)),
        tonalElevation = 0.dp
    ) {
        Column { content() }
    }
}

@Composable
private fun GroupDivider() {
    HorizontalDivider(
        modifier = Modifier.padding(horizontal = 16.dp),
        color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.3f)
    )
}

@Composable
private fun AboutDialog(onDismiss: () -> Unit) {
    val context = LocalContext.current
    // Deviates from the full-width dismiss rule: About is an info page, not a
    // decision, so it closes from the header like the log sheet.
    DsDialog(onDismiss = onDismiss) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(
                    text = stringResource(R.string.app_name),
                    style = MaterialTheme.typography.headlineSmall,
                    fontWeight = FontWeight.Bold
                )
                Text(
                    text = stringResource(R.string.app_subtitle),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
                )
            }
            DialogCloseButton(onClick = onDismiss)
        }
        Text(
            text = stringResource(R.string.about_description),
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        SectionHeader(text = stringResource(R.string.maintainer_header))
        SettingsGroup(raised = true) {
            LinkRow(
                icon = Icons.Default.Person,
                title = stringResource(R.string.maintainer_name),
                subtitle = stringResource(R.string.maintainer_role),
                onClick = { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/ravindu644"))) }
            )
            GroupDivider()
            LinkRow(
                icon = Icons.Default.Code,
                title = stringResource(R.string.source_code),
                onClick = { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/ravindu644/VirtualAP"))) }
            )
        }
    }
}

@Composable
private fun LinkRow(icon: ImageVector, title: String, subtitle: String? = null, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            modifier = Modifier.size(24.dp),
            tint = MaterialTheme.colorScheme.primary
        )
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
            if (subtitle != null) {
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
        Icon(
            imageVector = Icons.AutoMirrored.Filled.OpenInNew,
            contentDescription = title,
            modifier = Modifier.size(18.dp),
            tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
        )
    }
}

private fun getAppVersion(context: Context): String {
    return try {
        val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)
        packageInfo.versionName ?: context.getString(R.string.unknown)
    } catch (e: PackageManager.NameNotFoundException) {
        context.getString(R.string.unknown)
    }
}
