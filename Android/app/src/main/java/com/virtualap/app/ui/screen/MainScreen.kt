package com.virtualap.app.ui.screen

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.res.vectorResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.virtualap.app.R
import com.virtualap.app.ui.component.CardContentPadding
import com.virtualap.app.ui.component.CardHeaderHeight
import com.virtualap.app.ui.component.DialogCloseButton
import com.virtualap.app.ui.component.DsDropdown
import com.virtualap.app.ui.component.DsTextFieldDefaults
import com.virtualap.app.ui.component.LogActionRow
import com.virtualap.app.ui.component.PrimaryActionBottomBar
import com.virtualap.app.ui.component.PullToRefreshWrapper
import com.virtualap.app.ui.component.StatusPill
import com.virtualap.app.ui.component.TerminalConsole
import com.virtualap.app.ui.component.ToggleCard
import com.virtualap.app.ui.util.ClearFocusOnClickOutside
import com.virtualap.app.ui.util.FullScreenLoading
import com.virtualap.app.ui.util.LoadingIndicator
import com.virtualap.app.ui.util.LoadingSize
import com.virtualap.app.ui.viewmodel.APViewModel
import com.virtualap.app.util.AnimationUtils
import com.virtualap.app.util.QrCodeGenerator
import kotlinx.coroutines.launch

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

private val fieldShape = RoundedCornerShape(16.dp)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    vm: APViewModel = viewModel(),
    onNavigateToSettings: () -> Unit = {},
    onRefresh: () -> Unit = {}
) {
    val status = vm.status
    val busy = vm.isStarting || vm.isStopping

    ClearFocusOnClickOutside {
        Scaffold(
            modifier = Modifier.fillMaxSize().imePadding(),
            containerColor = Color.Transparent,
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
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
                            )
                        }
                    },
                    actions = {
                        IconButton(onClick = { vm.openLogSheet() }) {
                            Icon(
                                imageVector = Icons.Default.Terminal,
                                contentDescription = stringResource(R.string.view_logs),
                                modifier = Modifier.size(18.dp)
                            )
                        }
                        IconButton(onClick = onNavigateToSettings) {
                            Icon(
                                imageVector = Icons.Default.Settings,
                                contentDescription = stringResource(R.string.settings_title)
                            )
                        }
                    }
                )
            },
            bottomBar = { if (vm.isReady) StartStopBar(vm) }
        ) { innerPadding ->
            // Gate the whole screen on the first load so nothing flashes stale state
            // or pops in late (status, interfaces and containers all arrive together).
            if (!vm.isReady) {
                FullScreenLoading(modifier = Modifier.padding(innerPadding))
                return@Scaffold
            }

            // Global pull-to-refresh: re-fetch status, interfaces, containers and
            // root in one gesture.
            PullToRefreshWrapper(
                onRefresh = {
                    vm.refreshAllNow()
                    onRefresh()
                },
                modifier = Modifier.fillMaxSize().padding(innerPadding)
            ) {
                LazyColumn(
                    modifier = Modifier.fillMaxSize().padding(horizontal = 16.dp),
                    verticalArrangement = Arrangement.spacedBy(16.dp),
                    contentPadding = PaddingValues(vertical = 16.dp)
                ) {
                    item {
                        AnimatedVisibility(
                            visible = status.running,
                            enter = expandVertically(animationSpec = AnimationUtils.mediumSpec()) +
                                fadeIn(animationSpec = AnimationUtils.mediumSpec()),
                            exit = shrinkVertically(animationSpec = AnimationUtils.mediumSpec()) +
                                fadeOut(animationSpec = AnimationUtils.fadeOutSpec())
                        ) {
                            ActiveNetworkCard(vm = vm)
                        }
                    }
                    item { AccessPointCard(vm = vm) }
                    item { UpstreamCard(vm = vm) }
                    item { AdvancedCard(vm = vm) }
                }
            }
        }
    }

    // Unified log bottom sheet: auto-opens on start/stop, re-openable from the top bar.
    if (vm.showActionLogs) {
        ActionLogsSheet(
            logs = if (vm.actionLogs.isNotEmpty()) vm.actionLogs else vm.fallbackLogs,
            isProcessing = busy,
            onDismiss = { if (!busy) vm.dismissActionLogs() },
            onClear = { vm.clearLog() }
        )
    }
}

