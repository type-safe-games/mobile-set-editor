// Mobile Set Editor - the Set's home screen: its Cards as a grid of thumbnails, Set info, Keywords
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.content.pm.ApplicationInfo
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Image
import androidx.compose.foundation.border
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.SnackbarDuration
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.SnackbarResult
import androidx.compose.ui.semantics.selected
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
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
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.snapshotFlow
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.TopAppBar
import androidx.compose.ui.res.painterResource
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import io.github.type_safe_games.mobileseteditor.core.StyleSupport
import io.github.type_safe_games.mobileseteditor.core.TemplateInfo
import io.github.type_safe_games.mobileseteditor.core.Support
import io.github.type_safe_games.mobileseteditor.editor.EditTarget
import io.github.type_safe_games.mobileseteditor.editor.FieldSheet
import io.github.type_safe_games.mobileseteditor.render.RenderKey
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.material3.FilterChip
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.ui.graphics.Color
import androidx.compose.foundation.layout.width
import io.github.type_safe_games.mobileseteditor.core.CardTable
import io.github.type_safe_games.mobileseteditor.core.Core
import androidx.compose.material3.HorizontalDivider

private enum class SetTab(val title: String) { Cards("Cards"), SetInfo("Set info"), Keywords("Keywords") }

/**
 * The open Set's home (#17): a top app bar with its name, unsaved state, Undo/Redo, Save and a menu
 * (Save as, Export all, Close; #85), and three tabs. Cards is a grid of thumbnails; tapping one opens that Card. [grid] is kept by the
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
    // a Set info field being edited (#68), and a count of undos and redos (the sheet reads its value again)
    var editing by remember { mutableStateOf<EditTarget?>(null) }
    var revision by remember { mutableIntStateOf(0) }
    // the Cards picked in selection mode (null: not in it), and asking where to move them
    var selected by remember { mutableStateOf<Set<String>?>(null) }
    var moving by remember { mutableStateOf(false) }
    val snackbar = remember { SnackbarHostState() }
    var adding by remember { mutableStateOf(false) }
    val prefs = context.getSharedPreferences("views", android.content.Context.MODE_PRIVATE)
    var asList by remember { mutableStateOf(prefs.getBoolean("cards-as-list", false)) }
    val styles = rememberGameStyles(model)
    val export = rememberCardExport(model)
    val close: () -> Unit = { if (state.dirty) askClose = true else { model.close(CloseChoice.Keep, onClosed) } }
    BackHandler { if (editing != null) editing = null else if (selected != null) selected = null else close() }

    Box(Modifier.fillMaxSize()) {
    Column(Modifier.fillMaxSize().safeDrawingPadding()) {
        // the top bar (#85): the Set's name, Undo and Redo, Save while there is something to save,
        // and the rest in the overflow menu
        SetTopBar(
            name = state.workingCopy?.name,
            dirty = state.dirty,
            canSave = state.workingCopy != null && !state.saving && (state.dirty || state.workingCopy?.uri == null),
            canSaveAs = state.workingCopy != null && !state.saving,
            canUndo = state.canUndo,
            canRedo = state.canRedo,
            canExport = state.cardUids.isNotEmpty(),
            canClose = !state.saving,
            onSave = { scope.launch { if (!model.save()) onSaveAs() } },
            onSaveAs = onSaveAs,
            onUndo = { scope.launch { model.undo(); revision++ } },
            onRedo = { scope.launch { model.redo(); revision++ } },
            onExportAll = { export.startZip() },
            onClose = close,
            onTestEdit = if (debuggable && state.workingCopy != null) ({ model.testEdit() }) else null,
        )
        selected?.let { picked ->
            SelectionBar(
                count = picked.size,
                onAll = { selected = state.cardUids.toSet() },
                onDuplicate = { scope.launch { model.duplicateCards(inSetOrder(state, picked)); selected = null } },
                onDelete = {
                    scope.launch {
                        val n = picked.size
                        if (model.removeCards(inSetOrder(state, picked))) {
                            selected = null
                            val r = snackbar.showSnackbar("Deleted $n Card${if (n == 1) "" else "s"}", actionLabel = "Undo", duration = SnackbarDuration.Long)
                            if (r == SnackbarResult.ActionPerformed) model.undo()
                        }
                    }
                },
                onMove = { moving = true },
                onExport = { export.start(inSetOrder(state, picked)) },
                onDone = { selected = null },
            )
        }
        UnsupportedNotice(state)
        TabRow(selectedTabIndex = tab.ordinal) {
            for (t in SetTab.entries) Tab(selected = tab == t, onClick = { tab = t }, text = { Text(t.title) })
        }
        when (tab) {
            SetTab.Cards -> Box(Modifier.weight(1f)) {
                val open: (Int) -> Unit = { index ->
                    val picked = selected
                    if (picked == null) onOpenCard(index)
                    else state.cardUids.getOrNull(index)?.let { uid -> selected = if (uid in picked) picked - uid else picked + uid }
                }
                val select: (Int) -> Unit = { index -> state.cardUids.getOrNull(index)?.let { selected = (selected ?: emptySet()) + it } }
                Column(Modifier.fillMaxSize()) {
                    // the Cards as a grid of images or as a list of their values (#86)
                    Row(Modifier.padding(horizontal = 12.dp), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilterChip(selected = !asList, onClick = { asList = false; prefs.edit().putBoolean("cards-as-list", false).apply() }, label = { Text("Grid") }, modifier = Modifier.testTag("cards-grid-view"))
                        FilterChip(selected = asList, onClick = { asList = true; prefs.edit().putBoolean("cards-as-list", true).apply() }, label = { Text("List") }, modifier = Modifier.testTag("cards-list-view"))
                    }
                    Box(Modifier.weight(1f)) {
                        if (asList) CardsList(state, selected, onOpenCard = open, onSelect = select)
                        else CardsGrid(model, state, grid, selected, styles, onOpenCard = open, onSelect = select)
                    }
                }
                if (selected == null && state.session != 0L) {
                    // in which Style: the Set's, one its Cards use, or another (#87)
                    Box(Modifier.align(Alignment.BottomEnd).padding(16.dp)) {
                        ExtendedFloatingActionButton(onClick = { adding = true }, modifier = Modifier.testTag("add-card")) { Text("Add Card") }
                        AddCardMenu(model, styles, adding, onDismiss = { adding = false }) { style ->
                            scope.launch { model.addCard(state.cardUids.size - 1, style).takeIf { it >= 0 }?.let(onOpenCard) }
                        }
                    }
                }
            }
            SetTab.SetInfo -> Box(Modifier.weight(1f)) { SetInfoTab(model, state, revision) { editing = it } }
            SetTab.Keywords -> Box(Modifier.weight(1f)) { KeywordsTab(model, state, revision) { editing = it } }
        }
        editing?.let { target ->
            FieldSheet(model, target, revision, onField = { editing = target.copy(field = it) }, onClose = { editing = null; revision++ })
        }
        if (state.messages.isNotEmpty()) {
            Column(Modifier.fillMaxWidth().padding(8.dp).testTag("messages")) {
                for (m in state.messages.takeLast(3)) {
                    Text(m, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error, maxLines = 2)
                }
            }
        }
    }
    SnackbarHost(snackbar, Modifier.align(Alignment.BottomCenter).safeDrawingPadding().testTag("snackbar"))
    }
    CardExportUi(export)
    if (moving) {
        val picked = selected.orEmpty()
        MoveDialog(
            count = picked.size,
            cards = state.cardUids.size,
            onMove = { position ->
                moving = false
                scope.launch { model.moveCards(inSetOrder(state, picked), position); selected = null }
            },
            onCancel = { moving = false },
        )
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
private fun CardsGrid(
    model: SetViewModel,
    state: SetState,
    grid: LazyGridState,
    selected: Set<String>?,
    styles: List<TemplateInfo>,
    onOpenCard: (Int) -> Unit,
    onSelect: (Int) -> Unit,
) {
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
                // a Card in a Style of its own says which (#87)
                val ownStyle = state.cardStyles.getOrNull(index)?.takeIf { it.own }?.let { styleLabel(styles, it.stylesheet) }
                Thumbnail(
                    model, RenderKey(uid, cellWidthPx), state.cardNames.getOrElse(index) { "" }, state.aspect, linked, ownStyle,
                    picked = selected?.contains(uid),
                    onClick = { onOpenCard(index) },
                    onLongClick = { onSelect(index) },
                )
            }
        }
    }
}

@Composable
private fun Thumbnail(
    model: SetViewModel,
    key: RenderKey,
    name: String,
    aspect: Float,
    linked: Boolean,
    ownStyle: String?,
    picked: Boolean?,
    onClick: () -> Unit,
    onLongClick: () -> Unit,
) {
    val flow = remember(key) { model.observe(key) }
    val rendered = flow?.collectAsState()?.value
    val outline = if (picked == true) Modifier.border(3.dp, MaterialTheme.colorScheme.primary) else Modifier
    Column(
        Modifier.combinedClickable(onClick = onClick, onLongClick = onLongClick, onLongClickLabel = "Select")
            .then(outline)
            .semantics { if (picked != null) selected = picked }
            .testTag("thumbnail-${key.uid}"),
    ) {
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
        if (ownStyle != null) {
            Text(
                ownStyle, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.primary,
                maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.testTag("own-style-${key.uid}"),
            )
        }
    }
}

/**
 * The Cards as a list (#86): a row each, with the values of the columns the Game shows in its card
 * list (desktop MSE's), scrolling sideways together under their header. Tap opens, long press
 * selects, as in the grid.
 */
