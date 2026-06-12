package com.virtualap.app.ui.screen

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.animation.AnimatedContent
import kotlinx.coroutines.launch
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.Spring
import androidx.compose.animation.core.spring
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.ui.res.stringResource
import com.virtualap.app.R
import com.virtualap.app.ui.component.TerminalConsole
import com.virtualap.app.ui.viewmodel.APConfig
import com.virtualap.app.ui.viewmodel.APViewModel
import com.virtualap.app.util.AnsiColorParser


@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    vm: APViewModel = viewModel(),
    onNavigateToSettings: () -> Unit = {}
) {
    val status = vm.status
    var passwordVisible by remember { mutableStateOf(false) }
    val focusManager = LocalFocusManager.current

    Scaffold(
        modifier = Modifier
            .fillMaxSize()
            .pointerInput(Unit) {
                detectTapGestures(onTap = {
                    focusManager.clearFocus()
                })
            },
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text(
                            text = stringResource(R.string.app_name),
                            style = MaterialTheme.typography.titleLarge,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = stringResource(R.string.app_subtitle),
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                },
                actions = {
                    IconButton(onClick = onNavigateToSettings) {
                        Icon(
                            imageVector = Icons.Default.Settings,
                            contentDescription = stringResource(R.string.settings_title)
                        )
                    }
                    // Status pill chip
                    SuggestionChip(
                        onClick = { vm.refreshStatus() },
                        label = {
                            Text(
                                text = if (status.running) stringResource(R.string.status_running) else stringResource(R.string.status_stopped),
                                style = MaterialTheme.typography.labelSmall
                            )
                        },
                        icon = {
                            Icon(
                                imageVector = if (status.running) Icons.Default.Wifi else Icons.Default.WifiOff,
                                contentDescription = null,
                                modifier = Modifier.size(14.dp)
                            )
                        },
                        colors = SuggestionChipDefaults.suggestionChipColors(
                            containerColor = if (status.running)
                                MaterialTheme.colorScheme.primaryContainer
                            else
                                MaterialTheme.colorScheme.surfaceVariant
                        ),
                        modifier = Modifier.padding(end = 12.dp)
                    )
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
            // --- 1. ACTIVE NETWORK DASHBOARD (only when running) ---
            item {
                AnimatedVisibility(
                    visible = status.running,
                    enter = expandVertically(
                        animationSpec = spring(
                            dampingRatio = Spring.DampingRatioMediumBouncy,
                            stiffness = Spring.StiffnessMedium
                        )
                    ) + fadeIn(),
                    exit = shrinkVertically() + fadeOut()
                ) {
                    ActiveNetworkCard(vm = vm)
                }
            }

            // --- 2. ACCESS POINT CONFIGURATION CARD ---
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text(
                            stringResource(R.string.access_point_title),
                            style = MaterialTheme.typography.titleSmall,
                            fontWeight = FontWeight.SemiBold
                        )
                        Spacer(Modifier.height(12.dp))

                        // SSID
                        OutlinedTextField(
                            value = vm.config.ssid,
                            onValueChange = { vm.config = vm.config.copy(ssid = it) },
                            label = { Text(stringResource(R.string.ssid_label)) },
                            modifier = Modifier.fillMaxWidth(),
                            singleLine = true,
                            enabled = !status.running
                        )
                        Spacer(Modifier.height(8.dp))

                        // Password
                        OutlinedTextField(
                            value = vm.config.password,
                            onValueChange = { vm.config = vm.config.copy(password = it) },
                            label = { Text(stringResource(R.string.password_label)) },
                            modifier = Modifier.fillMaxWidth(),
                            singleLine = true,
                            visualTransformation = if (passwordVisible) VisualTransformation.None else PasswordVisualTransformation(),
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                            trailingIcon = {
                                IconButton(onClick = { passwordVisible = !passwordVisible }) {
                                    Icon(
                                        if (passwordVisible) Icons.Default.VisibilityOff else Icons.Default.Visibility,
                                        contentDescription = if (passwordVisible) stringResource(R.string.hide_password_desc) else stringResource(R.string.show_password_desc)
                                    )
                                }
                            },
                            enabled = !status.running
                        )
                        Spacer(Modifier.height(8.dp))

                        // Band dropdown
                        var bandExpanded by remember { mutableStateOf(false) }
                        val bandOptions = listOf("2.4 GHz" to "2", "5 GHz" to "5")
                        val selectedBandLabel = bandOptions.find { it.second == vm.config.band }?.first ?: "2.4 GHz"

                        ExposedDropdownMenuBox(
                            expanded = bandExpanded,
                            onExpandedChange = { if (!status.running) bandExpanded = it }
                        ) {
                            OutlinedTextField(
                                value = selectedBandLabel,
                                onValueChange = {},
                                readOnly = true,
                                label = { Text(stringResource(R.string.band_label)) },
                                trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = bandExpanded) },
                                modifier = Modifier.fillMaxWidth().menuAnchor(),
                                enabled = !status.running
                            )
                            ExposedDropdownMenu(
                                expanded = bandExpanded,
                                onDismissRequest = { bandExpanded = false }
                            ) {
                                bandOptions.forEach { (label, value) ->
                                    DropdownMenuItem(
                                        text = { Text(label) },
                                        onClick = {
                                            vm.config = vm.config.copy(band = value, channel = "")
                                            bandExpanded = false
                                        }
                                    )
                                }
                            }
                        }
                        Spacer(Modifier.height(8.dp))

                        // Channel dropdown
                        var channelExpanded by remember { mutableStateOf(false) }
                        val autoLabel = stringResource(R.string.auto_label)
                        val channelOptions = if (vm.config.band == "5") {
                            listOf(autoLabel to "", "36" to "36", "40" to "40", "44" to "44",
                                "48" to "48", "149" to "149", "153" to "153",
                                "157" to "157", "161" to "161", "165" to "165")
                        } else {
                            listOf(autoLabel to "") + (1..11).map { "$it" to "$it" }
                        }
                        val selectedChannelLabel = channelOptions.find { it.second == vm.config.channel }?.first ?: autoLabel

                        ExposedDropdownMenuBox(
                            expanded = channelExpanded,
                            onExpandedChange = { if (!status.running) channelExpanded = it }
                        ) {
                            OutlinedTextField(
                                value = selectedChannelLabel,
                                onValueChange = {},
                                readOnly = true,
                                label = { Text(stringResource(R.string.channel_label)) },
                                trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = channelExpanded) },
                                modifier = Modifier.fillMaxWidth().menuAnchor(),
                                enabled = !status.running
                            )
                            ExposedDropdownMenu(
                                expanded = channelExpanded,
                                onDismissRequest = { channelExpanded = false }
                            ) {
                                channelOptions.forEach { (label, value) ->
                                    DropdownMenuItem(
                                        text = { Text(label) },
                                        onClick = {
                                            vm.config = vm.config.copy(channel = value)
                                            channelExpanded = false
                                        }
                                    )
                                }
                            }
                        }
                        Spacer(Modifier.height(8.dp))

                        // Upstream dropdown with refresh button
                        var upstreamExpanded by remember { mutableStateOf(false) }
                        val upstreamAutoLabel = stringResource(R.string.upstream_auto)
                        val upstreamOptions = listOf(upstreamAutoLabel to "auto") +
                            vm.interfaces.filter { it.name != "ap0" }.map {
                                val label = if (it.ip != null) "${it.name} (${it.ip})" else it.name
                                label to it.name
                            }
                        val selectedUpstreamLabel = upstreamOptions.find { it.second == vm.config.upstream }?.first
                            ?: upstreamAutoLabel

                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            ExposedDropdownMenuBox(
                                expanded = upstreamExpanded,
                                onExpandedChange = { if (!status.running) upstreamExpanded = it },
                                modifier = Modifier.weight(1f)
                            ) {
                                OutlinedTextField(
                                    value = selectedUpstreamLabel,
                                    onValueChange = {},
                                    readOnly = true,
                                    label = { Text(stringResource(R.string.upstream_interface_label)) },
                                    trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = upstreamExpanded) },
                                    modifier = Modifier.fillMaxWidth().menuAnchor(),
                                    enabled = !status.running
                                )
                                ExposedDropdownMenu(
                                    expanded = upstreamExpanded,
                                    onDismissRequest = { upstreamExpanded = false }
                                ) {
                                    upstreamOptions.forEach { (label, value) ->
                                        DropdownMenuItem(
                                            text = { Text(label) },
                                            onClick = {
                                                vm.config = vm.config.copy(upstream = value)
                                                upstreamExpanded = false
                                            }
                                        )
                                    }
                                }
                            }
                            IconButton(
                                onClick = { vm.loadInterfaces() },
                                enabled = !status.running
                            ) {
                                Icon(Icons.Default.Refresh, contentDescription = stringResource(R.string.refresh_interfaces_desc))
                            }
                        }

                        Spacer(Modifier.height(16.dp))

                        // Start / Stop button
                        val isLoading = vm.isStarting || vm.isStopping
                        Button(
                            onClick = { if (status.running) vm.stop() else vm.start() },
                            modifier = Modifier.fillMaxWidth().height(52.dp),
                            enabled = !isLoading && (status.running || (vm.config.ssid.isNotBlank() && vm.config.password.length >= 8)),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = if (status.running)
                                    MaterialTheme.colorScheme.error
                                else
                                    MaterialTheme.colorScheme.primary
                            )
                        ) {
                            AnimatedContent(
                                targetState = isLoading to status.running,
                                transitionSpec = { fadeIn() togetherWith fadeOut() },
                                label = "startStopContent"
                            ) { (loading, running) ->
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.Center,
                                    modifier = Modifier.fillMaxWidth()
                                ) {
                                    if (loading) {
                                        CircularProgressIndicator(
                                            modifier = Modifier.size(20.dp),
                                            strokeWidth = 2.dp,
                                            color = MaterialTheme.colorScheme.onPrimary
                                        )
                                        Spacer(Modifier.width(8.dp))
                                        Text(
                                            if (vm.isStarting) stringResource(R.string.starting) else stringResource(R.string.stopping),
                                            style = MaterialTheme.typography.labelLarge
                                        )
                                    } else {
                                        Icon(
                                            if (running) Icons.Default.WifiOff else Icons.Default.Wifi,
                                            contentDescription = null,
                                            modifier = Modifier.size(18.dp)
                                        )
                                        Spacer(Modifier.width(8.dp))
                                        Text(
                                            if (running) stringResource(R.string.stop_ap) else stringResource(R.string.start_ap),
                                            style = MaterialTheme.typography.labelLarge,
                                            fontWeight = FontWeight.SemiBold
                                        )
                                    }
                                }
                            }
                        }

                        // View Logs button — always visible
                        Spacer(Modifier.height(8.dp))
                        OutlinedButton(
                            onClick = { vm.openLogSheet() },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Icon(
                                Icons.Default.Terminal,
                                contentDescription = null,
                                modifier = Modifier.size(16.dp)
                            )
                            Spacer(Modifier.width(8.dp))
                            Text(
                                stringResource(R.string.view_logs),
                                style = MaterialTheme.typography.labelLarge
                            )
                        }

                        if (vm.config.ssid.isBlank() && !status.running) {
                            Spacer(Modifier.height(4.dp))
                            Text(
                                stringResource(R.string.enter_ssid_prompt),
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                modifier = Modifier.fillMaxWidth()
                            )
                        }
                    }
                }
            }

            item { Spacer(Modifier.height(8.dp)) }
        }
    }

    // Unified log bottom sheet — auto-opens on start/stop, re-openable via "View Logs"
    if (vm.showActionLogs) {
        ActionLogsSheet(
            logs = if (vm.actionLogs.isNotEmpty()) vm.actionLogs
                   else vm.logText.split("\n").map { android.util.Log.INFO to it },
            isProcessing = vm.isStarting || vm.isStopping,
            onDismiss = { if (!vm.isStarting && !vm.isStopping) vm.dismissActionLogs() },
            onClear = { vm.clearLog() }
        )
    }
}

