package com.virtualap.app.ui.screen

import android.util.Log
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.virtualap.app.ui.component.SwitchItem
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
    var leasesExpanded by remember { mutableStateOf(false) }

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
            // --- 1. STATUS CARD (only when running) ---
            if (status.running) {
                item {
                    Card(
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(16.dp),
                        colors = CardDefaults.cardColors(
                            containerColor = MaterialTheme.colorScheme.primaryContainer
                        )
                    ) {
                        Column(modifier = Modifier.padding(16.dp)) {
                            Text(
                                text = "Active Network",
                                style = MaterialTheme.typography.titleSmall,
                                fontWeight = FontWeight.SemiBold,
                                color = MaterialTheme.colorScheme.onPrimaryContainer
                            )
                            Spacer(Modifier.height(12.dp))

                            // Stat chips grid
                            @Composable
                            fun StatChip(label: String, value: String) {
                                Surface(
                                    shape = RoundedCornerShape(8.dp),
                                    color = MaterialTheme.colorScheme.surface.copy(alpha = 0.6f),
                                    modifier = Modifier.padding(2.dp)
                                ) {
                                    Column(modifier = Modifier.padding(horizontal = 10.dp, vertical = 6.dp)) {
                                        Text(
                                            label,
                                            style = MaterialTheme.typography.labelSmall,
                                            color = MaterialTheme.colorScheme.onSurfaceVariant
                                        )
                                        Text(
                                            value,
                                            style = MaterialTheme.typography.labelMedium,
                                            fontWeight = FontWeight.Medium
                                        )
                                    }
                                }
                            }

                            val band = when (status.band) {
                                "2", "2.4" -> "2.4 GHz"
                                "5" -> "5 GHz"
                                else -> status.band ?: "—"
                            }
                            val channel = status.channel?.let { " ch$it" } ?: ""

                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                Column(
                                    modifier = Modifier.weight(1f),
                                    verticalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    StatChip("Gateway", status.gateway)
                                    StatChip("Band", "$band$channel")
                                    StatChip("Clients", "${status.clients}")
                                }
                                Column(
                                    modifier = Modifier.weight(1f),
                                    verticalArrangement = Arrangement.spacedBy(4.dp)
                                ) {
                                    StatChip("SSID", status.ssid ?: "—")
                                    StatChip("Upstream", status.upstream ?: "auto")
                                    if (status.started != null) {
                                        StatChip("Since", status.started)
                                    }
                                }
                            }

                            // DHCP leases
                            if (status.clients > 0) {
                                Spacer(Modifier.height(8.dp))
                                TextButton(
                                    onClick = { leasesExpanded = !leasesExpanded },
                                    modifier = Modifier.fillMaxWidth()
                                ) {
                                    Icon(
                                        if (leasesExpanded) Icons.Default.ExpandLess else Icons.Default.ExpandMore,
                                        contentDescription = null,
                                        modifier = Modifier.size(16.dp)
                                    )
                                    Spacer(Modifier.width(4.dp))
                                    Text(
                                        if (leasesExpanded) "Hide devices" else "Show ${status.clients} device(s)",
                                        style = MaterialTheme.typography.labelMedium
                                    )
                                }
                                AnimatedVisibility(
                                    visible = leasesExpanded,
                                    enter = expandVertically() + fadeIn(),
                                    exit = shrinkVertically() + fadeOut()
                                ) {
                                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                        vm.leases.forEach { lease ->
                                            Surface(
                                                shape = RoundedCornerShape(8.dp),
                                                color = MaterialTheme.colorScheme.surface.copy(alpha = 0.5f)
                                            ) {
                                                Row(
                                                    modifier = Modifier
                                                        .fillMaxWidth()
                                                        .padding(horizontal = 12.dp, vertical = 8.dp),
                                                    verticalAlignment = Alignment.CenterVertically,
                                                    horizontalArrangement = Arrangement.SpaceBetween
                                                ) {
                                                    Column {
                                                        Text(
                                                            lease.hostname,
                                                            style = MaterialTheme.typography.bodySmall,
                                                            fontWeight = FontWeight.Medium
                                                        )
                                                        Text(
                                                            lease.mac,
                                                            style = MaterialTheme.typography.labelSmall,
                                                            color = MaterialTheme.colorScheme.onSurfaceVariant
                                                        )
                                                    }
                                                    Text(
                                                        lease.ip,
                                                        style = MaterialTheme.typography.bodySmall,
                                                        color = MaterialTheme.colorScheme.primary
                                                    )
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // --- 2. ACCESS POINT CARD ---
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
                            if (isLoading) {
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
                                    if (status.running) Icons.Default.WifiOff else Icons.Default.Wifi,
                                    contentDescription = null,
                                    modifier = Modifier.size(18.dp)
                                )
                                Spacer(Modifier.width(8.dp))
                                Text(
                                    if (status.running) "Stop Access Point" else "Start Access Point",
                                    style = MaterialTheme.typography.labelLarge,
                                    fontWeight = FontWeight.SemiBold
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

            // --- 3. SETTINGS CARD ---
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text(
                            "Settings",
                            style = MaterialTheme.typography.titleSmall,
                            fontWeight = FontWeight.SemiBold
                        )
                        Spacer(Modifier.height(8.dp))
                        SwitchItem(
                            label = "Run at boot",
                            subtitle = "Automatically start the AP when the device boots",
                            checked = vm.bootEnabled,
                            onCheckedChange = { vm.setBootFlag(it) }
                        )
                    }
                }
            }

            // --- 4. LOGS CARD ---
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                "Logs",
                                style = MaterialTheme.typography.titleSmall,
                                fontWeight = FontWeight.SemiBold
                            )
                            Row {
                                IconButton(onClick = { vm.clearLog() }) {
                                    Icon(
                                        Icons.Default.Delete,
                                        contentDescription = "Clear logs",
                                        modifier = Modifier.size(20.dp)
                                    )
                                }
                                IconButton(onClick = { vm.refreshStatus() }) {
                                    Icon(
                                        Icons.Default.Refresh,
                                        contentDescription = "Refresh",
                                        modifier = Modifier.size(20.dp)
                                    )
                                }
                            }
                        }
                        Spacer(Modifier.height(8.dp))
                        TerminalConsole(
                            logs = if (vm.logText.isBlank()) listOf(Log.INFO to "No log output yet.")
                                   else vm.logText.split("\n").map { Log.INFO to it },
                            isProcessing = false,
                            modifier = Modifier.fillMaxWidth(),
                            maxHeight = 300.dp
                        )
                    }
                }
            }

            item { Spacer(Modifier.height(8.dp)) }
        }
    }

    // Action progress bottom sheet
    if (vm.showActionLogs) {
        ActionLogsSheet(
            logs = vm.actionLogs,
            isProcessing = vm.isStarting || vm.isStopping,
            onDismiss = { if (!vm.isStarting && !vm.isStopping) vm.dismissActionLogs() }
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ActionLogsSheet(
    logs: List<Pair<Int, String>>,
    isProcessing: Boolean,
    onDismiss: () -> Unit
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
                Text(
                    if (isProcessing) "Running…" else "Done",
                    style = MaterialTheme.typography.titleSmall,
                    fontWeight = FontWeight.SemiBold
                )
                if (!isProcessing) {
                    TextButton(onClick = onDismiss) {
                        Text("Dismiss")
                    }
                }
            }
            Spacer(Modifier.height(8.dp))
            TerminalConsole(
                logs = logs,
                isProcessing = isProcessing,
                modifier = Modifier.fillMaxWidth(),
                maxHeight = 400.dp
            )
        }
    }
}
