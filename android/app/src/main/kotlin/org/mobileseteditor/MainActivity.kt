// Mobile Set Editor - the app's entry point
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.os.Bundle
import android.text.format.Formatter
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import org.mobileseteditor.core.Outcome
import org.mobileseteditor.core.TemplateInfo
import org.mobileseteditor.core.Verdict

/** For now two screens: the Templates, and an open Set's Cards; the real UI comes with M4 */
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(Modifier.fillMaxSize()) {
                    App()
                }
            }
        }
    }
}

@Composable
fun App() {
    var showSet by rememberSaveable { mutableStateOf(false) }
    val setModel: SetViewModel = viewModel()
    val openSet = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            setModel.open(uri)
            showSet = true
        }
    }
    // (.mse-set files have no registered type)
    val pickSet = { openSet.launch(arrayOf("*/*")) }
    if (showSet) {
        BackHandler { showSet = false }
        SetScreen(setModel, onOpen = pickSet, onBack = { showSet = false })
    } else {
        TemplatesScreen(onOpenSet = pickSet, onShowSet = { showSet = true }, hasSet = setModel.state.collectAsState().value.session != 0L)
    }
}

/** The installed Templates, and "Import Templates" from a zip (ADR 0005) */
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
        Button(
            onClick = { pick.launch(arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")) },
            enabled = state !is ImportState.Installing && state !is ImportState.Scanning,
            modifier = Modifier.testTag("import"),
        ) { Text("Import Templates") }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Button(onClick = onOpenSet, modifier = Modifier.testTag("open-set")) { Text("Open Set") }
            if (hasSet) OutlinedButton(onClick = onShowSet) { Text("Back to the Set") }
        }
        ImportPanel(state, onConfirm = model::confirm, onCancel = model::cancel, onDismiss = model::dismiss)
        HorizontalDivider()
        Text("${installed.size} installed", style = MaterialTheme.typography.titleSmall, modifier = Modifier.testTag("installed-count"))
        LazyColumn(Modifier.fillMaxWidth()) {
            items(installed, key = { it.name }) { TemplateRow(it) }
        }
    }
}

@Composable
private fun ImportPanel(state: ImportState, onConfirm: () -> Unit, onCancel: () -> Unit, onDismiss: () -> Unit) {
    val context = LocalContext.current
    fun size(bytes: Long) = Formatter.formatShortFileSize(context, bytes)
    when (state) {
        ImportState.Idle -> {}
        ImportState.Scanning -> Card(Modifier.fillMaxWidth()) {
            Column(Modifier.padding(16.dp)) {
                Text("Reading the zip…")
                LinearProgressIndicator(Modifier.fillMaxWidth())
            }
        }
        is ImportState.Scanned -> Card(Modifier.fillMaxWidth().testTag("scan")) {
            val scan = state.scan
            val install = scan.packages.count { it.verdict == Verdict.Install }
            val replace = scan.packages.count { it.verdict == Verdict.Replace }
            val skip = scan.packages.count { it.verdict == Verdict.SkipNotNewer }
            val refuse = scan.packages.count { it.verdict == Verdict.Refuse }
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text("${install + replace} Templates to install ($install new, $replace newer), $skip already installed, $refuse can't be installed")
                Text("${scan.newFontCount} new loose fonts (of ${scan.fontCount}), ${scan.newDictionaryCount} new dictionary files")
                Text("Size when installed: ${size(scan.installSize)}", modifier = Modifier.testTag("install-size"))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = onConfirm, enabled = scan.anythingNew, modifier = Modifier.testTag("confirm")) {
                        Text("Install")
                    }
                    OutlinedButton(onClick = onCancel) { Text("Cancel") }
                }
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
                    "Installed ${packages.count { it.outcome == Outcome.Installed || it.outcome == Outcome.Replaced }}, " +
                        "skipped ${packages.count { it.outcome == Outcome.SkippedNotNewer }}, " +
                        "failed ${packages.count { it.outcome == Outcome.Failed || it.outcome == Outcome.Refused }}; " +
                        "${state.result.fontCount} fonts",
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
        Text("${t.name} · ${t.type}${if (t.version.isNotEmpty()) " · ${t.version}" else ""}",
            style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.padding(2.dp))
    }
}