@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun CardsList(state: SetState, selected: Set<String>?, onOpenCard: (Int) -> Unit, onSelect: (Int) -> Unit) {
    var table by remember { mutableStateOf<CardTable?>(null) }
    LaunchedEffect(state.session, state.cardUids, state.cardNames, state.dirty, state.cardStyles) {
        if (state.session != 0L) table = Core.cardListTable(state.session)
    }
    val t = table
    if (t == null) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { CircularProgressIndicator() }
        return
    }
    // a Game with no card list columns still shows the names
    val columns = t.columns.ifEmpty { listOf(CardTable.Column("", "Name", 200)) }
    val rows = if (t.columns.isEmpty()) state.cardNames.map { listOf(it) } else t.rows
    fun width(c: CardTable.Column) = c.width.coerceIn(48, 240).dp
    val sideways = rememberScrollState()
    LazyColumn(Modifier.fillMaxSize().testTag("card-list")) {
        stickyHeader(key = "header") {
            Row(Modifier.fillMaxWidth().background(MaterialTheme.colorScheme.surfaceVariant).horizontalScroll(sideways).padding(horizontal = 12.dp, vertical = 8.dp)) {
                for (c in columns) Text(c.title, style = MaterialTheme.typography.labelLarge, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.width(width(c)).padding(end = 8.dp))
            }
        }
        itemsIndexed(state.cardUids, key = { _, uid -> uid }) { index, uid ->
            val row = rows.getOrNull(index).orEmpty()
            val picked = selected?.contains(uid) == true
            Row(
                Modifier.fillMaxWidth()
                    .background(if (picked) MaterialTheme.colorScheme.primaryContainer else Color.Transparent)
                    .combinedClickable(onClick = { onOpenCard(index) }, onLongClick = { onSelect(index) }, onLongClickLabel = "Select")
                    .horizontalScroll(sideways)
                    .padding(horizontal = 12.dp, vertical = 10.dp)
                    .testTag("list-row-$uid"),
            ) {
                for ((i, c) in columns.withIndex()) {
                    Text(row.getOrElse(i) { "" }, style = MaterialTheme.typography.bodyMedium, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.width(width(c)).padding(end = 8.dp))
                }
            }
            HorizontalDivider()
        }
    }
}

