// Mobile Set Editor - share or save Cards as PNGs, as the desktop exports them (#74)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import java.io.File
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mobileseteditor.core.Core

/** File names for exported Cards: from their names, safe for file systems, each used once */
object ExportNames {
    private val unsafe = Regex("[\\\\/:*?\"<>|\\p{Cntrl}]")

    /** "name.png", or "name (2).png" if [used] has it already; "card-<position>" for a Card with no name */
    fun next(cardName: String, position: Int, used: MutableSet<String>): String {
        val base = unsafe.replace(cardName, "_").trim().trim('.').take(100).ifEmpty { "card-$position" }
        var name = "$base.png"
        var n = 2
        while (!used.add(name.lowercase())) name = "$base (${n++}).png"
        return name
    }
}

/** What the user chose to export */
data class ExportSettings(val zoom: Double, val bleed: Boolean)

/**
 * Exporting Cards: [start] asks how (size, bleed), then shares them or saves them. Put
 * [CardExportUi] in the screen that calls [start]. Each Card is one core call, so edits and the
 * visible Card's render get their turn between them.
 */
class CardExport internal constructor(private val model: SetViewModel, private val context: Context, private val scope: CoroutineScope) {
    internal var asking by mutableStateOf<List<String>?>(null)
    internal var progress by mutableStateOf<Pair<Int, Int>?>(null)
    internal var job: Job? = null
    internal var pendingSave: Pair<List<String>, ExportSettings>? = null

    /** Export these Cards (uids): ask how first */
    fun start(uids: List<String>) {
        if (uids.isNotEmpty()) asking = uids
    }

    private val prefs get() = context.getSharedPreferences("export", Context.MODE_PRIVATE)
    internal var lastZoom: Double
        get() = prefs.getFloat("zoom", 1f).toDouble()
        set(v) = prefs.edit().putFloat("zoom", v.toFloat()).apply()

    private fun nameOf(uid: String): Pair<String, Int> {
        val s = model.state.value
        val i = s.cardUids.indexOf(uid)
        return s.cardNames.getOrElse(i) { "" } to (i + 1)
    }

    /** The PNG of one Card, or null (the core's messages are shown with the Set's) */
    private suspend fun png(uid: String, settings: ExportSettings): ByteArray? {
        val session = model.state.value.session.takeIf { it != 0L } ?: return null
        val r = Core.exportCardImage(session, uid, settings.zoom, settings.bleed)
        model.report(r.result)
        return r.data.takeIf { r.result.ok && it.isNotEmpty() }
    }

    /** Export each Card in turn, with progress; [write] puts one PNG where it goes */
    private fun run(uids: List<String>, settings: ExportSettings, done: suspend () -> Unit = {}, write: suspend (uid: String, name: String, png: ByteArray) -> Unit) {
        job = scope.launch {
            val used = mutableSetOf<String>()
            try {
                for ((i, uid) in uids.withIndex()) {
                    progress = i to uids.size
                    val data = png(uid, settings) ?: continue
                    ensureActive()
                    val (name, position) = nameOf(uid)
                    write(uid, ExportNames.next(name, position, used), data)
                }
                progress = null
                done()
            } finally {
                progress = null
            }
        }
    }

