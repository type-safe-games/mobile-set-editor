// Mobile Set Editor - New Set: pick a Game (if there is a choice), then a Style (#66)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.LruCache
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawingPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.Stores
import org.mobileseteditor.core.StyleSupport
import org.mobileseteditor.core.Support
import org.mobileseteditor.core.TemplateInfo

/**
 * New Set: the installed Games (skipped when there is one), then the chosen Game's Styles. With
 * no Game installed it says where Templates come from (Template import, ADR 0005).
 */
@Composable
fun NewSetScreen(onCreate: (game: String, style: String) -> Unit, onTemplates: () -> Unit, onBack: () -> Unit) {
    val context = LocalContext.current
    val templates by produceState<List<TemplateInfo>?>(null) {
        value = if (Stores.startCore(context).ok) Core.listTemplates().templates.toList() else emptyList()
    }
    var game by remember { mutableStateOf<TemplateInfo?>(null) }
    val all = templates
    val games = all.orEmpty().filter { it.type == "game" }
    LaunchedEffect(all) { if (games.size == 1) game = games.single() }
    BackHandler { if (game != null && games.size > 1) game = null else onBack() }

    Column(Modifier.fillMaxSize().safeDrawingPadding().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            OutlinedButton(onClick = onBack) { Text("‹ Back") }
            Spacer(Modifier.width(12.dp))
            Text(game?.let { "New ${it.shortName} Set: pick a Style" } ?: "New Set", style = MaterialTheme.typography.titleLarge)
        }
        val chosen = game
        when {
            all == null -> Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { CircularProgressIndicator() }
            games.isEmpty() -> Column(verticalArrangement = Arrangement.spacedBy(12.dp), modifier = Modifier.testTag("no-games")) {
                Text("No Templates are installed yet. Templates come from Template import: pick a zip of them, such as the Full Magic Pack.")
                Button(onClick = onTemplates) { Text("Import Templates") }
            }
            chosen == null -> LazyColumn(Modifier.fillMaxSize().testTag("games")) {
                items(games, key = { it.name }) { g ->
                    TemplateRow(g, Modifier.clickable { game = g }.testTag("game-${g.name}"))
                }
            }
            else -> StylePicker(
                styles = all.filter { it.type == "style" && it.game == chosen.name },
                onPick = { onCreate(chosen.name, it.name) },
            )
        }
    }
}

/**
 * The installed Styles of a Game, searchable by name, each with its icon (#66). Reused wherever a
 * Style is chosen: the Card details sheet, the Set info tab.
 */
@Composable
fun StylePicker(
    styles: List<TemplateInfo>,
    onPick: (TemplateInfo) -> Unit,
    modifier: Modifier = Modifier,
    support: StyleSupport = StyleSupport.get(LocalContext.current),
) {
    var query by remember { mutableStateOf("") }
    val sorted = remember(styles) { styles.sortedBy { it.fullName.lowercase() } }
    val shown = remember(sorted, query) {
        val q = query.trim().lowercase()
        if (q.isEmpty()) sorted else sorted.filter { q in it.fullName.lowercase() || q in it.shortName.lowercase() || q in it.name.lowercase() }
    }
    Column(modifier, verticalArrangement = Arrangement.spacedBy(8.dp)) {
        OutlinedTextField(
            query, { query = it },
            label = { Text("Search ${styles.size} Styles") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth().testTag("style-search"),
        )
        // a new search starts at the top
        val list = rememberLazyListState()
        LaunchedEffect(query) { list.scrollToItem(0) }
        LazyColumn(Modifier.fillMaxSize().testTag("styles"), state = list) {
            items(shown, key = { it.name }) { style ->
                TemplateRow(style, Modifier.clickable { onPick(style) }.testTag("style-${style.name}"), support.of(style))
            }
        }
    }
}

/** A Template as pickers list it: its icon, full name and package name */
@Composable
private fun TemplateRow(t: TemplateInfo, modifier: Modifier, support: Support? = null) {
    Row(modifier.fillMaxWidth().padding(vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
        TemplateIcon(t.iconPath)
        Spacer(Modifier.width(12.dp))
        Column(Modifier.weight(1f)) {
            Text(t.fullName, style = MaterialTheme.typography.bodyLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(t.name, style = MaterialTheme.typography.bodySmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
            // Unsupported styles (ADR 0010): still usable, but flagged
            when (support) {
                Support.Unsupported -> Text(
                    "Unsupported in this version: Cards may not look as they do in the desktop editor",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.error,
                    modifier = Modifier.testTag("unsupported-${t.name}"),
                )
                Support.Untested -> Text(
                    "Untested in this version",
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.testTag("untested-${t.name}"),
                )
                else -> {}
            }
        }
    }
}

/** Decoded icons, small: a picker scrolls through hundreds */
private val icons = object : LruCache<String, Bitmap>(8 * 1024 * 1024) {
    override fun sizeOf(key: String, value: Bitmap) = value.byteCount
}

@Composable
private fun TemplateIcon(path: String) {
    val px = with(LocalDensity.current) { 40.dp.roundToPx() }
    val icon by produceState(if (path.isEmpty()) null else icons.get(path), path) {
        if (path.isEmpty() || value != null) return@produceState
        value = withContext(Dispatchers.IO) { decodeIcon(path, px) }?.also { icons.put(path, it) }
    }
    val bitmap = icon
    if (bitmap != null) {
        Image(bitmap.asImageBitmap(), contentDescription = null, modifier = Modifier.size(40.dp))
    } else {
        Box(Modifier.size(40.dp).background(MaterialTheme.colorScheme.surfaceVariant))
    }
}

/** An icon file, scaled down to about [px] */
private fun decodeIcon(path: String, px: Int): Bitmap? = try {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeFile(path, bounds)
    var sample = 1
    while (maxOf(bounds.outWidth, bounds.outHeight) / (sample * 2) >= px) sample *= 2
    BitmapFactory.decodeFile(path, BitmapFactory.Options().apply { inSampleSize = sample })
} catch (e: Exception) {
    null
}