/** Start/Stop as the screen's one primary action: error-tinted while running. */
@Composable
private fun StartStopBar(vm: APViewModel) {
    val status = vm.status
    val busy = vm.isStarting || vm.isStopping
    val canStart = vm.config.ssid.isNotBlank() && vm.passwordValid() &&
        (!vm.config.containerMode || vm.config.containerName.isNotBlank())
    val enabled = !busy && (status.running || canStart)
    val container = when {
        !enabled && !busy -> MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
        status.running -> MaterialTheme.colorScheme.error
        else -> MaterialTheme.colorScheme.primary
    }
    val content = when {
        !enabled && !busy -> MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f)
        status.running -> MaterialTheme.colorScheme.onError
        else -> MaterialTheme.colorScheme.onPrimary
    }

    PrimaryActionBottomBar(
        onClick = { if (status.running) vm.stop() else vm.start() },
        containerColor = container,
        enabled = enabled
    ) {
        if (busy) {
            LoadingIndicator(modifier = Modifier.size(20.dp), color = content)
            Text(
                text = stringResource(if (vm.isStarting) R.string.starting else R.string.stopping),
                style = MaterialTheme.typography.labelLarge,
                fontWeight = FontWeight.SemiBold,
                color = content
            )
        } else {
            Icon(
                imageVector = if (status.running) Icons.Default.WifiOff else Icons.Default.Wifi,
                contentDescription = null,
                modifier = Modifier.size(18.dp),
                tint = content
            )
            Text(
                text = stringResource(if (status.running) R.string.stop_ap else R.string.start_ap),
                style = MaterialTheme.typography.labelLarge,
                fontWeight = FontWeight.SemiBold,
                color = content
            )
        }
    }
}

/** A titled card on the screen background: surfaceContainer, radius 20, 1dp border. */
@Composable
private fun ConfigCard(title: String, content: @Composable ColumnScope.() -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.surfaceContainer,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.35f)),
        tonalElevation = 0.dp
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(text = title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
            content()
        }
    }
}

@Composable
private fun Caption(text: String) {
    Text(
        text = text,
        style = MaterialTheme.typography.bodySmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
    )
}

