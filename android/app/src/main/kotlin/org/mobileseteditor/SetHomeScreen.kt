// Mobile Set Editor - the Set's home screen: its Cards as a grid of thumbnails, Set info, Keywords
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.content.pm.ApplicationInfo
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyGridState
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.itemsIndexed
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import org.mobileseteditor.render.RenderKey

private enum class SetTab(val title: String) { Cards("Cards"), SetInfo("Set info"), Keywords("Keywords") }

/**
 * The open Set's home (#17): a top bar with its name, unsaved state, Save, Save as and Close, and
 * three tabs. Cards is a grid of thumbnails; tapping one opens that Card. [grid] is kept by the
 * caller, so coming back from a Card finds the grid where it was.
 */
@Composable
fun SetHomeScreen(
    model: SetViewModel,
    grid: LazyGridState,
    onOpenCard: (Int) -> Unit,
    onSaveAs: () -> Unit,
    onClosed: () -> Unit,
) {
    val state by model.state.collectAsState()
    val scope = rememberCoroutineScope()
    val context = LocalContext.current
    val debuggable = (context.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
    var tab by rememberSaveable { mutableStateOf(SetTab.Cards) }
    var askClose by remember { mutableStateOf(false) }
    val close: () -> Unit = { if (state.dirty) askClose = true else { model.close(CloseChoice.Keep, onClosed) } }
    BackHandler { close() }

    Column(Modifier.fillMaxSize().safeDrawingPadding()) {
        // the top bar
        Column(Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            val copy = state.workingCopy
            Text(
                (copy?.name ?: "Opening…") + if (state.dirty) " · unsaved changes" else "",
                style = MaterialTheme.typography.titleMedium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.testTag("set-name"),
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(
                    onClick = { scope.launch { if (!model.save()) onSaveAs() } },
                    enabled = copy != null && !state.saving && (state.dirty || copy.uri == null),
                    modifier = Modifier.testTag("save"),
                ) { Text("Save") }
                OutlinedButton(onClick = onSaveAs, enabled = copy != null && !state.saving) { Text("Save as…") }
                OutlinedButton(onClick = close, enabled = !state.saving) { Text("Close") }
                if (debuggable && copy != null) {
                    TextButton(onClick = { model.testEdit() }, modifier = Modifier.testTag("test-edit")) { Text("Test edit") }
                }
            }
        }
        TabRow(selectedTabIndex = tab.ordinal) {
            for (t in SetTab.entries) Tab(selected = tab == t, onClick = { tab = t }, text = { Text(t.title) })
        }
        when (tab) {
            SetTab.Cards -> CardsGrid(model, state, grid, onOpenCard)
            SetTab.SetInfo -> Placeholder("Set info and the Set symbol come here.")
            SetTab.Keywords -> Placeholder("The Set's keywords come here.")
        }
        if (state.messages.isNotEmpty()) {
            Column(Modifier.fillMaxWidth().padding(8.dp).testTag("messages")) {
                for (m in state.messages.takeLast(3)) {
                    Text(m, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error, maxLines = 2)
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
                TextButton(onClick = { askClose = false; model.close(CloseChoice.Save, onClosed) }) { Text("Save") }
            },
            dismissButton = {
                Row {
                    TextButton(onClick = { askClose = false; model.close(CloseChoice.Discard, onClosed) }) { Text("Discard") }
                    TextButton(onClick = { askClose = false; model.close(CloseChoice.Keep, onClosed) }) { Text("Keep") }
                }
            },
        )
    }
}

@Composable
private fun Placeholder(text: String) {
    Box(Modifier.fillMaxSize().padding(24.dp), contentAlignment = Alignment.Center) {
        Text(text, style = MaterialTheme.typography.bodyMedium)
    }
}

/** The grid: as many columns as fit (at least 3 on a phone), each Card's thumbnail and name */
@Composable
private fun CardsGrid(model: SetViewModel, state: SetState, grid: LazyGridState, onOpenCard: (Int) -> Unit) {
    if (state.session == 0L) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { CircularProgressIndicator() }
        return
    }
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val minCell = 110.dp
        val spacing = 8.dp
        val columns = maxOf(1, ((maxWidth - spacing) / (minCell + spacing)).toInt())
        val cellWidthPx = with(LocalDensity.current) { ((maxWidth - spacing * (columns + 1)) / columns).roundToPx() }
        // tell the render queue what is on screen, and what comes next; it drops the rest
        LaunchedEffect(grid, cellWidthPx, state.cardUids) {
            snapshotFlow {
                val visible = grid.layoutInfo.visibleItemsInfo.map { it.index }
                visible to grid.layoutInfo.totalItemsCount
            }.distinctUntilChanged().collect { (visible, total) ->
                val last = visible.maxOrNull() ?: -1
                val ahead = ((last + 1)..minOf(total - 1, last + 2 * columns)).toList()
                model.wantThumbnails(visible, ahead, cellWidthPx)
            }
        }
        LazyVerticalGrid(
            columns = GridCells.Fixed(columns),
            state = grid,
            contentPadding = androidx.compose.foundation.layout.PaddingValues(spacing),
            horizontalArrangement = Arrangement.spacedBy(spacing),
            verticalArrangement = Arrangement.spacedBy(spacing),
            modifier = Modifier.fillMaxSize().testTag("card-grid"),
        ) {
            itemsIndexed(state.cardUids, key = { _, uid -> uid }) { index, uid ->
                val linked = state.cardLinks.getOrElse(index) { emptyList() }.isNotEmpty()
                Thumbnail(model, RenderKey(uid, cellWidthPx), state.cardNames.getOrElse(index) { "" }, state.aspect, linked) {
                    onOpenCard(index)
                }
            }
        }
    }
}

@Composable
private fun Thumbnail(model: SetViewModel, key: RenderKey, name: String, aspect: Float, linked: Boolean, onClick: () -> Unit) {
    val flow = remember(key) { model.observe(key) }
    val rendered = flow?.collectAsState()?.value
    Column(Modifier.clickable(onClick = onClick).testTag("thumbnail-${key.uid}")) {
        if (rendered != null) {
            Image(
                bitmap = rendered.image.asImageBitmap(),
                contentDescription = name,
                modifier = Modifier.fillMaxWidth().aspectRatio(rendered.image.width.toFloat() / rendered.image.height),
            )
        } else {
            Box(
                Modifier.fillMaxWidth().aspectRatio(aspect).background(MaterialTheme.colorScheme.surfaceVariant),
                contentAlignment = Alignment.Center,
            ) { CircularProgressIndicator(Modifier.padding(24.dp)) }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            // Linked cards get a small badge
            if (linked) Text("🔗 ", style = MaterialTheme.typography.bodySmall, modifier = Modifier.testTag("linked-${key.uid}"))
            Text(name, style = MaterialTheme.typography.bodySmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
    }
}
