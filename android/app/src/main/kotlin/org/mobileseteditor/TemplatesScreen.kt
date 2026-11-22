// Mobile Set Editor - the Templates screen: the installed Templates, and importing more from a zip (ADR 0005, #71)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.text.format.Formatter
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.Checkbox
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TriStateCheckbox
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.state.ToggleableState
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import org.mobileseteditor.core.ImportChoice
import org.mobileseteditor.core.Outcome
import org.mobileseteditor.core.ScannedPackage
import org.mobileseteditor.core.TemplateInfo
import org.mobileseteditor.core.Verdict

/** The installed Templates by type, and "Import Templates" from a zip with a picker (ADR 0005) */
@Composable
fun TemplatesScreen(
    model: TemplatesViewModel = viewModel(),
    onOpenSet: () -> Unit = {},
    onShowSet: () -> Unit = {},
    hasSet: Boolean = false,
) {
    val installed by model.installed.collectAsState()
    val state by model.import.collectAsState()
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) model.picked(uri)
    }
    Column(Modifier.fillMaxSize().safeDrawingPadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("Templates", style = MaterialTheme.typography.headlineSmall)
        val scanned = state as? ImportState.Scanned
        if (scanned != null) {
            // the picker takes the screen until the user installs or cancels
            ImportPicker(scanned.choice, onChange = model::choose, onConfirm = model::confirm, onCancel = model::cancel)
            return@Column
        }
        Button(
            onClick = { pick.launch(arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")) },
            enabled = state !is ImportState.Installing && state !is ImportState.Scanning,
            modifier = Modifier.testTag("import"),
        ) { Text("Import Templates") }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onOpenSet, modifier = Modifier.testTag("open-set")) { Text("Open Set") }
            if (hasSet) OutlinedButton(onClick = onShowSet) { Text("Back to the Set") }
        }
        ImportPanel(state, onCancel = model::cancel, onDismiss = model::dismiss)
        HorizontalDivider()
        Text("${installed.size} installed", style = MaterialTheme.typography.titleSmall, modifier = Modifier.testTag("installed-count"))
        // by type: Games first, then Styles, symbol fonts, ...
        val byType = installed.groupBy { it.type }.toSortedMap(compareBy({ typeOrder(it) }, { it }))
        LazyColumn(Modifier.fillMaxWidth().testTag("installed")) {
            for ((type, list) in byType) {
                item(key = "type-$type") {
                    Text(
                        "${typeTitle(type)} (${list.size})",
                        style = MaterialTheme.typography.titleMedium,
                        modifier = Modifier.padding(top = 12.dp, bottom = 4.dp).semantics { heading() },
                    )
                }
                items(list, key = { it.name }) { TemplateRow(it) }
            }
        }
    }
}

private fun typeOrder(type: String) = listOf("game", "style", "symbol-font", "include", "export-template", "locale").indexOf(type).let { if (it < 0) 99 else it }

private fun typeTitle(type: String) = when (type) {
    "game" -> "Games"
    "style" -> "Styles"
    "symbol-font" -> "Symbol fonts"
    "include" -> "Includes"
    "export-template" -> "Export templates"
    "locale" -> "Locales"
    else -> type.replaceFirstChar { it.uppercase() }
}

/**
 * What to install from a scanned zip: the packages by installer group (collapsible; a group's box
 * picks or clears all of it), each with its version and what installing it does; the loose fonts
 * and dictionaries as one entry; the size of the choice, live.
 */