@Composable
private fun AccessPointCard(vm: APViewModel) {
    val status = vm.status
    val editable = !status.running
    var passwordVisible by remember { mutableStateOf(false) }
    val autoLabel = stringResource(R.string.auto_label)

    ConfigCard(title = stringResource(R.string.access_point_title)) {
        OutlinedTextField(
            value = vm.config.ssid,
            onValueChange = { vm.config = vm.config.copy(ssid = it) },
            label = { Text(stringResource(R.string.ssid_label)) },
            supportingText = if (vm.config.ssid.isBlank() && editable) {
                { Text(stringResource(R.string.enter_ssid_prompt)) }
            } else null,
            modifier = Modifier.fillMaxWidth(),
            shape = fieldShape,
            colors = DsTextFieldDefaults.colors(),
            singleLine = true,
            enabled = editable
        )

        val securityNames = mapOf(
            "open" to stringResource(R.string.security_open),
            "wpa2" to stringResource(R.string.security_wpa2),
            "wpa2wpa3" to stringResource(R.string.security_wpa2_wpa3),
            "wpa3" to stringResource(R.string.security_wpa3)
        )
        DsDropdown(
            label = stringResource(R.string.security_label),
            selected = vm.config.security,
            options = securityNames.keys.toList(),
            displayName = { securityNames[it] ?: it },
            onSelect = { vm.selectSecurity(it) },
            enabled = editable
        )
        // WPA3 (incl. the transition mode) can be rejected by older client
        // devices; warn so users know to fall back to WPA2.
        if (vm.config.security == "wpa2wpa3" || vm.config.security == "wpa3") {
            Caption(stringResource(R.string.security_wpa3_hint))
        }

        // Open networks have no passphrase, so the field goes away with them.
        if (vm.passwordRequired()) {
            // WPA-PSK/SAE passphrase is 8-63 chars; flag an out-of-range value
            // once the user has started typing (blank stays neutral).
            val pwLen = vm.config.password.length
            val pwError = pwLen in 1..7 || pwLen > 63
            OutlinedTextField(
                value = vm.config.password,
                onValueChange = { vm.config = vm.config.copy(password = it) },
                label = { Text(stringResource(R.string.password_label)) },
                modifier = Modifier.fillMaxWidth(),
                shape = fieldShape,
                colors = DsTextFieldDefaults.colors(),
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
                            imageVector = if (passwordVisible) Icons.Default.VisibilityOff else Icons.Default.Visibility,
                            contentDescription = stringResource(
                                if (passwordVisible) R.string.hide_password_desc else R.string.show_password_desc
                            )
                        )
                    }
                },
                enabled = editable
            )
        }

        val bandNames = mapOf(
            "2" to stringResource(R.string.band_2ghz),
            "5" to stringResource(R.string.band_5ghz)
        )
        DsDropdown(
            label = stringResource(R.string.band_label),
            selected = vm.config.band,
            options = bandNames.keys.toList(),
            displayName = { bandNames[it] ?: it },
            onSelect = { vm.selectBand(it) },   // resets channel: valid channels differ per band
            enabled = editable
        )

        val channelOptions = if (vm.config.band == "5") {
            listOf("", "36", "40", "44", "48", "149", "153", "157", "161", "165")
        } else {
            listOf("") + (1..11).map { "$it" }
        }
        DsDropdown(
            label = stringResource(R.string.channel_label),
            selected = vm.config.channel,
            options = channelOptions,
            displayName = { it.ifEmpty { autoLabel } },
            onSelect = { vm.selectChannel(it) },
            enabled = editable
        )

        // All widths are always selectable; the backend downgrades an
        // unsupported width (wrong band/chip/channel) to the widest it can do.
        val widthNames = mapOf(
            "auto" to autoLabel,
            "20" to stringResource(R.string.width_20),
            "40" to stringResource(R.string.width_40),
            "80" to stringResource(R.string.width_80)
        )
        DsDropdown(
            label = stringResource(R.string.width_label),
            selected = vm.config.width,
            options = widthNames.keys.toList(),
            displayName = { widthNames[it] ?: it },
            onSelect = { vm.selectWidth(it) },
            enabled = editable
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun UpstreamCard(vm: APViewModel) {
    val status = vm.status
    val editable = !status.running
    val hasContainers = vm.containers.isNotEmpty()

    ConfigCard(title = stringResource(R.string.upstream_card_title)) {
        // Interface | Container selector, shown only when Droidspaces is present
        // with at least one running container. Material's SegmentedButton stays
        // here: it is a two-way selector, not a row of peer actions, so the
        // action pill pattern does not apply.
        if (hasContainers) {
            SingleChoiceSegmentedButtonRow(modifier = Modifier.fillMaxWidth()) {
                SegmentedButton(
                    selected = !vm.config.containerMode,
                    onClick = { if (editable) vm.config = vm.config.copy(containerMode = false) },
                    shape = SegmentedButtonDefaults.itemShape(index = 0, count = 2),
                    enabled = editable,
                    icon = {}
                ) { Text(stringResource(R.string.interface_label)) }
                SegmentedButton(
                    selected = vm.config.containerMode,
                    onClick = {
                        if (editable) vm.config = vm.config.copy(
                            containerMode = true,
                            containerName = vm.config.containerName.ifBlank { vm.containers.first() }
                        )
                    },
                    shape = SegmentedButtonDefaults.itemShape(index = 1, count = 2),
                    enabled = editable,
                    icon = {}
                ) { Text(stringResource(R.string.container_label)) }
            }
        }

        if (vm.config.containerMode && hasContainers) {
            // The container owns DHCP/NAT.
            DsDropdown(
                label = stringResource(R.string.container_label),
                selected = vm.config.containerName.ifBlank { vm.containers.first() },
                options = vm.containers,
                displayName = { it },
                onSelect = { vm.config = vm.config.copy(containerName = it) },
                leadingIcon = ImageVector.vectorResource(R.drawable.ic_droidspaces),
                enabled = editable
            )
            Caption(stringResource(R.string.container_upstream_desc))
        } else {
            val autoLabel = stringResource(R.string.upstream_auto)
            val ifaces = vm.interfaces.filter { it.name != "ap0" }
            DsDropdown(
                label = stringResource(R.string.upstream_interface_label),
                selected = vm.config.upstream,
                options = listOf("auto") + ifaces.map { it.name },
                displayName = { name ->
                    if (name == "auto") autoLabel
                    else ifaces.find { it.name == name }?.ip?.let { "$name ($it)" } ?: name
                },
                onSelect = { vm.config = vm.config.copy(upstream = it) },
                enabled = editable
            )
            Caption(stringResource(R.string.interface_upstream_desc))
        }
    }
}

@Composable
private fun AdvancedCard(vm: APViewModel) {
    val status = vm.status
    val editable = !status.running

    ConfigCard(title = stringResource(R.string.advanced_settings_title)) {
        // Gateway IP: the AP subnet gateway in routed mode and the LAN gateway
        // provisioned inside the container in managed mode, so it's shown in
        // both. Blank uses the default.
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
                    Text(stringResource(R.string.gateway_ip_desc))
            },
            isError = gatewayError,
            leadingIcon = { Icon(Icons.Default.Router, contentDescription = null) },
            trailingIcon = {
                if (gatewayError)
                    Icon(Icons.Default.Error, contentDescription = null, tint = MaterialTheme.colorScheme.error)
            },
            singleLine = true,
            enabled = editable,
            keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number, imeAction = ImeAction.Next),
            modifier = Modifier.fillMaxWidth(),
            shape = fieldShape,
            colors = DsTextFieldDefaults.colors()
        )

        // DNS is VirtualAP's own L3, irrelevant when a container owns the LAN
        // (it runs its own resolver), so hide it in managed mode.
        if (!vm.config.containerMode) {
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
                        Text(stringResource(R.string.dns_servers_desc))
                },
                isError = dnsError,
                leadingIcon = { Icon(Icons.Default.Dns, contentDescription = null) },
                trailingIcon = {
                    if (dnsError)
                        Icon(Icons.Default.Error, contentDescription = null, tint = MaterialTheme.colorScheme.error)
                    else if (dnsText.isNotBlank())
                        Icon(Icons.Default.CheckCircle, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                },
                singleLine = true,
                enabled = editable,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Ascii, imeAction = ImeAction.Done),
                modifier = Modifier.fillMaxWidth(),
                shape = fieldShape,
                colors = DsTextFieldDefaults.colors()
            )
        }

        ToggleCard(
            title = stringResource(R.string.hidden_ssid_label),
            description = stringResource(R.string.hidden_ssid_desc),
            checked = vm.config.hidden,
            onCheckedChange = { vm.config = vm.config.copy(hidden = it) },
            enabled = editable,
            icon = Icons.Default.VisibilityOff
        )

        // Protected Management Frames is only a real choice in WPA2. WPA2/WPA3
        // and WPA3 set it automatically per the standard, so the toggle is
        // hidden for those modes.
        if (vm.config.security == "wpa2") {
            ToggleCard(
                title = stringResource(R.string.pmf_label),
                description = stringResource(R.string.pmf_desc),
                checked = vm.config.pmf,
                onCheckedChange = { vm.setPmf(it) },
                enabled = editable,
                icon = Icons.Default.Security
            )
        }
    }
}