/** The picked Cards in the Set's order (as the core's calls want them) */
private fun inSetOrder(state: SetState, picked: Set<String>) = state.cardUids.filter { it in picked }

/** Selection mode's bar: how many Cards are picked, and what to do with them */
@Composable
private fun SelectionBar(
    count: Int,
    onAll: () -> Unit,
    onDuplicate: () -> Unit,
    onDelete: () -> Unit,
    onMove: () -> Unit,
    onExport: () -> Unit,
    onDone: () -> Unit,
) {
    Row(
        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 8.dp).testTag("selection-bar"),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("$count selected", style = MaterialTheme.typography.titleSmall, modifier = Modifier.padding(horizontal = 8.dp).testTag("selected-count"))
        TextButton(onClick = onAll, modifier = Modifier.testTag("select-all")) { Text("Select all") }
        TextButton(onClick = onDuplicate, enabled = count > 0, modifier = Modifier.testTag("duplicate")) { Text("Duplicate") }
        TextButton(onClick = onMove, enabled = count > 0, modifier = Modifier.testTag("move")) { Text("Move…") }
        TextButton(onClick = onDelete, enabled = count > 0, modifier = Modifier.testTag("delete")) { Text("Delete") }
        TextButton(onClick = onExport, enabled = count > 0, modifier = Modifier.testTag("export")) { Text("Export…") }
        TextButton(onClick = onDone, modifier = Modifier.testTag("selection-done")) { Text("Done") }
    }
}

