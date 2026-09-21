package com.virtualap.app.ui.screen

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
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
import androidx.compose.material3.pulltorefresh.PullToRefreshContainer
import androidx.compose.material3.pulltorefresh.rememberPullToRefreshState
import androidx.compose.runtime.*
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.imePadding
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.res.vectorResource
import com.virtualap.app.R
import com.virtualap.app.ui.component.SwitchItem
import com.virtualap.app.ui.component.TerminalConsole
import com.virtualap.app.ui.viewmodel.APConfig
import com.virtualap.app.ui.viewmodel.APViewModel
import com.virtualap.app.util.AnsiColorParser
import com.virtualap.app.util.QrCodeGenerator

private val ipv4Regex = Regex(
    "^(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)" +
    "\\.(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)" +
    "\\.(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)" +
    "\\.(25[0-4]|2[0-4]\\d|[01]?\\d[1-9]|[01]?[1-9]\\d?|[1-9])$"
)
private fun isValidIpv4(ip: String): Boolean = ipv4Regex.matches(ip.trim())
private fun isValidDnsServers(dns: String): Boolean {
    if (dns.isBlank()) return true
    return dns.split(",").all { isValidIpv4(it.trim()) }
}


@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    vm: APViewModel = viewModel(),
    onNavigateToSettings: () -> Unit = {},
    onRefresh: () -> Unit = {}
) {
    val status = vm.status
    var passwordVisible by remember { mutableStateOf(false) }
    val focusManager = LocalFocusManager.current

    Scaffold(
        modifier = Modifier
            .fillMaxSize()
            .imePadding()
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
                    // Status pill chip — a small spinner stands in until the first
                    // fetch lands, so we never flash a wrong "Stopped" state.
                    if (!vm.isReady) {
                        CircularProgressIndicator(
                            modifier = Modifier.padding(end = 16.dp).size(18.dp),
                            strokeWidth = 2.dp
                        )
                    } else {
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
                }
            )
        }
    ) { innerPadding ->
        // Gate the whole screen on the first load so nothing flashes stale state
        // or pops in late (status, interfaces and containers all arrive together).
        if (!vm.isReady) {
            Box(
                modifier = Modifier.fillMaxSize().padding(innerPadding),
                contentAlignment = Alignment.Center
            ) {
                CircularProgressIndicator()
            }
            return@Scaffold
        }

        // Global pull-to-refresh: re-fetch status, interfaces, containers and
        // root in one gesture (replaces the old per-control refresh button).
        val pullState = rememberPullToRefreshState()
        if (pullState.isRefreshing) {
            LaunchedEffect(true) {
                try {
                    vm.refreshAllNow()
                    onRefresh()
                } finally {
                    pullState.endRefresh()
                }
            }
        }

        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .nestedScroll(pullState.nestedScrollConnection)
        ) {
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
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

                        // Security dropdown (Open / WPA2 / WPA2-WPA3 / WPA3)
                        var securityExpanded by remember { mutableStateOf(false) }
                        val securityOptions = listOf(
                            "Open" to "open",
                            "WPA2-Personal" to "wpa2",
                            "WPA2/WPA3-Personal" to "wpa2wpa3",
                            "WPA3-Personal" to "wpa3"
                        )
                        val selectedSecurityLabel = securityOptions.find { it.second == vm.config.security }?.first ?: "WPA2-Personal"

                        ExposedDropdownMenuBox(
                            expanded = securityExpanded,
                            onExpandedChange = { if (!status.running) securityExpanded = it }
                        ) {
                            OutlinedTextField(
                                value = selectedSecurityLabel,
                                onValueChange = {},
                                readOnly = true,
                                label = { Text(stringResource(R.string.security_label)) },
                                trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = securityExpanded) },
                                modifier = Modifier.fillMaxWidth().menuAnchor(),
                                enabled = !status.running
                            )
                            ExposedDropdownMenu(
                                expanded = securityExpanded,
                                onDismissRequest = { securityExpanded = false }
                            ) {
                                securityOptions.forEach { (label, value) ->
                                    DropdownMenuItem(
                                        text = { Text(label) },
                                        onClick = {
                                            vm.selectSecurity(value)
                                            securityExpanded = false
                                        }
                                    )
                                }
                            }
                        }
                        // WPA3 (incl. the transition mode) can be rejected by older
                        // client devices; warn so users know to fall back to WPA2.
                        if (vm.config.security == "wpa2wpa3" || vm.config.security == "wpa3") {
                            Spacer(Modifier.height(4.dp))
                            Text(
                                stringResource(R.string.security_wpa3_hint),
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                        Spacer(Modifier.height(8.dp))

                        // Password — open networks have none, so hide the field there.
                        if (vm.passwordRequired()) {
                            // WPA-PSK/SAE passphrase is 8-63 chars; flag an out-of-range
                            // value once the user has started typing (blank stays neutral).
                            val pwLen = vm.config.password.length
                            val pwError = pwLen in 1..7 || pwLen > 63
                            OutlinedTextField(
                                value = vm.config.password,
                                onValueChange = { vm.config = vm.config.copy(password = it) },
                                label = { Text(stringResource(R.string.password_label)) },
                                modifier = Modifier.fillMaxWidth(),
                                singleLine = true,
                                isError = pwError,
                                supportingText = if (pwError) {
                                    { Text(stringResource(R.string.password_length_error), color = MaterialTheme.colorScheme.error) }
                                } else null,
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
                        }

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
                                            // Reset channel: valid channels differ per band.
                                            vm.selectBand(value)
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
                                            vm.selectChannel(value)
                                            channelExpanded = false
                                        }
                                    )
                                }
                            }
                        }
                        Spacer(Modifier.height(8.dp))

                        // Width dropdown. All options are always selectable; the
                        // backend downgrades an unsupported width (wrong band/chip/
                        // channel) to the widest it can actually bring up.
                        var widthExpanded by remember { mutableStateOf(false) }
                        val widthOptions = listOf(
                            autoLabel to "auto", "20 MHz" to "20",
                            "40 MHz" to "40", "80 MHz" to "80"
                        )
                        val selectedWidthLabel = widthOptions.find { it.second == vm.config.width }?.first ?: autoLabel

                        ExposedDropdownMenuBox(
                            expanded = widthExpanded,
                            onExpandedChange = { if (!status.running) widthExpanded = it }
                        ) {
                            OutlinedTextField(
                                value = selectedWidthLabel,
                                onValueChange = {},
                                readOnly = true,
                                label = { Text(stringResource(R.string.width_label)) },
                                trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = widthExpanded) },
                                modifier = Modifier.fillMaxWidth().menuAnchor(),
                                enabled = !status.running
                            )
                            ExposedDropdownMenu(
                                expanded = widthExpanded,
                                onDismissRequest = { widthExpanded = false }
                            ) {
                                widthOptions.forEach { (label, value) ->
                                    DropdownMenuItem(
                                        text = { Text(label) },
                                        onClick = {
                                            vm.selectWidth(value)
                                            widthExpanded = false
                                        }
                                    )
                                }
                            }
                        }
                    }
                }
            }

            // --- 3. UPSTREAM CARD (where the internet comes from) ---
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text(
                            stringResource(R.string.upstream_card_title),
                            style = MaterialTheme.typography.titleSmall,
                            fontWeight = FontWeight.SemiBold
                        )
                        Spacer(Modifier.height(12.dp))

                        val hasContainers = vm.containers.isNotEmpty()

                        // Interface | Container selector — shown only when Droidspaces
                        // is present with at least one running container.
                        if (hasContainers) {
                            SingleChoiceSegmentedButtonRow(modifier = Modifier.fillMaxWidth()) {
                                SegmentedButton(
                                    selected = !vm.config.containerMode,
                                    onClick = { if (!status.running) vm.config = vm.config.copy(containerMode = false) },
                                    shape = SegmentedButtonDefaults.itemShape(index = 0, count = 2),
                                    enabled = !status.running,
                                    icon = {}
                                ) { Text(stringResource(R.string.interface_label)) }
                                SegmentedButton(
                                    selected = vm.config.containerMode,
                                    onClick = {
                                        if (!status.running) vm.config = vm.config.copy(
                                            containerMode = true,
                                            containerName = vm.config.containerName.ifBlank { vm.containers.first() }
                                        )
                                    },
                                    shape = SegmentedButtonDefaults.itemShape(index = 1, count = 2),
                                    enabled = !status.running,
                                    icon = {}
                                ) { Text(stringResource(R.string.container_label)) }
                            }
                            Spacer(Modifier.height(12.dp))
                        }

                        if (vm.config.containerMode && hasContainers) {
                            // Container picker (the container owns DHCP/NAT)
                            var containerExpanded by remember { mutableStateOf(false) }
                            val selectedContainer = vm.config.containerName.ifBlank { vm.containers.first() }
                            ExposedDropdownMenuBox(
                                expanded = containerExpanded,
                                onExpandedChange = { if (!status.running) containerExpanded = it }
                            ) {
                                OutlinedTextField(
                                    value = selectedContainer,
                                    onValueChange = {},
                                    readOnly = true,
                                    label = { Text(stringResource(R.string.container_label)) },
                                    leadingIcon = { Icon(ImageVector.vectorResource(R.drawable.ic_droidspaces), contentDescription = null) },
                                    trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = containerExpanded) },
                                    modifier = Modifier.fillMaxWidth().menuAnchor(),
                                    enabled = !status.running
                                )
                                ExposedDropdownMenu(
                                    expanded = containerExpanded,
                                    onDismissRequest = { containerExpanded = false }
                                ) {
                                    vm.containers.forEach { name ->
                                        DropdownMenuItem(
                                            text = { Text(name) },
                                            onClick = {
                                                vm.config = vm.config.copy(containerName = name)
                                                containerExpanded = false
                                            }
                                        )
                                    }
                                }
                            }
                            Spacer(Modifier.height(4.dp))
                            // Fixed 2-line caption so the card height stays identical
                            // across the Interface/Container tabs.
                            Text(
                                stringResource(R.string.container_upstream_desc),
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                minLines = 2,
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis
                            )
                        } else {
                            // Host interface dropdown (refresh via pull-to-refresh)
                            var upstreamExpanded by remember { mutableStateOf(false) }
                            val upstreamAutoLabel = stringResource(R.string.upstream_auto)
                            val upstreamOptions = listOf(upstreamAutoLabel to "auto") +
                                vm.interfaces.filter { it.name != "ap0" }.map {
                                    val label = if (it.ip != null) "${it.name} (${it.ip})" else it.name
                                    label to it.name
                                }
                            val selectedUpstreamLabel = upstreamOptions.find { it.second == vm.config.upstream }?.first
                                ?: upstreamAutoLabel

                            ExposedDropdownMenuBox(
                                expanded = upstreamExpanded,
                                onExpandedChange = { if (!status.running) upstreamExpanded = it }
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
                            Spacer(Modifier.height(4.dp))
                            // Fixed 2-line caption so the card height stays identical
                            // across the Interface/Container tabs.
                            Text(
                                stringResource(R.string.interface_upstream_desc),
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                minLines = 2,
                                maxLines = 2,
                                overflow = TextOverflow.Ellipsis
                            )
                        }
                    }
                }
            }

            // --- 4. ADVANCED CARD ---
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(16.dp)
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text(
                            stringResource(R.string.advanced_settings_title),
                            style = MaterialTheme.typography.titleSmall,
                            fontWeight = FontWeight.SemiBold
                        )
                        Spacer(Modifier.height(12.dp))

                        // Gateway IP - the AP subnet gateway in routed mode and the
                        // LAN gateway provisioned inside the container in managed
                        // mode, so it's shown in both. Blank uses the default.
                        var gatewayText by remember(vm.config.gateway) { mutableStateOf(vm.config.gateway) }
                        val gatewayError = gatewayText.isNotBlank() && !isValidIpv4(gatewayText)
                        OutlinedTextField(
                            value = gatewayText,
                            onValueChange = { v ->
                                gatewayText = v
                                when {
                                    v.isBlank() -> vm.config = vm.config.copy(gateway = "")
                                    isValidIpv4(v) -> vm.config = vm.config.copy(gateway = v.trim())
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
                            leadingIcon = { Icon(Icons.Default.Router, contentDescription = null) },
                            trailingIcon = {
                                if (gatewayError)
                                    Icon(Icons.Default.Error, contentDescription = null, tint = MaterialTheme.colorScheme.error)
                            },
                            singleLine = true,
                            enabled = !status.running,
                            keyboardOptions = KeyboardOptions(
                                keyboardType = KeyboardType.Number,
                                imeAction = ImeAction.Next
                            ),
                            modifier = Modifier.fillMaxWidth()
                        )
                        Spacer(Modifier.height(8.dp))

                      // DNS is VirtualAP's own L3 — irrelevant when a container owns
                      // the LAN (it runs its own resolver), so hide it in managed mode.
                      if (!vm.config.containerMode) {
                        // DNS Servers
                        var dnsText by remember(vm.config.dnsServers) { mutableStateOf(vm.config.dnsServers) }
                        val dnsError = !isValidDnsServers(dnsText)
                        OutlinedTextField(
                            value = dnsText,
                            onValueChange = { v ->
                                dnsText = v
                                if (isValidDnsServers(v)) vm.config = vm.config.copy(dnsServers = v.trim())
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
                            leadingIcon = { Icon(Icons.Default.Dns, contentDescription = null) },
                            trailingIcon = {
                                if (dnsError)
                                    Icon(Icons.Default.Error, contentDescription = null, tint = MaterialTheme.colorScheme.error)
                                else if (dnsText.isNotBlank() && !dnsError)
                                    Icon(Icons.Default.CheckCircle, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                            },
                            singleLine = true,
                            enabled = !status.running,
                            keyboardOptions = KeyboardOptions(
                                keyboardType = KeyboardType.Ascii,
                                imeAction = ImeAction.Done
                            ),
                            modifier = Modifier.fillMaxWidth()
                        )
                        Spacer(Modifier.height(8.dp))
                      } // end !containerMode (DNS)

                        // Hide SSID
                        SwitchItem(
                            title = stringResource(R.string.hidden_ssid_label),
                            summary = stringResource(R.string.hidden_ssid_desc),
                            icon = Icons.Default.VisibilityOff,
                            checked = vm.config.hidden,
                            onCheckedChange = { vm.config = vm.config.copy(hidden = it) },
                            enabled = !status.running
                        )

                        // Protected Management Frames — only a real choice in WPA2.
                        // WPA2/WPA3 and WPA3 set it automatically per the standard,
                        // so the toggle is hidden for those modes.
                        if (vm.config.security == "wpa2") {
                            Spacer(Modifier.height(8.dp))
                            SwitchItem(
                                title = stringResource(R.string.pmf_label),
                                summary = stringResource(R.string.pmf_desc),
                                icon = Icons.Default.Security,
                                checked = vm.config.pmf,
                                onCheckedChange = { vm.setPmf(it) },
                                enabled = !status.running
                            )
                        }
                    }
                }
            }

            // --- 5. ACTION BUTTONS (outside the cards) ---
            item {
                Column {
                    // Start / Stop button
                    val isLoading = vm.isStarting || vm.isStopping
                    Button(
                        onClick = { if (status.running) vm.stop() else vm.start() },
                        modifier = Modifier.fillMaxWidth().height(52.dp),
                        enabled = !isLoading && (status.running || (vm.config.ssid.isNotBlank() && vm.passwordValid() && (!vm.config.containerMode || vm.config.containerName.isNotBlank()))),
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

            item { Spacer(Modifier.height(8.dp)) }
        }

            PullToRefreshContainer(
                state = pullState,
                modifier = Modifier.align(Alignment.TopCenter)
            )
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
    var showQr by remember { mutableStateOf(false) }

    val band = when (status.band) {
        "2", "2.4" -> "2.4 GHz"
        "5" -> "5 GHz"
        else -> status.band ?: "—"
    }
    val channel = status.channel?.let { " · ch$it" } ?: ""
    val width = status.width?.let { " · ${it}MHz" } ?: ""

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
                Row(verticalAlignment = Alignment.CenterVertically) {
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
                    IconButton(onClick = { showQr = true }, modifier = Modifier.size(36.dp)) {
                        Icon(
                            Icons.Default.QrCode2,
                            contentDescription = stringResource(R.string.qr_title),
                            tint = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.size(22.dp)
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
                        value = "$band$channel$width"
                    )
                }
                Column(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    if (status.mode == "bridged") {
                        DashboardStatRow(
                            icon = ImageVector.vectorResource(R.drawable.ic_droidspaces),
                            label = stringResource(R.string.container_label),
                            value = status.container ?: "—"
                        )
                        DashboardStatRow(
                            icon = Icons.Default.SwapVert,
                            label = stringResource(R.string.upstream_label),
                            value = stringResource(R.string.managed_label)
                        )
                    } else {
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

    if (showQr) {
        WifiQrSheet(vm = vm, onDismiss = { showQr = false })
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun WifiQrSheet(vm: APViewModel, onDismiss: () -> Unit) {
    val ssid = vm.status.ssid ?: vm.config.ssid
    // The password is still embedded in the QR (so scanning joins automatically);
    // it's just not shown on screen.
    val payload = remember(ssid, vm.config.password, vm.config.security, vm.config.hidden) {
        QrCodeGenerator.wifiPayload(ssid, vm.config.password, vm.config.security, vm.config.hidden)
    }
    val qr = remember(payload) { QrCodeGenerator.encode(payload, 600) }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .padding(bottom = 32.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                stringResource(R.string.qr_title),
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            Spacer(Modifier.height(4.dp))
            Text(
                stringResource(R.string.qr_scan_hint),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Spacer(Modifier.height(16.dp))
            Surface(
                shape = RoundedCornerShape(16.dp),
                color = androidx.compose.ui.graphics.Color.White,
                modifier = Modifier.size(240.dp)
            ) {
                Image(
                    bitmap = qr,
                    contentDescription = null,
                    modifier = Modifier.padding(12.dp).fillMaxSize()
                )
            }
            Spacer(Modifier.height(20.dp))
            Text(ssid, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
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