@Composable
private fun ActiveNetworkCard(vm: APViewModel) {
    val status = vm.status
    var showQr by remember { mutableStateOf(false) }

    val band = when (status.band) {
        "2", "2.4" -> stringResource(R.string.band_2ghz)
        "5" -> stringResource(R.string.band_5ghz)
        else -> status.band ?: stringResource(R.string.unknown)
    }
    val radio = listOfNotNull(
        band,
        status.channel?.let { stringResource(R.string.channel_short, it) },
        status.width?.let { stringResource(R.string.width_mhz, it) }
    ).joinToString(" · ")

    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.surfaceContainer,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.35f)),
        tonalElevation = 0.dp
    ) {
        Column(
            modifier = Modifier.padding(CardContentPadding),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().height(CardHeaderHeight),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = stringResource(R.string.active_network_title),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                    modifier = Modifier.weight(1f)
                )
                StatusPill(
                    label = stringResource(R.string.status_running).uppercase(),
                    color = MaterialTheme.colorScheme.primary
                )
                IconButton(onClick = { showQr = true }) {
                    Icon(
                        imageVector = Icons.Default.QrCode2,
                        contentDescription = stringResource(R.string.qr_title),
                        tint = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.size(20.dp)
                    )
                }
            }
            HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.3f))

            status.ssid?.let { ssid ->
                Text(text = ssid, style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
            }
            status.started?.let { Caption(stringResource(R.string.since_time, it)) }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    StatRow(Icons.Default.Router, stringResource(R.string.gateway_label), status.gateway)
                    StatRow(Icons.Default.SignalCellularAlt, stringResource(R.string.band_label), radio)
                }
                Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    if (status.mode == "bridged") {
                        StatRow(
                            ImageVector.vectorResource(R.drawable.ic_droidspaces),
                            stringResource(R.string.container_label),
                            status.container ?: stringResource(R.string.unknown)
                        )
                        StatRow(Icons.Default.SwapVert, stringResource(R.string.upstream_label), stringResource(R.string.managed_label))
                    } else {
                        StatRow(
                            Icons.Default.SwapVert,
                            stringResource(R.string.upstream_label),
                            status.upstream ?: stringResource(R.string.auto_label)
                        )
                        StatRow(
                            Icons.Default.SettingsEthernet,
                            stringResource(R.string.interface_label),
                            status.upstreamIface ?: stringResource(R.string.unknown)
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

@Composable
private fun StatRow(icon: ImageVector, label: String, value: String) {
    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            tint = MaterialTheme.colorScheme.primary.copy(alpha = 0.7f),
            modifier = Modifier.size(16.dp)
        )
        Column {
            Caption(label)
            Text(text = value, style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
        }
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
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        shape = RoundedCornerShape(topStart = 24.dp, topEnd = 24.dp),
        containerColor = MaterialTheme.colorScheme.surfaceContainer,
        tonalElevation = 0.dp
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .padding(bottom = 32.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = stringResource(R.string.qr_title),
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold
            )
            Spacer(Modifier.height(4.dp))
            Caption(stringResource(R.string.qr_scan_hint))
            Spacer(Modifier.height(16.dp))
            // The plate stays white under every palette: a scanner needs the
            // contrast the QR spec assumes, and a themed plate reads as a broken code.
            Surface(
                shape = RoundedCornerShape(16.dp),
                color = Color.White,
                modifier = Modifier.size(240.dp)
            ) {
                Image(
                    bitmap = qr,
                    contentDescription = null,
                    modifier = Modifier.padding(12.dp).fillMaxSize()
                )
            }
            Spacer(Modifier.height(20.dp))
            Text(text = ssid, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
        }
    }
}

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
    // state-machine level. Guarding only onDismissRequest is not enough:
    // gesture dismissal animates the sheet away BEFORE that callback fires,
    // so the sheet ended up hidden while showActionLogs stayed true, leaving
    // an invisible scrim that ate every touch once the command finished.
    val processing by rememberUpdatedState(isProcessing)
    val sheetState = rememberModalBottomSheetState(
        skipPartiallyExpanded = true,
        confirmValueChange = { it != SheetValue.Hidden || !processing }
    )
    val scope = rememberCoroutineScope()

    // Animated dismiss: slide the sheet out first, then notify the caller.
    val animatedDismiss: () -> Unit = animatedDismiss@{
        if (isProcessing) return@animatedDismiss
        scope.launch {
            sheetState.hide()
            onDismiss()
        }
    }

    ModalBottomSheet(
        // Only reachable when confirmValueChange allowed Hidden (not processing).
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        shape = RoundedCornerShape(topStart = 24.dp, topEnd = 24.dp),
        containerColor = MaterialTheme.colorScheme.surfaceContainer,
        tonalElevation = 0.dp
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .padding(bottom = 32.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    if (isProcessing) LoadingIndicator(size = LoadingSize.Small)
                    Text(
                        text = stringResource(if (isProcessing) R.string.running_log_title else R.string.logs_title),
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.Bold
                    )
                }
                DialogCloseButton(onClick = animatedDismiss, enabled = !isProcessing)
            }

            LogActionRow(logs = logs, isBlocking = isProcessing, onClear = onClear)

            TerminalConsole(
                logs = if (logs.isEmpty()) listOf(android.util.Log.INFO to stringResource(R.string.no_logs_msg)) else logs,
                isProcessing = isProcessing,
                modifier = Modifier.fillMaxWidth(),
                maxHeight = 460.dp
            )
        }
    }
}