/** Where to move the picked Cards: the position (1 = first) the first of them goes to */
@Composable
private fun MoveDialog(count: Int, cards: Int, onMove: (Int) -> Unit, onCancel: () -> Unit) {
    val last = cards - count + 1
    var text by remember { mutableStateOf("1") }
    val position = text.toIntOrNull()?.takeIf { it in 1..last }
    AlertDialog(
        onDismissRequest = onCancel,
        title = { Text("Move $count Card${if (count == 1) "" else "s"}") },
        text = {
            OutlinedTextField(
                text, { text = it.filter(Char::isDigit) },
                label = { Text("To position (1 to $last)") },
                singleLine = true,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
                modifier = Modifier.testTag("move-position"),
            )
        },
        confirmButton = {
            TextButton(onClick = { position?.let { onMove(it - 1) } }, enabled = position != null, modifier = Modifier.testTag("move-confirm")) { Text("Move") }
        },
        dismissButton = { TextButton(onClick = onCancel) { Text("Cancel") } },
    )
}

/** A notice when the Set or one of its Cards uses an Unsupported style (ADR 0010) */
@Composable
private fun UnsupportedNotice(state: SetState) {
    val context = LocalContext.current
    val styles = remember(state.cardStyles) { state.cardStyles.map { it.stylesheet }.toSet() }
    var unsupported by remember { mutableStateOf<List<String>>(emptyList()) }
    LaunchedEffect(styles) {
        if (styles.isEmpty()) return@LaunchedEffect
        val support = StyleSupport.get(context)
        unsupported = io.github.type_safe_games.mobileseteditor.core.Core.listTemplates().templates
            .filter { it.name in styles && support.of(it) == Support.Unsupported }
            .map { it.fullName.ifEmpty { it.name } }
    }
    if (unsupported.isEmpty()) return
    Text(
        "Unsupported in this version: ${unsupported.joinToString()}. Cards with it may not look as they do in the desktop editor.",
        style = MaterialTheme.typography.bodySmall,
        color = MaterialTheme.colorScheme.error,
        modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 4.dp).testTag("unsupported-notice"),
    )
}

/** The Set home's app bar (#85). [onTestEdit] is the debug build's "Test edit" (null: not shown). */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SetTopBar(
    name: String?,
    dirty: Boolean,
    canSave: Boolean,
    canSaveAs: Boolean,
    canUndo: Boolean,
    canRedo: Boolean,
    canExport: Boolean,
    canClose: Boolean,
    onSave: () -> Unit,
    onSaveAs: () -> Unit,
    onUndo: () -> Unit,
    onRedo: () -> Unit,
    onExportAll: () -> Unit,
    onClose: () -> Unit,
    onTestEdit: (() -> Unit)?,
) {
    var menu by remember { mutableStateOf(false) }
    TopAppBar(
        title = {
            Column {
                Text(
                    (name ?: "Opening…").removeSuffix(".mse-set"),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.testTag("set-name"),
                )
                if (dirty) Text("Unsaved changes", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.error)
            }
        },
        navigationIcon = {
            IconButton(onClick = onClose, enabled = canClose, modifier = Modifier.testTag("close")) {
                Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Close")
            }
        },
        actions = {
            IconButton(onClick = onUndo, enabled = canUndo, modifier = Modifier.testTag("home-undo")) {
                Icon(painterResource(R.drawable.ic_undo), contentDescription = "Undo")
            }
            IconButton(onClick = onRedo, enabled = canRedo, modifier = Modifier.testTag("home-redo")) {
                Icon(painterResource(R.drawable.ic_redo), contentDescription = "Redo")
            }
            if (canSave) {
                IconButton(onClick = onSave, modifier = Modifier.testTag("save")) {
                    Icon(painterResource(R.drawable.ic_save), contentDescription = "Save")
                }
            }
            Box {
                IconButton(onClick = { menu = true }, modifier = Modifier.testTag("set-menu")) {
                    Icon(Icons.Filled.MoreVert, contentDescription = "More")
                }
                DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                    DropdownMenuItem(text = { Text("Save") }, enabled = canSave, onClick = { menu = false; onSave() })
                    DropdownMenuItem(text = { Text("Save as…") }, enabled = canSaveAs, onClick = { menu = false; onSaveAs() })
                    DropdownMenuItem(
                        text = { Text("Export all…") }, enabled = canExport,
                        onClick = { menu = false; onExportAll() }, modifier = Modifier.testTag("export-all"),
                    )
                    DropdownMenuItem(text = { Text("Close") }, enabled = canClose, onClick = { menu = false; onClose() })
                    if (onTestEdit != null) {
                        DropdownMenuItem(text = { Text("Test edit") }, onClick = { menu = false; onTestEdit() }, modifier = Modifier.testTag("test-edit"))
                    }
                }
            }
        },
    )
}
