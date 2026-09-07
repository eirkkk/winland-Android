package com.winland.server.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.winland.server.ExecutionMode

/**
 * Lets the user pick the Linux-guest execution backend:
 * - ROOT: classic `su` + real chroot/mount (needs rooted device, best performance + GPU).
 * - PROOT: rootless ptrace-based backend (works without root, software rendering).
 */
@Composable
fun ExecutionModeDialog(
    initial: ExecutionMode,
    rootAvailable: Boolean,
    onConfirm: (ExecutionMode) -> Unit
) {
    var selected by remember { mutableStateOf(initial) }

    AlertDialog(
        onDismissRequest = { },
        title = { Text("Choose Execution Mode") },
        text = {
            Column(modifier = Modifier.fillMaxWidth()) {
                ModeOption(
                    title = "Root mode",
                    description = "Full chroot with GPU acceleration. Requires a rooted device.",
                    enabled = rootAvailable,
                    chosen = selected == ExecutionMode.ROOT,
                    onClick = { selected = ExecutionMode.ROOT }
                )
                Spacer(modifier = Modifier.height(8.dp))
                ModeOption(
                    title = "Rootless (proot) mode",
                    description = "Works without root. Software rendering; ~15-20% slower.",
                    enabled = true,
                    chosen = selected == ExecutionMode.PROOT,
                    onClick = { selected = ExecutionMode.PROOT }
                )
                if (!rootAvailable) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        text = "No root detected — rootless mode was selected automatically.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.secondary
                    )
                }
            }
        },
        confirmButton = {
            Button(onClick = { onConfirm(selected) }) {
                Text("Start")
            }
        }
    )
}

@Composable
private fun ModeOption(
    title: String,
    description: String,
    enabled: Boolean,
    chosen: Boolean,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = enabled, onClick = onClick)
            .padding(4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        RadioButton(selected = chosen, onClick = null, enabled = enabled)
        Column(modifier = Modifier.padding(start = 8.dp)) {
            Text(
                text = title,
                style = MaterialTheme.typography.titleSmall,
                color = if (enabled) MaterialTheme.colorScheme.onSurface
                else MaterialTheme.colorScheme.onSurface.copy(alpha = 0.4f)
            )
            Text(
                text = description,
                style = MaterialTheme.typography.bodySmall,
                color = if (enabled) MaterialTheme.colorScheme.onSurfaceVariant
                else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.4f)
            )
        }
    }
}