// ---------------------------------------------------------------------------
// Active Network Dashboard Card
// ---------------------------------------------------------------------------

@Composable
private fun ActiveNetworkCard(vm: APViewModel) {
    val status = vm.status

    val band = when (status.band) {
        "2", "2.4" -> "2.4 GHz"
        "5" -> "5 GHz"
        else -> status.band ?: "—"
    }
    val channel = status.channel?.let { " · ch$it" } ?: ""

    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.primaryContainer
        )
    ) {
        Column(modifier = Modifier.padding(20.dp)) {
            // Header row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        Icons.Default.Wifi,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.size(20.dp)
                    )
                    Spacer(Modifier.width(8.dp))
                    Text(
                        text = stringResource(R.string.active_network_title),
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onPrimaryContainer
                    )
                }
                if (status.started != null) {
                    Surface(
                        shape = RoundedCornerShape(8.dp),
                        color = MaterialTheme.colorScheme.primary.copy(alpha = 0.15f)
                    ) {
                        Text(
                            text = stringResource(R.string.since_time, status.started),
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp)
                        )
                    }
                }
            }

            Spacer(Modifier.height(16.dp))

            // SSID prominently
            status.ssid?.let { ssid ->
                Text(
                    text = ssid,
                    style = MaterialTheme.typography.headlineSmall,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }

            Spacer(Modifier.height(12.dp))
            HorizontalDivider(color = MaterialTheme.colorScheme.primary.copy(alpha = 0.15f))
            Spacer(Modifier.height(12.dp))

            // Stat grid: 2 columns
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Column(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    DashboardStatRow(
                        icon = Icons.Default.Router,
                        label = stringResource(R.string.gateway_label),
                        value = status.gateway
                    )
                    DashboardStatRow(
                        icon = Icons.Default.SignalCellularAlt,
                        label = stringResource(R.string.band_label),
                        value = "$band$channel"
                    )
                }
                Column(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    DashboardStatRow(
                        icon = Icons.Default.SwapVert,
                        label = stringResource(R.string.upstream_label),
                        value = status.upstream ?: stringResource(R.string.auto_label)
                    )
                    DashboardStatRow(
                        icon = Icons.Default.SettingsEthernet,
                        label = stringResource(R.string.interface_label),
                        value = status.upstreamIface ?: "—"
                    )
                }
            }
        }
    }
}