    /** Share: the PNGs go into a cache directory a FileProvider serves, then to the share sheet */
    internal fun share(uids: List<String>, settings: ExportSettings) {
        val dir = exportDir(context)
        val files = mutableListOf<File>()
        run(uids, settings, done = {
            if (files.isEmpty()) return@run
            val authority = "${context.packageName}.exports"
            val uris = ArrayList(files.map { FileProvider.getUriForFile(context, authority, it) })
            val send = if (uris.size == 1) {
                Intent(Intent.ACTION_SEND).putExtra(Intent.EXTRA_STREAM, uris[0])
            } else {
                Intent(Intent.ACTION_SEND_MULTIPLE).putParcelableArrayListExtra(Intent.EXTRA_STREAM, uris)
            }.setType("image/png").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            send.clipData = ClipData.newUri(context.contentResolver, "Cards", uris[0]).apply { uris.drop(1).forEach { addItem(ClipData.Item(it)) } }
            context.startActivity(Intent.createChooser(send, "Share ${uris.size} Card${if (uris.size == 1) "" else "s"}").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        }) { _, name, png ->
            if (files.isEmpty()) withContext(Dispatchers.IO) { dir.deleteRecursively(); dir.mkdirs() } // (the last export's files go now)
            val file = File(dir, name)
            withContext(Dispatchers.IO) { file.writeBytes(png) }
            files += file
        }
    }

    /** Save one Card to a document the user created */
    internal fun saveTo(uri: Uri, uid: String, settings: ExportSettings) = run(listOf(uid), settings) { _, _, png ->
        withContext(Dispatchers.IO) { context.contentResolver.openOutputStream(uri, "wt")?.use { it.write(png) } }
    }

    /** Save Cards into a folder (tree) the user picked */
    internal fun saveInto(tree: Uri, uids: List<String>, settings: ExportSettings) {
        val folder = DocumentsContract.buildDocumentUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree))
        run(uids, settings) { _, name, png ->
            withContext(Dispatchers.IO) {
                val doc = DocumentsContract.createDocument(context.contentResolver, folder, "image/png", name) ?: return@withContext
                context.contentResolver.openOutputStream(doc, "wt")?.use { it.write(png) }
            }
        }
    }

    /** The suggested file name for one Card */
    internal fun suggestedName(uid: String): String = nameOf(uid).let { (name, position) -> ExportNames.next(name, position, mutableSetOf()) }

    companion object {
        /** Where shared PNGs wait for the app they're shared with (res/xml/export_paths.xml) */
        fun exportDir(context: Context) = File(context.cacheDir, "exports")
    }
}

@Composable
fun rememberCardExport(model: SetViewModel): CardExport {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    return remember(model) { CardExport(model, context, scope) }
}

/** The dialogs of an export: how (size, bleed; share or save), then its progress */
@Composable
fun CardExportUi(export: CardExport) {
    val createOne = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("image/png")) { uri ->
        val (uids, settings) = export.pendingSave ?: return@rememberLauncherForActivityResult
        export.pendingSave = null
        if (uri != null) export.saveTo(uri, uids.single(), settings)
    }
    val pickFolder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { tree ->
        val (uids, settings) = export.pendingSave ?: return@rememberLauncherForActivityResult
        export.pendingSave = null
        if (tree != null) export.saveInto(tree, uids, settings)
    }
    export.asking?.let { uids ->
        var zoom by remember { mutableStateOf(export.lastZoom) }
        var bleed by remember { mutableStateOf(false) }
        val settings = ExportSettings(zoom, bleed)
        val count = "${uids.size} Card${if (uids.size == 1) "" else "s"}"
        AlertDialog(
            onDismissRequest = { export.asking = null },
            title = { Text("Export $count as PNG") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    Text("Size", style = MaterialTheme.typography.labelLarge)
                    for ((z, label) in listOf(1.0 to "1× (the Style's own size)", 2.0 to "2×")) {
                        Row(
                            Modifier.fillMaxWidth().clickable { zoom = z }.testTag("export-zoom-${z.toInt()}"),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            RadioButton(selected = zoom == z, onClick = { zoom = z })
                            Text(label)
                        }
                    }
                    Row(Modifier.fillMaxWidth().clickable { bleed = !bleed }, verticalAlignment = Alignment.CenterVertically) {
                        Text("Bleed edge (for printing)", modifier = Modifier.weight(1f))
                        Switch(checked = bleed, onCheckedChange = { bleed = it }, modifier = Modifier.testTag("export-bleed"))
                    }
                }
            },
            confirmButton = {
                Row {
                    TextButton(onClick = {
                        export.asking = null
                        export.lastZoom = zoom
                        export.share(uids, settings)
                    }, modifier = Modifier.testTag("export-share")) { Text("Share") }
                    TextButton(onClick = {
                        export.asking = null
                        export.lastZoom = zoom
                        export.pendingSave = uids to settings
                        if (uids.size == 1) createOne.launch(export.suggestedName(uids[0])) else pickFolder.launch(null)
                    }, modifier = Modifier.testTag("export-save")) { Text("Save to…") }
                }
            },
            dismissButton = { TextButton(onClick = { export.asking = null }) { Text("Cancel") } },
        )
    }
    // progress, for more than a few Cards
    export.progress?.let { (done, total) ->
        if (total > 3) {
            AlertDialog(
                onDismissRequest = {},
                title = { Text("Exporting") },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("$done of $total Cards")
                        LinearProgressIndicator(progress = { done.toFloat() / total }, modifier = Modifier.fillMaxWidth())
                    }
                },
                confirmButton = { TextButton(onClick = { export.job?.cancel() }) { Text("Cancel") } },
            )
        }
    }
}
