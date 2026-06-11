package com.virtualap.app.ui.screen

import androidx.compose.animation.AnimatedContent
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
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.virtualap.app.ui.component.TerminalConsole
import com.virtualap.app.ui.viewmodel.APConfig
import com.virtualap.app.ui.viewmodel.APViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    vm: APViewModel = viewModel(),
    onNavigateToSettings: () -> Unit = {}
) {
    val status = vm.status
    var passwordVisible by remember { mutableStateOf(false) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text(
                            text = "VirtualAP",
                            style = MaterialTheme.typography.titleLarge,
                            fontWeight = FontWeight.Bold
                        )
                        Text(
                            text = "Your phone, an actual router",
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                },
                actions = {
                    IconButton(onClick = onNavigateToSettings) {
                        Icon(
                            imageVector = Icons.Default.Settings,
                            contentDescription = "Settings"
                        )
                    }
                    // Status pill chip
                    SuggestionChip(
                        onClick = { vm.refreshStatus() },
                        label = {
                            Text(
                                text = if (status.running) "Running" else "Stopped",
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
                            "Access Point",
                            style = MaterialTheme.typography.titleSmall,
                            fontWeight = FontWeight.SemiBold
                        )
                        Spacer(Modifier.height(12.dp))

                        // SSID
                        OutlinedTextField(
                            value = vm.config.ssid,
                            onValueChange = { vm.config = vm.config.copy(ssid = it) },
                            label = { Text("Network Name (SSID)") },
                            modifier = Modifier.fillMaxWidth(),
                            singleLine = true,
                            enabled = !status.running
                        )
                        Spacer(Modifier.height(8.dp))

                        // Password
                        OutlinedTextField(
                            value = vm.config.password,
                            onValueChange = { vm.config = vm.config.copy(password = it) },
                            label = { Text("Password (min 8 chars)") },
                            modifier = Modifier.fillMaxWidth(),
                            singleLine = true,
                            visualTransformation = if (passwordVisible) VisualTransformation.None else PasswordVisualTransformation(),
                            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                            trailingIcon = {
                                IconButton(onClick = { passwordVisible = !passwordVisible }) {
                                    Icon(
                                        if (passwordVisible) Icons.Default.VisibilityOff else Icons.Default.Visibility,
                                        contentDescription = if (passwordVisible) "Hide password" else "Show password"
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
                                label = { Text("Band") },
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
                        val channelOptions = if (vm.config.band == "5") {
                            listOf("Auto" to "", "36" to "36", "40" to "40", "44" to "44",
                                "48" to "48", "149" to "149", "153" to "153",
                                "157" to "157", "161" to "161", "165" to "165")
                        } else {
                            listOf("Auto" to "") + (1..11).map { "$it" to "$it" }
                        }
                        val selectedChannelLabel = channelOptions.find { it.second == vm.config.channel }?.first ?: "Auto"

                        ExposedDropdownMenuBox(
                            expanded = channelExpanded,
                            onExpandedChange = { if (!status.running) channelExpanded = it }
                        ) {
                            OutlinedTextField(
                                value = selectedChannelLabel,
                                onValueChange = {},
                                readOnly = true,
                                label = { Text("Channel") },
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
                        val upstreamOptions = listOf("Auto (recommended)" to "auto") +
                            vm.interfaces.filter { it.name != "ap0" }.map {
                                val label = if (it.ip != null) "${it.name} (${it.ip})" else it.name
                                label to it.name
                            }
                        val selectedUpstreamLabel = upstreamOptions.find { it.second == vm.config.upstream }?.first
                            ?: "Auto (recommended)"

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
                                    label = { Text("Upstream Interface") },
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
                                Icon(Icons.Default.Refresh, contentDescription = "Refresh interfaces")
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
                                            if (vm.isStarting) "Starting…" else "Stopping…",
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
                                            if (running) "Stop Access Point" else "Start Access Point",
                                            style = MaterialTheme.typography.labelLarge,
                                            fontWeight = FontWeight.SemiBold
                                        )
                                    }
                                }
                            }
                        }

                        // View Logs button — always visible once there are logs
                        if (vm.logText.isNotBlank() || vm.actionLogs.isNotEmpty()) {
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
                                    "View Logs",
                                    style = MaterialTheme.typography.labelLarge
                                )
                            }
                        }

                        if (vm.config.ssid.isBlank() && !status.running) {
                            Spacer(Modifier.height(4.dp))
                            Text(
                                "Enter a network name to get started",
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
                        text = "Active Network",
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
                            text = "Since ${status.started}",
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
                        label = "Gateway",
                        value = status.gateway
                    )
                    DashboardStatRow(
                        icon = Icons.Default.SignalCellularAlt,
                        label = "Band",
                        value = "$band$channel"
                    )
                }
                Column(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    DashboardStatRow(
                        icon = Icons.Default.SwapVert,
                        label = "Upstream",
                        value = status.upstream ?: "auto"
                    )
                    DashboardStatRow(
                        icon = Icons.Default.SettingsEthernet,
                        label = "Interface",
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
// Unified Log Bottom Sheet
// ---------------------------------------------------------------------------

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ActionLogsSheet(
    logs: List<Pair<Int, String>>,
    isProcessing: Boolean,
    onDismiss: () -> Unit,
    onClear: () -> Unit
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp)
                .padding(bottom = 24.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
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
                        if (isProcessing) "Running…" else "Logs",
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.SemiBold
                    )
                }
                Row {
                    IconButton(onClick = onClear) {
                        Icon(
                            Icons.Default.Delete,
                            contentDescription = "Clear logs",
                            modifier = Modifier.size(18.dp)
                        )
                    }
                    if (!isProcessing) {
                        TextButton(onClick = onDismiss) {
                            Text("Dismiss")
                        }
                    }
                }
            }
            Spacer(Modifier.height(8.dp))
            TerminalConsole(
                logs = if (logs.isEmpty()) listOf(android.util.Log.INFO to "No log output yet.")
                       else logs,
                isProcessing = isProcessing,
                modifier = Modifier.fillMaxWidth(),
                maxHeight = 480.dp
            )
        }
    }
}
