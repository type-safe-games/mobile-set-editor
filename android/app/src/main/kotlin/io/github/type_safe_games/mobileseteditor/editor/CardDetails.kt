// Mobile Set Editor - the Card details sheet: every field of a Card, its Style and style options (#67)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
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
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import kotlinx.coroutines.launch
import io.github.type_safe_games.mobileseteditor.SetViewModel
import io.github.type_safe_games.mobileseteditor.StylePicker
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.FieldInfo
import io.github.type_safe_games.mobileseteditor.core.TargetKind
import io.github.type_safe_games.mobileseteditor.core.TemplateInfo
import io.github.type_safe_games.mobileseteditor.render.RenderKey

/** A field as the sheet lists it, with its value as a line of plain text */
private data class FieldRow(val info: FieldInfo, val kind: TargetKind, val summary: String)

/**
 * Every field of a Card in one list (#17): first those with no spot on the Card to tap (no
 * editable region in its render), then those on it, in the Game's order; then its Style, whether
 * it has its own style options, and those options. Picking a field opens its editor ([onEdit]).
 * Also the way to edit a Card without tapping it (TalkBack, small fields).
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun CardDetailsSheet(model: SetViewModel, uid: String, revision: Int, onEdit: (EditTarget) -> Unit, onDismiss: () -> Unit) {
    val state by model.state.collectAsState()
    val session = state.session
    val index = state.cardUids.indexOf(uid)
    val style = state.cardStyles.getOrNull(index)
    val scope = rememberCoroutineScope()
    var cardFields by remember(uid) { mutableStateOf<List<FieldRow>?>(null) }
    var styleFields by remember(uid) { mutableStateOf<List<FieldRow>>(emptyList()) }
    var styles by remember { mutableStateOf<List<TemplateInfo>>(emptyList()) }
    var picking by remember { mutableStateOf(false) }
    // a magic Card has over a hundred fields: find one by name
    var query by remember { mutableStateOf("") }
    val regions = model.observe(RenderKey(uid, model.renderWidth))?.collectAsState()?.value?.regions
    // read again after every change (to the Card, its Style, its styling)
    var error by remember(uid) { mutableStateOf("") }
    LaunchedEffect(uid, revision, style) {
        suspend fun rows(kind: TargetKind): List<FieldRow> {
            val list = Core.listFields(session, kind, uid)
            if (!list.result.ok) error = list.result.error
            return list.fields.map { f -> FieldRow(f, kind, summary(Core.getValue(session, kind, uid, f.name).value, f.type)) }
        }
        // both at once, so the sheet never shows one list without the other
        val card = rows(TargetKind.Card)
        val options = rows(TargetKind.CardStyle)
        cardFields = card
        styleFields = options
        if (styles.isEmpty()) {
            val game = model.game()
            styles = Core.listTemplates().templates.filter { it.type == "style" && it.game == game }
        }
    }
    // (read here, not in the sheet's content, so that a change recomposes the sheet with it)
    fun matching(rows: List<FieldRow>?) = rows?.filter { query.isBlank() || query.trim().lowercase() in displayName(it.info.name).lowercase() }
    val all = matching(cardFields)
    val options = matching(styleFields).orEmpty()
    val current = styles.firstOrNull { it.name == style?.stylesheet }
    val onCard = regions?.filter { it.editable }?.map { it.field }?.toSet().orEmpty()
    val sheet = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    ModalBottomSheet(onDismissRequest = onDismiss, sheetState = sheet, modifier = Modifier.testTag("card-details")) {
        LazyColumn(Modifier.fillMaxWidth().padding(horizontal = 16.dp).testTag("details-list")) {
            item {
                Text(
                    "Card details: ${state.cardNames.getOrElse(index) { "" }}",
                    style = MaterialTheme.typography.titleLarge,
                    modifier = Modifier.semantics { heading() },
                )
            }
            item {
                OutlinedTextField(
                    query, { query = it },
                    label = { Text("Find a field") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth().testTag("details-find"),
                )
            }
            if (error.isNotEmpty()) item { Text(error, color = MaterialTheme.colorScheme.error) }
            if (all != null) {
                section("Not on the card", all.filter { it.info.name !in onCard }, onEdit, uid)
                section("On the card", all.filter { it.info.name in onCard }, onEdit, uid)
            }
            item { Header("Style") }
            item {
                Column(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
                    Text(
                        (current?.fullName ?: style?.stylesheet.orEmpty()) + if (style?.own == true) "" else " (the Set's)",
                        style = MaterialTheme.typography.bodyLarge,
                        modifier = Modifier.testTag("card-style"),
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedButton(onClick = { picking = true }, modifier = Modifier.testTag("change-style")) { Text("Change Style…") }
                        if (style?.own == true) {
                            TextButton(onClick = { scope.launch { model.setCardStyle(uid, "") } }, modifier = Modifier.testTag("set-style")) {
                                Text("Use the Set's")
                            }
                        }
                    }
                }
            }
            item {
                Row(
                    Modifier.fillMaxWidth().clickable { scope.launch { model.setOwnStyling(uid, style?.ownStyling != true) } }.padding(vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Own style options for this Card", modifier = Modifier.weight(1f))
                    Switch(
                        checked = style?.ownStyling == true,
                        onCheckedChange = { scope.launch { model.setOwnStyling(uid, it) } },
                        modifier = Modifier.testTag("own-styling"),
                    )
                }
            }
            section(
                if (style?.ownStyling == true) "Style options (this Card's own)" else "Style options (the Set's, for every Card in this Style)",
                options, onEdit, uid,
            )
            item { Row(Modifier.padding(bottom = 24.dp)) {} }
        }
    }
    if (picking) {
        Dialog(onDismissRequest = { picking = false }, properties = DialogProperties(usePlatformDefaultWidth = false)) {
            Surface(Modifier.fillMaxSize()) {
                Column(Modifier.fillMaxSize().safeDrawingPadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        OutlinedButton(onClick = { picking = false }) { Text("‹ Back") }
                        Text("  The Card's Style", style = MaterialTheme.typography.titleLarge)
                    }
                    StylePicker(styles, onPick = { picked ->
                        picking = false
                        scope.launch { model.setCardStyle(uid, picked.name) }
                    })
                }
            }
        }
    }
}

@Composable
private fun Header(text: String) {
    Column(Modifier.padding(top = 16.dp, bottom = 4.dp)) {
        Text(text, style = MaterialTheme.typography.titleMedium, modifier = Modifier.semantics { heading() })
        HorizontalDivider()
    }
}

private fun LazyListScope.section(title: String, rows: List<FieldRow>, onEdit: (EditTarget) -> Unit, uid: String) {
    if (rows.isEmpty()) return
    item(key = title) { Header(title) }
    items(rows, key = { "${it.kind}/${it.info.name}" }) { row ->
        val editable = row.info.editable
        Column(
            Modifier.fillMaxWidth()
                .clickable(enabled = editable) { onEdit(EditTarget(uid, row.info.name, row.kind)) }
                .padding(vertical = 8.dp)
                .testTag("details-${if (row.kind == TargetKind.CardStyle) "style-" else ""}${row.info.name}"),
        ) {
            Text(
                displayName(row.info.name).replaceFirstChar { it.uppercase() } + if (editable) "" else " (not editable)",
                style = MaterialTheme.typography.bodyLarge,
            )
            if (row.summary.isNotEmpty()) {
                Text(row.summary, style = MaterialTheme.typography.bodySmall, maxLines = 2, overflow = TextOverflow.Ellipsis)
            }
        }
    }
}

/** A value as one line of plain text (tags taken out) */
fun summary(value: String, type: String): String = when (type) {
    "image" -> if (value.isEmpty()) "(none)" else "an image"
    "symbol" -> if (value.isEmpty()) "(none)" else "a symbol"
    else -> value.replace(Regex("<[^>]*>"), "").replace('\n', ' ').trim()
}