@Composable
private fun ImportPicker(choice: ImportChoice, onChange: ((ImportChoice) -> ImportChoice) -> Unit, onConfirm: () -> Unit, onCancel: () -> Unit) {
    val context = LocalContext.current
    fun size(bytes: Long) = Formatter.formatShortFileSize(context, bytes)
    val scan = choice.scan
    var query by remember { mutableStateOf("") }
    var open by remember { mutableStateOf(setOf<String>()) }
    val q = query.trim().lowercase()
    val shown = scan.packages.filter { q.isEmpty() || q in it.fullName.lowercase() || q in it.name.lowercase() || q in it.installerGroup.lowercase() }
    val groups = shown.groupBy { it.installerGroup }.toSortedMap()
    Column(Modifier.fillMaxSize().testTag("scan"), verticalArrangement = Arrangement.spacedBy(8.dp)) {
        val counts = scan.packages.groupingBy { it.verdict }.eachCount()
        Text(
            "${counts[Verdict.Install] ?: 0} new, ${counts[Verdict.Replace] ?: 0} newer, " +
                "${counts[Verdict.SkipNotNewer] ?: 0} already installed, ${counts[Verdict.Refuse] ?: 0} can't be installed",
            style = MaterialTheme.typography.bodyMedium,
        )
        Text(
            "${choice.packages.size} chosen · ${size(choice.size)} when installed",
            style = MaterialTheme.typography.titleSmall,
            modifier = Modifier.testTag("install-size"),
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onConfirm, enabled = !choice.isEmpty, modifier = Modifier.testTag("confirm")) { Text("Install") }
            OutlinedButton(onClick = onCancel) { Text("Cancel") }
            TextButton(onClick = { onChange { ImportChoice.everything(scan) } }, modifier = Modifier.testTag("choose-all")) { Text("All new") }
            TextButton(onClick = { onChange { ImportChoice.nothing(scan) } }, modifier = Modifier.testTag("choose-none")) { Text("None") }
        }
        OutlinedTextField(
            query, { query = it },
            label = { Text("Search Templates and groups") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth().testTag("import-search"),
        )
        LazyColumn(Modifier.fillMaxWidth().weight(1f).testTag("import-groups")) {
            if (scan.fontCount > 0 || scan.dictionaryCount > 0) item(key = "fonts") {
                Row(
                    Modifier.fillMaxWidth().clickable { onChange { it.copy(fonts = !it.fonts) } }.testTag("import-fonts"),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Checkbox(checked = choice.fonts, onCheckedChange = { on -> onChange { it.copy(fonts = on) } })
                    Text(
                        "Loose fonts: ${scan.newFontCount} new of ${scan.fontCount} (${size(scan.newFontsSize)}); " +
                            "dictionaries: ${scan.newDictionaryCount} new",
                        style = MaterialTheme.typography.bodyMedium,
                    )
                }
            }
            // the installer groups as a tree: "magic", "magic/M15 Frames", "magic/M15 Frames/Standard", ...
            val all = groups.keys.flatMap { g -> g.split('/').indices.map { g.split('/').take(it + 1).joinToString("/") } }.toSortedSet()
            for (group in all) {
                val parent = group.substringBeforeLast('/', "")
                // shown when every group above it is open (or while searching)
                val ancestors = generateSequence(parent.takeIf { it.isNotEmpty() }) { it.substringBeforeLast('/', "").takeIf { p -> p.isNotEmpty() } }.toList()
                if (q.isEmpty() && ancestors.any { it !in open }) continue
                val under = shown.filter { ImportChoice.inGroup(it.installerGroup, group) }
                val installable = under.filter { it.installable }
                val picked = installable.count { it.name in choice.packages }
                val expanded = q.isNotEmpty() || group in open
                val depth = ancestors.size
                item(key = "group-$group") {
                    Row(
                        Modifier.fillMaxWidth().clickable { open = if (group in open) open - group else open + group }
                            .padding(start = (16 * depth).dp, top = 2.dp, bottom = 2.dp).testTag("group-$group"),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        TriStateCheckbox(
                            state = when {
                                picked == 0 -> ToggleableState.Off
                                picked == installable.size -> ToggleableState.On
                                else -> ToggleableState.Indeterminate
                            },
                            onClick = { onChange { if (picked == installable.size) it.clearGroup(group) else it.selectGroup(group) } },
                            enabled = installable.isNotEmpty(),
                            modifier = Modifier.testTag("group-box-$group"),
                        )
                        Text(
                            (if (expanded) "▾ " else "▸ ") + group.substringAfterLast('/').ifEmpty { "(no group)" },
                            style = MaterialTheme.typography.titleSmall,
                            modifier = Modifier.weight(1f),
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                        Text("$picked / ${under.size}", style = MaterialTheme.typography.labelMedium)
                    }
                }
                if (expanded) items(groups[group].orEmpty(), key = { it.name }) { p -> PackageRow(p, choice, onChange, depth) }
            }
        }
    }
}

@Composable
private fun PackageRow(p: ScannedPackage, choice: ImportChoice, onChange: ((ImportChoice) -> ImportChoice) -> Unit, depth: Int) {
    val context = LocalContext.current
    val picked = p.name in choice.packages
    val neededBy = if (picked) choice.neededBy(p.name) else emptyList()
    Row(
        Modifier.fillMaxWidth().clickable(enabled = p.installable) { onChange { it.toggle(p.name) } }
            .padding(start = (16 * depth + 24).dp).testTag("package-${p.name}"),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Checkbox(checked = picked, onCheckedChange = { onChange { it.toggle(p.name) } }, enabled = p.installable)
        Column(Modifier.weight(1f)) {
            Text(p.fullName, style = MaterialTheme.typography.bodyMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(
                listOf(p.name, p.version.ifEmpty { null }, Formatter.formatShortFileSize(context, p.size)).filterNotNull().joinToString(" · "),
                style = MaterialTheme.typography.bodySmall,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            Text(
                when (p.verdict) {
                    Verdict.Install -> "new"
                    Verdict.Replace -> "replaces ${p.installedVersion}"
                    Verdict.SkipNotNewer -> "already installed (${p.installedVersion.ifEmpty { "same" }})"
                    Verdict.Refuse -> "can't be installed: ${p.reason}"
                } + if (neededBy.isNotEmpty()) " · needed by ${neededBy.size} chosen" else "",
                style = MaterialTheme.typography.labelSmall,
                color = if (p.verdict == Verdict.Refuse) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun ImportPanel(state: ImportState, onCancel: () -> Unit, onDismiss: () -> Unit) {
    val context = LocalContext.current
    fun size(bytes: Long) = Formatter.formatShortFileSize(context, bytes)
    when (state) {
        ImportState.Idle, is ImportState.Scanned -> {}
        ImportState.Scanning -> Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp)) {
                Text("Reading the zip…")
                LinearProgressIndicator(Modifier.fillMaxWidth())
            }
        }
        is ImportState.Installing -> Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("Installing: ${size(state.done)} of ${size(state.total)}")
                LinearProgressIndicator(
                    progress = { if (state.total > 0) state.done.toFloat() / state.total else 0f },
                    modifier = Modifier.fillMaxWidth(),
                )
                OutlinedButton(onClick = onCancel) { Text("Cancel") }
            }
        }
        is ImportState.Finished -> Card(Modifier.fillMaxWidth().testTag("finished")) {
            val packages = state.result.packages
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(
                    "Installed ${packages.count { it.outcome == Outcome.Installed || it.outcome == Outcome.Replaced }} " +
                        "skipped ${packages.count { it.outcome == Outcome.SkippedNotNewer }}, " +
                        "failed ${packages.count { it.outcome == Outcome.Failed || it.outcome == Outcome.Refused }}; " +
                        "${state.result.fontCount} fonts",
                    modifier = Modifier.testTag("import-summary"),
                )
                packages.filter { it.error.isNotEmpty() }.take(5).forEach { Text("${it.name}: ${it.error}") }
                OutlinedButton(onClick = onDismiss) { Text("OK") }
            }
        }
        ImportState.Cancelled -> Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp)) {
                Text("Import cancelled. Templates installed before that are kept.")
                OutlinedButton(onClick = onDismiss) { Text("OK") }
            }
        }
        is ImportState.Failed -> Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp)) {
                Text("Import failed: ${state.message}")
                OutlinedButton(onClick = onDismiss) { Text("OK") }
            }
        }
    }
}

@Composable
private fun TemplateRow(t: TemplateInfo) {
    Column(Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Text(t.fullName.ifEmpty { t.name }, style = MaterialTheme.typography.bodyLarge)
        Text(
            "${t.name}${if (t.version.isNotEmpty()) " · ${t.version}" else ""}",
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.padding(2.dp).width(0.dp))
    }
}
