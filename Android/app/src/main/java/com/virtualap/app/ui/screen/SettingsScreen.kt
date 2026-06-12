package com.virtualap.app.ui.screen

import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.OpenInNew
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.virtualap.app.R
import com.virtualap.app.ui.component.SwitchItem
import com.virtualap.app.ui.theme.ThemePalette
import com.virtualap.app.ui.viewmodel.APViewModel
import com.virtualap.app.ui.viewmodel.AppViewModel

// IPv4 regex: A.B.C.D where each octet is 0-255, last octet not 0 or 255
private val ipv4Regex = Regex(
    "^(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)" +
    "\\.(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)" +
    "\\.(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)" +
    "\\.(25[0-4]|2[0-4]\\d|[01]?\\d[1-9]|[01]?[1-9]\\d?|[1-9])$"
)

private fun isValidIpv4(ip: String): Boolean = ipv4Regex.matches(ip.trim())

private fun isValidDnsServers(dns: String): Boolean {
    if (dns.isBlank()) return true  // empty = auto, that's fine
    return dns.split(",").all { isValidIpv4(it.trim()) }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    appVm: AppViewModel,
    apVm: APViewModel,
    onBack: () -> Unit = {}
) {
    // About dialog state
    var showAboutDialog by remember { mutableStateOf(false) }
    val focusManager = LocalFocusManager.current

    Scaffold(
        modifier = Modifier
            .fillMaxSize()
            .imePadding()
            .pointerInput(Unit) {
                detectTapGestures(onTap = { focusManager.clearFocus() })
            },
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


            item { HorizontalDivider(modifier = Modifier.padding(vertical = 4.dp)) }

            // Hotspot Settings Section
            item { SettingsSectionHeader(stringResource(R.string.hotspot_settings_header)) }
            item {
                HotspotSettingsCard(apVm = apVm)
            }


            item { HorizontalDivider(modifier = Modifier.padding(vertical = 4.dp)) }

            // About Section
            item {
                SettingsSectionHeader(stringResource(R.string.about_header))
                SettingsCard {
                    ListItem(
                        leadingContent = {
                            Icon(imageVector = Icons.Default.Info, contentDescription = null)
                        },
                        headlineContent = {
                            Text(
                                text = stringResource(R.string.about_virtualap),
                                style = MaterialTheme.typography.titleMedium,
                                fontWeight = FontWeight.SemiBold
                            )
                        },
                        supportingContent = {
                            val context = LocalContext.current
                            Text(getAppVersion(context))
                        },
                        modifier = Modifier.clickable { showAboutDialog = true }
                    )
                }
            }
        }
    }

    // About Dialog
    if (showAboutDialog) {
        AboutDialog(onDismiss = { showAboutDialog = false })
    }
}

