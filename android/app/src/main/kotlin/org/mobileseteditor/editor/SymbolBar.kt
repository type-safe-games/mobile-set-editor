// Mobile Set Editor - the text sheet's symbol bar: a field's insert-symbol menu, with previews (#64)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.editor

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.VerticalDivider
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.SymbolMenuItem
import org.mobileseteditor.core.SymbolMenuResult
import org.mobileseteditor.core.TargetKind

/**
 * Under a text field with a symbol font: its insert-symbol menu as a row (submenus drop down,
 * "custom" items ask for a code), each with its symbol drawn, and a "Symbols" toggle that types in
 * the symbol font (not offered when the whole field is symbols anyway, e.g. a mana cost).
 */
@Composable
fun SymbolBar(
    menu: SymbolMenuResult,
    session: Long,
    uid: String,
    field: String,
    symbolMode: Boolean,
    onSymbolMode: (Boolean) -> Unit,
    onInsert: (String) -> Unit,
) {
    var asking by remember { mutableStateOf<SymbolMenuItem?>(null) }
    val pick: (SymbolMenuItem) -> Unit = { item ->
        when (item.type) {
            SymbolMenuItem.Type.Code -> onInsert(item.code)
            SymbolMenuItem.Type.Custom -> asking = item
            else -> {}
        }
    }
    Row(Modifier.fillMaxWidth().testTag("symbol-bar"), verticalAlignment = Alignment.CenterVertically) {
        if (!menu.alwaysSymbol) {
            FilterChip(
                selected = symbolMode,
                onClick = { onSymbolMode(!symbolMode) },
                label = { Text("Symbols") },
                modifier = Modifier.padding(end = 4.dp).testTag("symbol-mode"),
            )
        }
        LazyRow(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(2.dp), verticalAlignment = Alignment.CenterVertically) {
            items(menu.items) { item ->
                when (item.type) {
                    SymbolMenuItem.Type.Line -> VerticalDivider(Modifier.height(28.dp).padding(horizontal = 2.dp))
                    SymbolMenuItem.Type.Submenu -> Submenu(item, session, uid, field, pick)
                    else -> TextButton(onClick = { pick(item) }, modifier = Modifier.testTag("symbol-${item.label}")) {
                        SymbolLabel(item, session, uid, field)
                    }
                }
            }
        }
    }
    asking?.let { item ->
        var code by remember(item) { mutableStateOf("") }
        AlertDialog(
            onDismissRequest = { asking = null },
            title = { Text(item.label) },
            text = { OutlinedTextField(code, { code = it }, label = { Text(item.prompt) }, singleLine = true, modifier = Modifier.testTag("symbol-custom")) },
            confirmButton = {
                TextButton(onClick = { asking = null; if (code.isNotEmpty()) onInsert(code) }, modifier = Modifier.testTag("symbol-custom-insert")) { Text("Insert") }
            },
            dismissButton = { TextButton(onClick = { asking = null }) { Text("Cancel") } },
        )
    }
}

@Composable
private fun Submenu(item: SymbolMenuItem, session: Long, uid: String, field: String, pick: (SymbolMenuItem) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box {
        TextButton(onClick = { open = true }, modifier = Modifier.testTag("symbol-menu-${item.label}")) { Text("${item.label} ▾") }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            for (sub in item.items) {
                when (sub.type) {
                    SymbolMenuItem.Type.Line -> HorizontalDivider()
                    // (deeper submenus: their items, flattened, are rare enough)
                    SymbolMenuItem.Type.Submenu -> for (deeper in sub.items.filter { it.type != SymbolMenuItem.Type.Line && it.type != SymbolMenuItem.Type.Submenu }) {
                        DropdownMenuItem(
                            text = { Text("${sub.label}: ${deeper.label}") },
                            leadingIcon = { SymbolPreview(deeper, session, uid, field) },
                            onClick = { open = false; pick(deeper) },
                        )
                    }
                    else -> DropdownMenuItem(
                        text = { Text(sub.label) },
                        leadingIcon = { SymbolPreview(sub, session, uid, field) },
                        onClick = { open = false; pick(sub) },
                        modifier = Modifier.testTag("symbol-${sub.label}"),
                    )
                }
            }
        }
    }
}

/** An item as the bar shows it: its symbol, or its label if it has none */
@Composable
private fun SymbolLabel(item: SymbolMenuItem, session: Long, uid: String, field: String) {
    val preview = rememberPreview(item, session, uid, field)
    if (preview != null) {
        Image(preview.asImageBitmap(), contentDescription = item.label, modifier = Modifier.size(28.dp))
    } else {
        Text(item.label, style = MaterialTheme.typography.labelLarge, maxLines = 1)
    }
}

@Composable
private fun SymbolPreview(item: SymbolMenuItem, session: Long, uid: String, field: String) {
    val preview = rememberPreview(item, session, uid, field)
    if (preview != null) Image(preview.asImageBitmap(), contentDescription = null, modifier = Modifier.size(24.dp))
    else Box(Modifier.width(24.dp))
}

@Composable
private fun rememberPreview(item: SymbolMenuItem, session: Long, uid: String, field: String): Bitmap? {
    val px = with(LocalDensity.current) { 28.dp.roundToPx() }
    val state by produceState<Bitmap?>(null, item.id, uid, field, px) {
        if (item.id < 0) return@produceState
        val r = Core.symbolPreview(session, TargetKind.Card, uid, field, item.id, px)
        if (r.result.ok && r.width > 0) value = Bitmap.createBitmap(r.pixels, r.width, r.height, Bitmap.Config.ARGB_8888)
    }
    return state
}
