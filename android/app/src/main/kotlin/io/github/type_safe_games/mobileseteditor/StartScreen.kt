// Mobile Set Editor - the start screen: the Working copies, Open Set, Templates, About
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.text.format.DateUtils
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import io.github.type_safe_games.mobileseteditor.core.WorkingCopies
import io.github.type_safe_games.mobileseteditor.core.WorkingCopy

/** The Working copies (name, when last edited, unsaved or not), Open Set, Templates, New Set and About */
@Composable
fun StartScreen(onOpenSet: () -> Unit, onOpenCopy: (WorkingCopy) -> Unit, onTemplates: () -> Unit, onNewSet: () -> Unit = {}, onAbout: () -> Unit = {}) {
    val context = LocalContext.current
    var copies by remember { mutableStateOf(WorkingCopies.list(context)) }
    var removing by remember { mutableStateOf<WorkingCopy?>(null) }
    Column(Modifier.fillMaxSize().safeDrawingPadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Mobile Set Editor", style = MaterialTheme.typography.headlineSmall, modifier = Modifier.weight(1f))
            TextButton(onClick = onAbout, modifier = Modifier.testTag("about-button")) { Text("About") }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onOpenSet, modifier = Modifier.testTag("open-set")) { Text("Open Set") }
            OutlinedButton(onClick = onTemplates) { Text("Templates") }
            OutlinedButton(onClick = onNewSet, modifier = Modifier.testTag("new-set")) { Text("New Set") }
        }
        HorizontalDivider()
        Text(if (copies.isEmpty()) "No Sets open yet" else "Working copies", style = MaterialTheme.typography.titleSmall)
        LazyColumn(Modifier.fillMaxWidth().testTag("working-copies")) {
            items(copies, key = { it.id }) { copy ->
                Row(
                    Modifier.fillMaxWidth().clickable { onOpenCopy(copy) }.padding(vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(copy.name, style = MaterialTheme.typography.bodyLarge)
                        val edited = DateUtils.getRelativeTimeSpanString(copy.lastEdited)
                        Text(
                            "Edited $edited" + if (copy.unsaved) " · unsaved changes" else "",
                            style = MaterialTheme.typography.bodySmall,
                            color = if (copy.unsaved) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    TextButton(onClick = { removing = copy }) { Text("Remove") }
                }
            }
        }
    }
    removing?.let { copy ->
        AlertDialog(
            onDismissRequest = { removing = null },
            title = { Text("Remove ${copy.name}?") },
            text = {
                Text(
                    if (copy.unsaved) "Its changes have not been saved to the document, and will be lost."
                    else "The document itself is not affected.",
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    WorkingCopies.delete(context, copy)
                    copies = WorkingCopies.list(context)
                    removing = null
                }) { Text("Remove") }
            },
            dismissButton = { TextButton(onClick = { removing = null }) { Text("Cancel") } },
        )
    }
}
