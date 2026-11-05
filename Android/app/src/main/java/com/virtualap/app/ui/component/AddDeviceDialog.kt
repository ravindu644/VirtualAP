package com.virtualap.app.ui.component

import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.selection.toggleable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.pluralStringResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardCapitalization
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.OffsetMapping
import androidx.compose.ui.text.input.TransformedText
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import com.virtualap.app.R
import com.virtualap.app.ui.theme.JetBrainsMono
import com.virtualap.app.util.ApClient

private const val MAC_DIGITS = 12

// A whole address inside pasted text, so the letters of a label beside it do not count as digits
// and digits already in the field do not join it. No word boundary on the colon form: those digits touch it.
private val WHOLE_MAC = Regex("""[0-9a-f]{2}([:-])(?:[0-9a-f]{2}\1){4}[0-9a-f]{2}|\b[0-9a-f]{12}\b""", RegexOption.IGNORE_CASE)

/** The field holds bare digits and this draws the colons, so the cursor never lands on one. */
private object MacColons : VisualTransformation {
    override fun filter(text: AnnotatedString) = TransformedText(
        AnnotatedString(text.text.chunked(2).joinToString(":")),
        object : OffsetMapping {
            override fun originalToTransformed(offset: Int) = offset + (offset - 1).coerceAtLeast(0) / 2
            override fun transformedToOriginal(offset: Int) = offset - offset / 3
        }
    )
}

/** Why the digits cannot be added yet, or null when they can. The engine refuses the same. */
@StringRes
private fun macProblem(digits: String): Int? = when {
    digits.all { it == '0' } -> R.string.mac_zeros
    digits.all { it == 'f' } -> R.string.mac_broadcast
    // The low bit of the first byte marks a group address, which no device sends from.
    digits[1] in "13579bdf" -> R.string.mac_group
    else -> null
}

/**
 * Add a device by hand. Anything but a hex digit never reaches the field, whatever is typed or
 * pasted. What is well formed and still wrong is said under it, with Add off until it is right.
 */
@Composable
fun AddDeviceDialog(
    @StringRes title: Int,
    list: List<ApClient>,
    other: List<ApClient>,
    @StringRes onOtherNote: Int,
    onDismiss: () -> Unit,
    onAdd: (mac: String, name: String?) -> Unit
) {
    var digits by rememberSaveable { mutableStateOf("") }
    var name by rememberSaveable { mutableStateOf("") }
    val mac = digits.chunked(2).joinToString(":")
    val complete = digits.length == MAC_DIGITS
    val problem = if (complete) macProblem(digits) else null
    val listed = if (complete) list.find { it.mac == mac } else null
    val canAdd = complete && problem == null && listed == null
    val moving = canAdd && other.any { it.mac == mac }

    DsDialog(
        onDismiss = onDismiss,
        modifier = Modifier.imePadding(),
        footer = {
            DialogFooterRow(
                dismissLabel = stringResource(R.string.cancel),
                confirmLabel = stringResource(R.string.device_add_confirm),
                onDismiss = onDismiss,
                // Left blank on a move, it keeps the name it was saved under.
                onConfirm = { onAdd(mac, name.ifEmpty { other.find { it.mac == mac }?.name }) },
                confirmEnabled = canAdd
            )
        }
    ) {
        Text(
            text = stringResource(title),
            style = MaterialTheme.typography.titleLarge,
            fontWeight = FontWeight.Bold
        )
        OutlinedTextField(
            value = digits,
            onValueChange = { typed ->
                val hex = (WHOLE_MAC.find(typed)?.value ?: typed).filter { it in "0123456789abcdefABCDEF" }
                // Past 12 is refused, not cut: a cut paste would be a valid address of some other device.
                if (hex.length <= MAC_DIGITS) digits = hex.lowercase()
            },
            label = { Text(stringResource(R.string.mac_label)) },
            supportingText = {
                when {
                    problem != null -> Text(stringResource(problem))
                    listed != null -> Text(
                        stringResource(R.string.mac_listed, listed.name ?: stringResource(R.string.client_unnamed))
                    )
                    moving -> Text(stringResource(onOtherNote), color = MaterialTheme.colorScheme.tertiary)
                    digits.isEmpty() -> Text(stringResource(R.string.mac_help))
                    !complete -> {
                        val left = MAC_DIGITS - digits.length
                        Text(pluralStringResource(R.plurals.mac_digits_left, left, left))
                    }
                }
            },
            isError = problem != null || listed != null,
            visualTransformation = MacColons,
            textStyle = TextStyle(fontFamily = JetBrainsMono),
            keyboardOptions = KeyboardOptions(
                capitalization = KeyboardCapitalization.None,
                autoCorrectEnabled = false,
                keyboardType = KeyboardType.Ascii
            ),
            singleLine = true,
            shape = RoundedCornerShape(16.dp),
            colors = DsTextFieldDefaults.colors(),
            modifier = Modifier.fillMaxWidth()
        )
        OutlinedTextField(
            value = name,
            // One word of printable ASCII is all the engine keeps, so a space becomes a hyphen here.
            onValueChange = { typed ->
                name = typed.map { if (it.isWhitespace()) '-' else it }
                    .filter { it in '!'..'~' }
                    .joinToString("")
                    .take(63)
            },
            label = { Text(stringResource(R.string.device_name_optional)) },
            singleLine = true,
            shape = RoundedCornerShape(16.dp),
            colors = DsTextFieldDefaults.colors(),
            modifier = Modifier.fillMaxWidth()
        )
        Caption(stringResource(R.string.mac_random_note))
    }
}

/**
 * Turning on the allowed list with devices connected that are not on it: they drop off unless
 * ticked. None start ticked, so nobody already here gets approved by a tap on Turn on.
 */
@Composable
fun AllowOnlyDialog(unlisted: List<ApClient>, onDismiss: () -> Unit, onConfirm: (kept: List<ApClient>) -> Unit) {
    val kept = remember { mutableStateListOf<ApClient>() }
    DsDialog(
        onDismiss = onDismiss,
        footer = {
            DialogFooterRow(
                dismissLabel = stringResource(R.string.cancel),
                confirmLabel = stringResource(R.string.allow_only_turn_on),
                onDismiss = onDismiss,
                onConfirm = { onConfirm(kept.toList()) }
            )
        }
    ) {
        Text(
            text = stringResource(R.string.allow_only_confirm_title),
            style = MaterialTheme.typography.titleLarge,
            fontWeight = FontWeight.Bold
        )
        Text(text = stringResource(R.string.allow_only_confirm_body), style = MaterialTheme.typography.bodyMedium)
        Column {
            unlisted.forEach { device ->
                val ticked = device in kept
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .heightIn(min = 56.dp)
                        .toggleable(
                            value = ticked,
                            role = Role.Checkbox,
                            onValueChange = { if (it) kept.add(device) else kept.remove(device) }
                        ),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    Checkbox(checked = ticked, onCheckedChange = null)
                    Column {
                        Text(
                            text = device.name ?: stringResource(R.string.client_unnamed),
                            style = MaterialTheme.typography.bodyMedium,
                            fontWeight = FontWeight.Medium
                        )
                        Text(
                            text = device.mac,
                            style = MaterialTheme.typography.bodySmall,
                            fontFamily = JetBrainsMono,
                            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.7f)
                        )
                    }
                }
            }
        }
    }
}
