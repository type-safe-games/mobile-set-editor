// Mobile Set Editor - the Keywords tab: the Set's keywords to add, edit and remove, the Game's to read (#70)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.KeywordInfo
import io.github.type_safe_games.mobileseteditor.core.TargetKind
import io.github.type_safe_games.mobileseteditor.editor.EditTarget

/**
 * The Keywords tab (#17): the Set's keywords, searchable, each opening its fields (name, match
 * with parameters, reminder text, rules, mode) in the field sheet ([onEdit]); Add and Remove.
 * Then the Game's keywords, read-only and collapsed. Changes re-render the Cards that use them.
 */
@Composable
fun KeywordsTab(model: SetViewModel, state: SetState, revision: Int, onEdit: (EditTarget) -> Unit) {
    val session = state.session
    val scope = rememberCoroutineScope()
    var keywords by remember { mutableStateOf<List<KeywordInfo>>(emptyList()) }
    var query by remember { mutableStateOf("") }
    var showGame by remember { mutableStateOf(false) }
    LaunchedEffect(session, revision, state.dirty, state.canUndo, state.canRedo, state.undoName) {
        if (session != 0L) keywords = Core.listKeywords(session).keywords
    }
    val q = query.trim().lowercase()
    fun matches(k: KeywordInfo) = q.isEmpty() || q in k.keyword.lowercase() || q in k.match.lowercase() || q in k.reminder.lowercase()
    val own = keywords.filter { !it.fromGame && matches(it) }
    val game = keywords.filter { it.fromGame && matches(it) }
    Box(Modifier.fillMaxSize()) {
        LazyColumn(Modifier.fillMaxSize().padding(horizontal = 16.dp).testTag("keywords")) {
            item {
                OutlinedTextField(
                    query, { query = it },
                    label = { Text("Search keywords") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp).testTag("keyword-search"),
                )
            }
            item { Header("The Set's keywords (${keywords.count { !it.fromGame }})") }
            if (own.isEmpty()) item {
                Text(
                    if (q.isEmpty()) "None yet: Add keyword makes one." else "None match.",
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.padding(vertical = 8.dp),
                )
            }
            items(own, key = { it.id }) { k ->
                KeywordRow(k, onOpen = { onEdit(EditTarget(k.id.toString(), "keyword", TargetKind.Keyword)) }) {
                    TextButton(onClick = { scope.launch { model.removeKeyword(k.id) } }, modifier = Modifier.testTag("remove-keyword-${k.id}")) {
                        Text("Remove")
                    }
                }
            }
            item {
                Row(
                    Modifier.fillMaxWidth().clickable { showGame = !showGame }.padding(top = 16.dp).testTag("game-keywords"),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Header((if (showGame || q.isNotEmpty()) "▾ " else "▸ ") + "The Game's keywords (${keywords.count { it.fromGame }}, read-only)")
                }
            }
            if (showGame || q.isNotEmpty()) items(game, key = { it.id }) { k -> KeywordRow(k, onOpen = null) {} }
            item { Box(Modifier.padding(bottom = 96.dp)) }
        }
        ExtendedFloatingActionButton(
            onClick = {
                scope.launch {
                    val id = model.addKeyword()
                    if (id != 0L) onEdit(EditTarget(id.toString(), "keyword", TargetKind.Keyword))
                }
            },
            modifier = Modifier.align(Alignment.BottomEnd).padding(16.dp).testTag("add-keyword"),
        ) { Text("Add keyword") }
    }
}

@Composable
private fun Header(text: String) {
    Column(Modifier.padding(top = 8.dp, bottom = 4.dp)) {
        Text(text, style = MaterialTheme.typography.titleMedium, modifier = Modifier.semantics { heading() })
        HorizontalDivider()
    }
}

/** A keyword: its name and match, its reminder text, how many Cards use it, whether it works */
@Composable
private fun KeywordRow(k: KeywordInfo, onOpen: (() -> Unit)?, actions: @Composable () -> Unit) {
    Row(
        Modifier.fillMaxWidth().let { if (onOpen != null) it.clickable(onClick = onOpen) else it }
            .padding(vertical = 6.dp).testTag("keyword-${k.id}"),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(k.match.ifEmpty { k.keyword.ifEmpty { "(new keyword)" } }, style = MaterialTheme.typography.bodyLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (k.reminder.isNotEmpty()) {
                Text(k.reminder, style = MaterialTheme.typography.bodySmall, maxLines = 2, overflow = TextOverflow.Ellipsis)
            }
            val notes = listOfNotNull(
                "used by ${k.usage} Card${if (k.usage == 1) "" else "s"}".takeIf { !k.fromGame || k.usage > 0 },
                k.mode.takeIf { it.isNotEmpty() },
            )
            if (notes.isNotEmpty()) Text(notes.joinToString(" · "), style = MaterialTheme.typography.labelSmall)
            if (!k.valid || k.reminderError.isNotEmpty()) {
                Text(
                    k.reminderError.ifEmpty { "This keyword has an error" },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error,
                )
            }
        }
        actions()
    }
}
