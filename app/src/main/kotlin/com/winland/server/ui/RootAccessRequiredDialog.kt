package com.winland.server.ui

import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.TextButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable

@Composable
fun RootAccessRequiredDialog(onRetry: () -> Unit, onExit: () -> Unit) {
    AlertDialog(
        onDismissRequest = { },
        title = { Text("Root Access Required") },
        text = { Text("Please grant root permission to use Winland.") },
        confirmButton = {
            Button(onClick = onRetry) {
                Text("Retry")
            }
        },
        dismissButton = {
            TextButton(onClick = onExit) {
                Text("Exit", color = MaterialTheme.colorScheme.error)
            }
        }
    )
}