@Composable
private fun HotspotSettingsCard(apVm: APViewModel) {
    // Local transient edit state — committed to the ViewModel on valid change
    var gatewayText by remember(apVm.config.gateway) { mutableStateOf(apVm.config.gateway) }
    var dnsText     by remember(apVm.config.dnsServers) { mutableStateOf(apVm.config.dnsServers) }

    val gatewayError = gatewayText.isNotBlank() && !isValidIpv4(gatewayText)
    val dnsError     = !isValidDnsServers(dnsText)

    SettingsCard {
        Column(
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // --- Gateway IP ---
            OutlinedTextField(
                value = gatewayText,
                onValueChange = { v ->
                    gatewayText = v
                    if (isValidIpv4(v)) {
                        apVm.config = apVm.config.copy(gateway = v.trim())
                    }
                },
                label = { Text(stringResource(R.string.gateway_ip_label)) },
                placeholder = { Text(stringResource(R.string.gateway_ip_placeholder)) },
                supportingText = {
                    if (gatewayError)
                        Text(stringResource(R.string.gateway_ip_error), color = MaterialTheme.colorScheme.error)
                    else
                        Text(stringResource(R.string.gateway_ip_desc), color = MaterialTheme.colorScheme.onSurfaceVariant)
                },
                isError = gatewayError,
                leadingIcon = {
                    Icon(Icons.Default.Router, contentDescription = null)
                },
                trailingIcon = {
                    if (gatewayError)
                        Icon(Icons.Default.Error, contentDescription = null, tint = MaterialTheme.colorScheme.error)
                },
                singleLine = true,
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Number,
                    imeAction = ImeAction.Next
                ),
                modifier = Modifier.fillMaxWidth()
            )

            // --- DNS Servers ---
            OutlinedTextField(
                value = dnsText,
                onValueChange = { v ->
                    dnsText = v
                    if (isValidDnsServers(v)) {
                        apVm.config = apVm.config.copy(dnsServers = v.trim())
                    }
                },
                label = { Text(stringResource(R.string.dns_servers_label)) },
                placeholder = { Text(stringResource(R.string.dns_servers_placeholder)) },
                supportingText = {
                    if (dnsError)
                        Text(stringResource(R.string.dns_servers_error), color = MaterialTheme.colorScheme.error)
                    else
                        Text(stringResource(R.string.dns_servers_desc), color = MaterialTheme.colorScheme.onSurfaceVariant)
                },
                isError = dnsError,
                leadingIcon = {
                    Icon(Icons.Default.Dns, contentDescription = null)
                },
                trailingIcon = {
                    if (dnsError)
                        Icon(Icons.Default.Error, contentDescription = null, tint = MaterialTheme.colorScheme.error)
                    else if (dnsText.isNotBlank() && !dnsError)
                        Icon(Icons.Default.CheckCircle, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                },
                singleLine = true,
                keyboardOptions = KeyboardOptions(
                    keyboardType = KeyboardType.Ascii,
                    imeAction = ImeAction.Done
                ),
                modifier = Modifier.fillMaxWidth()
            )
        }
        SwitchItem(
            label = stringResource(R.string.hidden_ssid_label),
            subtitle = stringResource(R.string.hidden_ssid_desc),
            icon = Icons.Default.VisibilityOff,
            checked = apVm.config.hidden,
            onCheckedChange = { apVm.config = apVm.config.copy(hidden = it) }
        )
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

@Composable
private fun AboutDialog(onDismiss: () -> Unit) {
    val context = LocalContext.current

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false)
    ) {
        Surface(
            modifier = Modifier
                .fillMaxWidth(0.9f)
                .wrapContentHeight()
                .padding(16.dp),
            shape = RoundedCornerShape(20.dp),
            color = MaterialTheme.colorScheme.surfaceContainer,
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.35f))
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(24.dp)
            ) {
                // Title
                Text(
                    text = stringResource(R.string.app_name),
                    style = MaterialTheme.typography.headlineSmall,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.padding(bottom = 16.dp)
                )

                // Content
                Column(
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Text(
                        text = stringResource(R.string.about_description),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Spacer(modifier = Modifier.height(4.dp))
                    Text(
                        text = stringResource(R.string.maintainer_header),
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold,
                        color = MaterialTheme.colorScheme.primary
                    )
                    // Maintainer
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable {
                                val intent = Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/ravindu644"))
                                context.startActivity(intent)
                            },
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.Person,
                            contentDescription = null,
                            modifier = Modifier.size(20.dp),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = stringResource(R.string.maintainer_name),
                                style = MaterialTheme.typography.bodyMedium,
                                fontWeight = FontWeight.Medium
                            )
                            Text(
                                text = stringResource(R.string.maintainer_role),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.OpenInNew,
                            contentDescription = stringResource(R.string.github),
                            modifier = Modifier.size(18.dp),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
                        )
                    }
                    Spacer(modifier = Modifier.height(4.dp))
                    Spacer(modifier = Modifier.height(4.dp))
                    // Source Code row
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable {
                                val intent = Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/ravindu644/VirtualAP"))
                                context.startActivity(intent)
                            },
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.Code,
                            contentDescription = null,
                            modifier = Modifier.size(24.dp),
                            tint = MaterialTheme.colorScheme.primary
                        )
                        Text(
                            text = stringResource(R.string.source_code),
                            style = MaterialTheme.typography.bodyMedium,
                            fontWeight = FontWeight.Medium,
                            modifier = Modifier.weight(1f)
                        )
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.OpenInNew,
                            contentDescription = stringResource(R.string.source_code),
                            modifier = Modifier.size(18.dp),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
                        )
                    }
                }

                // OK Button
                Spacer(modifier = Modifier.height(8.dp))
                TextButton(
                    onClick = onDismiss,
                    modifier = Modifier.align(Alignment.End)
                ) {
                    Text(stringResource(R.string.ok))
                }
            }
        }
    }
}

private fun getAppVersion(context: android.content.Context): String {
    return try {
        val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)
        packageInfo.versionName ?: context.getString(R.string.unknown)
    } catch (e: PackageManager.NameNotFoundException) {
        context.getString(R.string.unknown)
    }
}