@Composable
private fun DashboardStatRow(icon: ImageVector, label: String, value: String) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(
            icon,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.primary.copy(alpha = 0.7f),
            modifier = Modifier.size(14.dp)
        )
        Spacer(Modifier.width(6.dp))
        Column {
            Text(
                label,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.6f)
            )
            Text(
                value,
                style = MaterialTheme.typography.labelMedium,
                fontWeight = FontWeight.Medium,
                color = MaterialTheme.colorScheme.onPrimaryContainer
            )
        }
    }
}

// ---------------------------------------------------------------------------
// Unified Log Bottom Sheet — Droidspaces-style action bar
// ---------------------------------------------------------------------------

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ActionLogsSheet(
    logs: List<Pair<Int, String>>,
    isProcessing: Boolean,
    onDismiss: () -> Unit,
    onClear: () -> Unit
) {
    // The sheet must be truly undismissable while a command runs: rejecting
    // Hidden here blocks swipe-down, scrim taps and back presses at the
    // state-machine level. Guarding only onDismissRequest is not enough -
    // gesture dismissal animates the sheet away BEFORE that callback fires,
    // so the sheet ended up hidden while showActionLogs stayed true, leaving
    // an invisible scrim that ate every touch once the command finished.
    val processing by rememberUpdatedState(isProcessing)
    val sheetState = rememberModalBottomSheetState(
        skipPartiallyExpanded = true,
        confirmValueChange = { it != SheetValue.Hidden || !processing }
    )
    val scope = rememberCoroutineScope()
    val context = androidx.compose.ui.platform.LocalContext.current
    val buttonShape = RoundedCornerShape(14.dp)

    // Animated dismiss: slide the sheet out first, then notify the caller
    val animatedDismiss: () -> Unit = animatedDismiss@{
        if (isProcessing) return@animatedDismiss
        scope.launch {
            sheetState.hide()
            onDismiss()
        }
    }

    ModalBottomSheet(
        // Only reachable when confirmValueChange allowed Hidden (not processing)
        onDismissRequest = onDismiss,
        sheetState = sheetState
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 20.dp)
                .padding(bottom = 28.dp)
        ) {
            // Title row
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 14.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    if (isProcessing) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(14.dp),
                            strokeWidth = 2.dp
                        )
                        Spacer(Modifier.width(8.dp))
                    }
                    Text(
                        text = if (isProcessing) stringResource(R.string.running_log_title) else stringResource(R.string.logs_title),
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.Bold
                    )
                }
                // Close button — Droidspaces style, slides sheet out on press
                val canClose = !isProcessing
                Surface(
                    modifier = Modifier
                        .size(36.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .clickable(enabled = canClose, onClick = animatedDismiss),
                    shape = RoundedCornerShape(10.dp),
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = if (canClose) 0.08f else 0.04f),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = if (canClose) 0.3f else 0.15f)),
                    tonalElevation = 0.dp
                ) {
                    Box(contentAlignment = Alignment.Center, modifier = Modifier.fillMaxSize()) {
                        Icon(
                            imageVector = Icons.Default.Close,
                            contentDescription = stringResource(R.string.close),
                            modifier = Modifier.size(18.dp),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = if (canClose) 1f else 0.38f)
                        )
                    }
                }
            }

            // Action button row — Clear Logs + Copy Logs
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 12.dp),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // Clear Logs button
                val canClear = logs.isNotEmpty() && !isProcessing
                Surface(
                    modifier = Modifier
                        .height(38.dp)
                        .weight(1f)
                        .clip(buttonShape)
                        .clickable(enabled = canClear, onClick = onClear),
                    shape = buttonShape,
                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = if (canClear) 0.06f else 0.03f),
                    border = BorderStroke(
                        1.dp,
                        if (canClear) MaterialTheme.colorScheme.error.copy(alpha = 0.4f)
                        else MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.2f)
                    ),
                    tonalElevation = 0.dp
                ) {
                    Row(
                        modifier = Modifier.fillMaxSize().padding(horizontal = 14.dp),
                        horizontalArrangement = Arrangement.Center,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            imageVector = Icons.Default.Delete,
                            contentDescription = stringResource(R.string.clear_logs),
                            modifier = Modifier.size(16.dp),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = if (canClear) 0.8f else 0.38f)
                        )
                        Spacer(Modifier.width(8.dp))
                        Text(
                            text = stringResource(R.string.clear_logs),
                            style = MaterialTheme.typography.labelMedium,
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = if (canClear) 0.8f else 0.38f)
                        )
                    }
                }

                // Copy Logs button
                val canCopy = logs.isNotEmpty() && !isProcessing
                val terminalLogsLabel = stringResource(R.string.terminal_logs)
                Surface(
                    modifier = Modifier
                        .height(38.dp)
                        .weight(1f)
                        .clip(buttonShape)
                        .clickable(
                            enabled = canCopy,
                            onClick = {
                                val text = logs.joinToString("\n") { AnsiColorParser.stripAnsi(it.second) }
                                val clipboard = context.getSystemService(android.content.ClipboardManager::class.java)
                                val clip = android.content.ClipData.newPlainText(terminalLogsLabel, text)
                                clipboard.setPrimaryClip(clip)
                                android.widget.Toast.makeText(context, R.string.logs_copied, android.widget.Toast.LENGTH_SHORT).show()
                            }
                        ),
                    shape = buttonShape,
                    color = if (canCopy) MaterialTheme.colorScheme.primary.copy(alpha = 0.08f)
                            else MaterialTheme.colorScheme.onSurface.copy(alpha = 0.03f),
                    border = BorderStroke(
                        1.dp,
                        if (canCopy) MaterialTheme.colorScheme.primary.copy(alpha = 0.3f)
                        else MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.2f)
                    ),
                    tonalElevation = 0.dp
                ) {
                    Row(
                        modifier = Modifier.fillMaxSize().padding(horizontal = 14.dp),
                        horizontalArrangement = Arrangement.Center,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(
                            imageVector = Icons.Default.ContentCopy,
                            contentDescription = stringResource(R.string.copy_logs),
                            modifier = Modifier.size(16.dp),
                            tint = if (canCopy) MaterialTheme.colorScheme.primary
                                   else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.38f)
                        )
                        Spacer(Modifier.width(8.dp))
                        Text(
                            text = stringResource(R.string.copy_logs),
                            style = MaterialTheme.typography.labelMedium,
                            fontWeight = FontWeight.SemiBold,
                            color = if (canCopy) MaterialTheme.colorScheme.primary
                                    else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.38f)
                        )
                    }
                }
            }

            // Terminal console — fills remaining space
            TerminalConsole(
                logs = if (logs.isEmpty()) listOf(android.util.Log.INFO to stringResource(R.string.no_logs_msg))
                       else logs,
                isProcessing = isProcessing,
                modifier = Modifier.fillMaxWidth(),
                maxHeight = 460.dp
            )
        }
    }
}
