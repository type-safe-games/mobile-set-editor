// Mobile Set Editor - the Set info tab: set info, the Set symbol, the Set's Style and style options (#68)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
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
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import kotlinx.coroutines.launch
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.FieldInfo
import org.mobileseteditor.core.TargetKind
import org.mobileseteditor.core.TemplateInfo
import org.mobileseteditor.editor.EditTarget
import org.mobileseteditor.editor.displayName
import org.mobileseteditor.editor.summary
import org.mobileseteditor.render.RenderKey

private class InfoRow(val info: FieldInfo, val kind: TargetKind, val summary: String)

/**
 * The Set info tab (#17): the Set's own fields (title, copyright, the Set symbol, ...), its Style
 * and its style options. A field opens the field sheet ([onEdit]); the first Card is shown to see
 * the effect, since every change here re-renders every Card.
 */
@Composable
fun SetInfoTab(model: SetViewModel, state: SetState, revision: Int, onEdit: (EditTarget) -> Unit) {
    val session = state.session
    val scope = rememberCoroutineScope()
    var setFields by remember { mutableStateOf<List<InfoRow>>(emptyList()) }
    var styleFields by remember { mutableStateOf<List<InfoRow>>(emptyList()) }
    var styles by remember { mutableStateOf<List<TemplateInfo>>(emptyList()) }
    var stylesheet by remember { mutableStateOf("") }
    var picking by remember { mutableStateOf(false) }
    // read again after every change (the tab is shown again after the field sheet closes)
    LaunchedEffect(session, revision, state.dirty, state.cardStyles) {
        if (session == 0L) return@LaunchedEffect
        suspend fun rows(kind: TargetKind) = Core.listFields(session, kind, "").fields.map { f ->
            InfoRow(f, kind, summary(Core.getValue(session, kind, "", f.name).value, f.type))
        }
        val info = Core.sessionInfo(session)
        val set = rows(TargetKind.Set)
        val options = rows(TargetKind.SetStyle)
        setFields = set
        styleFields = options
        stylesheet = info.stylesheet
        if (styles.isEmpty()) styles = Core.listTemplates().templates.filter { it.type == "style" && it.game == info.game }
    }
    val current = styles.firstOrNull { it.name == stylesheet }
    BoxWithConstraints(Modifier.fillMaxSize()) {
        val previewDp = maxWidth / 3
        val previewWidth = with(LocalDensity.current) { previewDp.roundToPx() }
        LaunchedEffect(previewWidth, state.cardUids) { model.wantThumbnails(listOf(0), emptyList(), previewWidth) }
        LazyColumn(Modifier.fillMaxSize().padding(horizontal = 16.dp).testTag("set-info")) {
            item {
                Row(Modifier.padding(vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
                    state.cardUids.firstOrNull()?.let { uid ->
                        Preview(model, RenderKey(uid, previewWidth), state.aspect, Modifier.width(previewDp))
                    }
                    Text(
                        "Changes here show on every Card.",
                        style = MaterialTheme.typography.bodySmall,
                        modifier = Modifier.padding(start = 12.dp),
                    )
                }
            }
            rows("Set info", setFields, onEdit)
            item { Header("The Set's Style") }
            item {
                Column(Modifier.padding(vertical = 4.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text(current?.fullName ?: stylesheet, style = MaterialTheme.typography.bodyLarge, modifier = Modifier.testTag("set-style"))
                    OutlinedButton(onClick = { picking = true }, modifier = Modifier.testTag("change-set-style")) { Text("Change Style…") }
                }
            }
            rows("Style options", styleFields, onEdit)
        }
    }
    if (picking) {
        Dialog(onDismissRequest = { picking = false }, properties = DialogProperties(usePlatformDefaultWidth = false)) {
            Surface(Modifier.fillMaxSize()) {
                Column(Modifier.fillMaxSize().safeDrawingPadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        OutlinedButton(onClick = { picking = false }) { Text("‹ Back") }
                        Text("  The Set's Style", style = MaterialTheme.typography.titleLarge)
                    }
                    StylePicker(styles, onPick = { picked ->
                        picking = false
                        scope.launch { model.setSetStyle(picked.name) }
                    })
                }
            }
        }
    }
}

@Composable
private fun Preview(model: SetViewModel, key: RenderKey, aspect: Float, modifier: Modifier) {
    val flow = remember(key) { model.observe(key) }
    val rendered = flow?.collectAsState()?.value
    if (rendered != null) {
        Image(
            rendered.image.asImageBitmap(), contentDescription = "The first Card",
            modifier = modifier.aspectRatio(rendered.image.width.toFloat() / rendered.image.height).testTag("set-info-preview"),
        )
    } else {
        Box(modifier.aspectRatio(aspect).background(MaterialTheme.colorScheme.surfaceVariant))
    }
}

@Composable
private fun Header(text: String) {
    Column(Modifier.padding(top = 16.dp, bottom = 4.dp)) {
        Text(text, style = MaterialTheme.typography.titleMedium, modifier = Modifier.semantics { heading() })
        HorizontalDivider()
    }
}

private fun LazyListScope.rows(title: String, rows: List<InfoRow>, onEdit: (EditTarget) -> Unit) {
    if (rows.isEmpty()) return
    item(key = title) { Header(title) }
    items(rows, key = { "${it.kind}/${it.info.name}" }) { row ->
        val editable = row.info.editable
        Column(
            Modifier.fillMaxWidth()
                .clickable(enabled = editable) { onEdit(EditTarget("", row.info.name, row.kind)) }
                .padding(vertical = 8.dp)
                .testTag("info-${if (row.kind == TargetKind.SetStyle) "style-" else ""}${row.info.name}"),
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
