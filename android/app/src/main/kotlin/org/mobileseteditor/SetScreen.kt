// Mobile Set Editor - an open Set's Cards, one at a time
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.content.pm.ApplicationInfo
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Image
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch

/** The open Set: a Card rendered to the screen's width, previous/next, and the core's messages */
@Composable
fun SetScreen(
    model: SetViewModel,
    onOpen: () -> Unit,
    onBack: () -> Unit,
    onSaveAs: () -> Unit = {},
    onClose: () -> Unit = onBack,
) {
    val state by model.state.collectAsState()
    val scope = rememberCoroutineScope()
    val context = LocalContext.current
    val debuggable = (context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
    var askClose by remember { mutableStateOf(false) }
    val close: () -> Unit = { if (state.dirty) askClose = true else { model.close(CloseChoice.Keep, onClose) } }
    BackHandler(enabled = state.session != 0L) { close() }
    // (scrolls: a Card at the screen's width can be taller than the screen)
    Column(
        Modifier.fillMaxSize().safeDrawingPadding().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = onBack) { Text("Start") }
            Button(onClick = onOpen, modifier = Modifier.testTag("open-set")) { Text("Open Set") }
        }
        state.workingCopy?.let { copy ->
            Text(
                copy.name + if (state.dirty) " · unsaved changes" else " · saved",
                style = MaterialTheme.typography.titleSmall,
                modifier = Modifier.testTag("set-name"),
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = { scope.launch { if (!model.save()) onSaveAs() } },
                    enabled = !state.saving && (state.dirty || copy.uri == null),
                    modifier = Modifier.testTag("save"),
                ) { Text("Save") }
                OutlinedButton(onClick = onSaveAs, enabled = !state.saving) { Text("Save as…") }
                OutlinedButton(onClick = close, enabled = !state.saving) { Text("Close") }
                if (debuggable) TextButton(onClick = { model.testEdit() }, modifier = Modifier.testTag("test-edit")) { Text("Test edit") }
            }
        }
        if (state.cardUids.isNotEmpty()) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = model::previous, enabled = state.index > 0, modifier = Modifier.testTag("previous")) { Text("‹") }
                Text(
                    "${state.index + 1} / ${state.cardUids.size}  ${state.cardNames.getOrElse(state.index) { "" }}",
                    modifier = Modifier.weight(1f).testTag("card-position"),
                    maxLines = 1,
                )
                OutlinedButton(onClick = model::next, enabled = state.index < state.cardUids.size - 1, modifier = Modifier.testTag("next")) { Text("›") }
            }
        }
        // the Card, rendered to this width in pixels (so at the screen's density)
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val width = constraints.maxWidth
            LaunchedEffect(width) { model.setWidth(width) }
            val card = state.card
            when {
                card != null -> Image(
                    bitmap = card.bitmap.asImageBitmap(),
                    contentDescription = card.name,
                    modifier = Modifier.fillMaxWidth().aspectRatio(card.bitmap.width.toFloat() / card.bitmap.height).testTag("card"),
                )
                state.opening || state.session != 0L ->
                    Box(Modifier.fillMaxWidth().aspectRatio(0.716f).testTag("card-placeholder"), contentAlignment = Alignment.Center) {
                        CircularProgressIndicator()
                    }
                else -> Box(Modifier.fillMaxWidth().padding(vertical = 48.dp), contentAlignment = Alignment.Center) {
                    Text("Open a Set to see its Cards")
                }
            }
        }
        if (state.messages.isNotEmpty()) {
            Column(Modifier.fillMaxWidth().testTag("messages")) {
                for (m in state.messages.takeLast(50)) {
                    Text(m, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error)
                }
            }
        }
    }
    if (askClose) {
        AlertDialog(
            onDismissRequest = { askClose = false },
            title = { Text("Unsaved changes") },
            text = { Text("Save them to the document, discard them, or keep them in the Working copy to come back to?") },
            confirmButton = {
                TextButton(onClick = { askClose = false; model.close(CloseChoice.Save, onClose) }) { Text("Save") }
            },
            dismissButton = {
                Row {
                    TextButton(onClick = { askClose = false; model.close(CloseChoice.Discard, onClose) }) { Text("Discard") }
                    TextButton(onClick = { askClose = false; model.close(CloseChoice.Keep, onClose) }) { Text("Keep") }
                }
            },
        )
    }
}
