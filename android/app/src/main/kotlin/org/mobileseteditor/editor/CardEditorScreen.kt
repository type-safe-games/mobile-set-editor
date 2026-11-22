// Mobile Set Editor - the swipe editor: swipe between Cards, tap a field on the Card to edit it (#62)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.editor

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
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
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.drawscope.translate
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.text.input.TextFieldValue
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.mobileseteditor.SetViewModel
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.FieldInfo
import org.mobileseteditor.core.TargetKind
import org.mobileseteditor.core.SymbolMenuResult
import org.mobileseteditor.core.TextResult
import org.mobileseteditor.render.RenderKey

/** A field's name (or an undo step's, which may contain one) as people read it: "card_color" is "card color" */
fun displayName(name: String) = name.replace('_', ' ')

/** The field being edited: which Card, which field, of the Card itself or of its style options */
data class EditTarget(val uid: String, val field: String, val kind: TargetKind = TargetKind.Card)

/** Hold the re-render back this long after a keystroke, so typing doesn't queue a render per key */
private const val TYPING_RENDER_DELAY = 300L

/**
 * The open Set's Cards, one per page in the Set's order; swiping goes to the neighbours, which are
 * rendered ahead. Tapping a field on the Card edits it in a sheet below the Card, which keeps the
 * field in view above it. Undo and Redo are in the top bar.
 */
@Composable
fun CardEditorScreen(model: SetViewModel, onBack: () -> Unit) {
    val state by model.state.collectAsState()
    val scope = rememberCoroutineScope()
    var editing by remember { mutableStateOf<EditTarget?>(null) }
    // bumped by Undo and Redo: the sheet reads its text again
    var revision by remember { mutableIntStateOf(0) }
    // the Card details sheet, for this Card (#67)
    var details by remember { mutableStateOf<String?>(null) }
    val export = org.mobileseteditor.rememberCardExport(model)
    BackHandler { if (editing != null) editing = null else onBack() }
    val pager = rememberPagerState(initialPage = state.index.coerceIn(0, maxOf(0, state.cardUids.size - 1))) { state.cardUids.size }

    Column(Modifier.fillMaxSize().safeDrawingPadding()) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            OutlinedButton(onClick = onBack, modifier = Modifier.testTag("back-to-cards")) { Text("‹ Cards") }
            Text(
                if (state.cardUids.isEmpty()) "" else "${state.index + 1} / ${state.cardUids.size}  ${state.cardNames.getOrElse(state.index) { "" }}",
                modifier = Modifier.weight(1f).testTag("card-position"),
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            TextButton(
                onClick = { scope.launch { model.undo(); revision++ } },
                enabled = state.canUndo,
                modifier = Modifier.testTag("undo"),
            ) { Text(if (state.undoName.isEmpty()) "Undo" else "Undo ${displayName(state.undoName).lowercase()}", maxLines = 1) }
            TextButton(
                onClick = { scope.launch { model.redo(); revision++ } },
                enabled = state.canRedo,
                modifier = Modifier.testTag("redo"),
            ) { Text(if (state.redoName.isEmpty()) "Redo" else "Redo ${displayName(state.redoName).lowercase()}", maxLines = 1) }
        }
        if (state.cardUids.isNotEmpty()) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                TextButton(
                    onClick = { details = state.cardUids.getOrNull(pager.currentPage) },
                    modifier = Modifier.padding(start = 8.dp).testTag("details"),
                ) { Text("Details") }
                TextButton(
                    onClick = {
                        scope.launch {
                            val added = model.addCard(pager.currentPage)
                            if (added >= 0) pager.scrollToPage(added)
                        }
                    },
                    modifier = Modifier.testTag("add-card-here"),
                ) { Text("Add") }
                TextButton(
                    onClick = { state.cardUids.getOrNull(pager.currentPage)?.let { export.start(listOf(it)) } },
                    modifier = Modifier.testTag("export-card"),
                ) { Text("Export") }
                Box(Modifier.weight(1f)) {
                    LinkBar(model, state, pager.currentPage) { target -> scope.launch { pager.animateScrollToPage(target) } }
                }
            }
        }
        if (state.cardUids.isEmpty()) {
            Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                if (state.session != 0L || state.opening) CircularProgressIndicator() else Text("The Set has no Cards")
            }
        } else {
            // the card list changed (added, removed, moved): stay on the model's current Card
            LaunchedEffect(state.cardUids) {
                if (pager.currentPage != state.index && state.index in state.cardUids.indices) pager.scrollToPage(state.index)
            }
            LaunchedEffect(pager) {
                snapshotFlow { pager.currentPage }.collect { page ->
                    model.go(page)
                    if (editing?.uid != state.cardUids.getOrNull(page)) editing = null
                }
            }
            BoxWithConstraints(Modifier.weight(1f).fillMaxWidth()) {
                val width = constraints.maxWidth
                val viewport = constraints.maxHeight
                LaunchedEffect(width) { model.setWidth(width) }
                HorizontalPager(
                    state = pager,
                    userScrollEnabled = editing == null,
                    beyondViewportPageCount = 1,
                    key = { state.cardUids.getOrElse(it) { "$it" } },
                    modifier = Modifier.fillMaxSize().testTag("card-pager"),
                ) { page ->
                    val uid = state.cardUids.getOrNull(page) ?: return@HorizontalPager
                    CardPage(
                        model, RenderKey(uid, width), state.cardNames.getOrElse(page) { "" }, state.aspect, viewport,
                        selected = editing?.takeIf { it.uid == uid && it.kind == TargetKind.Card }?.field,
                        onTap = { editing = EditTarget(uid, it) },
                        onLongPress = { details = uid },
                    )
                }
            }
        }
        editing?.let { target ->
            FieldSheet(model, target, revision, onField = { editing = target.copy(field = it) }, onClose = { editing = null })
        }
        if (state.messages.isNotEmpty()) {
            Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp).testTag("messages")) {
                for (m in state.messages.takeLast(2)) {
                    Text(m, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error, maxLines = 2)
                }
            }
        }
    }
    org.mobileseteditor.CardExportUi(export)
    details?.let { uid ->
        CardDetailsSheet(
            model, uid, revision,
            onEdit = { target -> details = null; editing = target },
            onDismiss = { details = null },
        )
    }
}

/** One Card at the screen's width, scrolling when taller than the space; its fields can be tapped */
@Composable
private fun CardPage(
    model: SetViewModel,
    key: RenderKey,
    name: String,
    aspect: Float,
    viewport: Int,
    selected: String?,
    onTap: (String) -> Unit,
    onLongPress: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    val flow = remember(key) { model.observe(key) }
    val rendered = flow?.collectAsState()?.value
    val scroll = rememberScrollState()
    val highlight = MaterialTheme.colorScheme.primary
    val stroke = with(LocalDensity.current) { 3.dp.toPx() }
    val region = rendered?.regions?.firstOrNull { it.field == selected }
    // keep the field being edited in view, also when the sheet and the keyboard take space
    LaunchedEffect(region?.field, region?.y, viewport) {
        if (region == null) return@LaunchedEffect
        val scale = key.width.toDouble() / rendered.image.width
        val (_, top, _, bottom) = HitTest.bounds(region).map { (it * scale).toInt() }
        val margin = 16
        val target = when {
            bottom - top + 2 * margin > viewport -> top - margin
            top - margin < scroll.value -> top - margin
            bottom + margin > scroll.value + viewport -> bottom + margin - viewport
            else -> return@LaunchedEffect
        }
        scroll.animateScrollTo(target.coerceIn(0, scroll.maxValue))
    }
    Column(Modifier.fillMaxSize().verticalScroll(scroll)) {
        if (rendered == null) {
            Box(Modifier.fillMaxWidth().aspectRatio(aspect).background(MaterialTheme.colorScheme.surfaceVariant), contentAlignment = Alignment.Center) {
                CircularProgressIndicator()
            }
            return@Column
        }
        val image = rendered.image
        Image(
            bitmap = image.asImageBitmap(),
            contentDescription = name,
            modifier = Modifier
                .fillMaxWidth()
                .aspectRatio(image.width.toFloat() / image.height)
                .pointerInput(rendered) {
                    detectTapGestures(onLongPress = { onLongPress() }) { at ->
                        // the core picks the field, as the desktop does (masks included)
                        val s = image.width / size.width.toDouble()
                        scope.launch {
                            val hit = Core.fieldAt(model.state.value.session, key.uid, at.x * s, at.y * s, image.width)
                            if (hit.result.ok && hit.field.isNotEmpty()) onTap(hit.field)
                        }
                    }
                }
                .drawWithContent {
                    drawContent()
                    if (region != null) {
                        val s = size.width / image.width
                        translate(region.x.toFloat() * s, region.y.toFloat() * s) {
                            rotate(-region.rotation.toFloat(), pivot = Offset.Zero) {
                                drawRect(highlight, size = Size(region.width.toFloat() * s, region.height.toFloat() * s), style = Stroke(stroke))
                            }
                        }
                    }
                }
                .testTag("card-${key.uid}"),
        )
    }
}

/** The sheet below the Card: the field's name, ‹ › to the Card's other editable fields, and its editor */
@Composable
fun FieldSheet(model: SetViewModel, target: EditTarget, revision: Int, onField: (String) -> Unit, onClose: () -> Unit) {
    val session = model.state.value.session
    var fields by remember(target.uid, target.kind) { mutableStateOf<List<FieldInfo>?>(null) }
    LaunchedEffect(target.uid, target.kind, revision) { fields = Core.listFields(session, target.kind, target.uid).fields }
    // the fields this Card shows, from its render
    val regions = if (target.kind != TargetKind.Card) null
    else model.observe(RenderKey(target.uid, model.renderWidth))?.collectAsState()?.value?.regions
    val onCard = regions?.map { it.field }?.toSet()
    val aspect = regions?.firstOrNull { it.field == target.field }?.let { (it.width / it.height).toFloat() } ?: 1f
    val all = fields
    val info = all?.firstOrNull { it.name == target.field }
    // ‹ › steps through the fields on the Card when one was tapped there, else through all of them
    val editable = all.orEmpty().filter { it.editable && (it.type == "text" || it.type in VALUE_EDITOR_TYPES) }.map { it.name }
    val textFields = if (target.kind == TargetKind.Card && onCard != null && target.field in onCard) editable.filter { it in onCard } else editable
    val at = textFields.indexOf(target.field)
    Surface(tonalElevation = 3.dp, modifier = Modifier.fillMaxWidth().testTag("field-sheet")) {
        Column(Modifier.padding(8.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                TextButton(onClick = { onField(textFields[at - 1]) }, enabled = at > 0, modifier = Modifier.testTag("previous-field")) { Text("‹") }
                Text(
                    (when (target.kind) { TargetKind.CardStyle, TargetKind.SetStyle -> "Style option: "; TargetKind.Keyword -> "Keyword: "; else -> "" }) + displayName(target.field).replaceFirstChar { it.uppercase() },
                    style = MaterialTheme.typography.titleSmall,
                    modifier = Modifier.weight(1f).testTag("editing-field"),
                    maxLines = 1,
                )
                TextButton(onClick = { onField(textFields[at + 1]) }, enabled = at in 0 until textFields.size - 1, modifier = Modifier.testTag("next-field")) { Text("›") }
                TextButton(onClick = onClose, modifier = Modifier.testTag("done")) { Text("Done") }
            }
            when {
                info == null -> CircularProgressIndicator(Modifier.padding(8.dp))
                info.editable && info.type in VALUE_EDITOR_TYPES -> ValueEditor(model, session, target.uid, info, aspect, revision, target.kind)
                info.type != "text" || !info.editable -> Text(
                    if (!info.editable) "This field can't be edited." else "This is a ${info.type} field: editing it here comes later.",
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.padding(8.dp).testTag("not-editable"),
                )
                else -> TextEditor(model, session, target, info.multiLine, revision)
            }
        }
    }
}

/**
 * A text field's editor. The core holds the text: each change goes to it as one replacement (in
 * code points), and what it answers (scripts may rewrite the text) replaces what is shown, unless
 * more was typed meanwhile. Locked spans (e.g. reminder text, keyword parameters) are atomic.
 */
@Composable
private fun TextEditor(model: SetViewModel, session: Long, target: EditTarget, multiLine: Boolean, revision: Int) {
    val scope = rememberCoroutineScope()
    val holder = remember(target) { CoreText() }
    var value by remember(target) { mutableStateOf<TextFieldValue?>(null) }
    val focus = remember { FocusRequester() }
    fun show(t: TextResult) {
        holder.sync.answered(t.text, t.selectionEnd)
        holder.locked = t.locked
        value = TextFieldValue(t.text, TextRange(CodePoints.toUtf16(t.text, t.selectionStart), CodePoints.toUtf16(t.text, t.selectionEnd)))
    }
    LaunchedEffect(target, revision) {
        show(Core.getText(session, target.kind, target.uid, target.field))
    }
    // the field's symbol font, if any: a symbol bar, and typing in symbols (#64)
    var symbols by remember(target) { mutableStateOf<SymbolMenuResult?>(null) }
    var symbolMode by remember(target) { mutableStateOf(false) }
    LaunchedEffect(target) { symbols = Core.symbolMenu(session, target.kind, target.uid, target.field) }
    /** Insert a symbol's code at the caret (over the selection), as a symbol */
    fun insertSymbol(code: String) = scope.launch {
        holder.mutex.withLock {
            val shown = value ?: return@withLock
            if (shown.text != holder.sync.shown || code.isEmpty()) return@withLock
            val start = CodePoints.fromUtf16(shown.text, shown.selection.min)
            val end = CodePoints.fromUtf16(shown.text, shown.selection.max)
            val atomic = LockedSpans.adjust(holder.sync.toCore(TextChange(start, end, code)), holder.locked)
            val r = Core.replaceRange(session, target.kind, target.uid, target.field, atomic.start, atomic.end, code, asSymbol = true)
            model.edited(r.action)
            show(if (r.action.result.ok) r else Core.getText(session, target.kind, target.uid, target.field))
        }
        focus.requestFocus()
    }
    val current = value ?: return
    OutlinedTextField(
        value = current,
        onValueChange = { new ->
            val old = value ?: return@OutlinedTextField
            if (new.text == old.text) {
                // the caret moved: not into a locked span
                fun snap(i: Int) =
                    if (new.text != holder.sync.core) i // (locked spans are where the core's text has them)
                    else CodePoints.toUtf16(new.text, LockedSpans.snap(CodePoints.fromUtf16(new.text, i), holder.locked))
                value = new.copy(selection = TextRange(snap(new.selection.start), snap(new.selection.end)))
                return@OutlinedTextField
            }
            value = new
            scope.launch {
                holder.mutex.withLock {
                    val typed = value ?: return@withLock
                    val change = TextDiff.between(holder.sync.shown, typed.text, typed.selection.end) ?: return@withLock
                    val atomic = LockedSpans.adjust(holder.sync.toCore(change), holder.locked)
                    val asSymbol = symbolMode && atomic.text.isNotEmpty()
                    // a key at a time, each at the caret the core answers with (auto-replace may move it)
                    var start = atomic.start
                    var end = atomic.end
                    var r: TextResult? = null
                    for (key in Keystrokes.of(atomic.text)) {
                        val step = Core.replaceRange(session, target.kind, target.uid, target.field, start, end, key, asSymbol)
                        model.edited(step.action, renderDelayMillis = TYPING_RENDER_DELAY)
                        if (!step.action.result.ok) return@withLock show(Core.getText(session, target.kind, target.uid, target.field))
                        r = step
                        start = step.selectionEnd
                        end = start
                    }
                    r ?: return@withLock
                    holder.sync.answered(r.text, r.selectionEnd, typed.text, CodePoints.fromUtf16(typed.text, typed.selection.end))
                    holder.locked = r.locked
                    // nothing typed meanwhile: show what the core made of it
                    if (value?.text == typed.text) show(r)
                }
            }
        },
        singleLine = !multiLine,
        minLines = if (multiLine) 3 else 1,
        maxLines = if (multiLine) 6 else 1,
        modifier = Modifier.fillMaxWidth().focusRequester(focus).testTag("field-text"),
    )
    // a keyword's match: its parameters are inserted, as locked spans
    if (target.kind == TargetKind.Keyword && target.field == "match") {
        KeywordParameterBar(session) { type ->
            scope.launch {
                holder.mutex.withLock {
                    val shown = value ?: return@withLock
                    if (shown.text != holder.sync.shown) return@withLock
                    val start = CodePoints.fromUtf16(shown.text, shown.selection.min)
                    val end = CodePoints.fromUtf16(shown.text, shown.selection.max)
                    val atomic = LockedSpans.adjust(holder.sync.toCore(TextChange(start, end, "x")), holder.locked)
                    val r = Core.insertKeywordParameter(session, target.uid.toLong(), atomic.start, atomic.end, type)
                    model.edited(r.action)
                    show(if (r.action.result.ok) r else Core.getText(session, target.kind, target.uid, target.field))
                }
                focus.requestFocus()
            }
        }
    }
    symbols?.takeIf { it.hasSymbols }?.let { menu ->
        SymbolBar(
            menu, session, target.uid, target.field, target.kind,
            symbolMode = symbolMode,
            onSymbolMode = { symbolMode = it; focus.requestFocus() },
            onInsert = { insertSymbol(it) },
        )
    }
    // (once the field is there to take it)
    LaunchedEffect(target) { focus.requestFocus() }
}

/** The text as the core last answered it, which edits are diffed against */
private class CoreText {
    val sync = TextSync()
    var locked: List<IntRange> = emptyList()
    val mutex = Mutex()
}

/** Buttons that insert a keyword parameter of each of the Game's types */
@Composable
private fun KeywordParameterBar(session: Long, onInsert: (String) -> Unit) {
    val types by androidx.compose.runtime.produceState(emptyList<org.mobileseteditor.core.KeywordParamType>(), session) {
        value = Core.listKeywords(session).parameterTypes
    }
    if (types.isEmpty()) return
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.testTag("keyword-parameters")) {
        Text("Insert:", style = MaterialTheme.typography.labelLarge, modifier = Modifier.padding(end = 4.dp))
        androidx.compose.foundation.lazy.LazyRow {
            items(types.size) { i ->
                TextButton(onClick = { onInsert(types[i].name) }, modifier = Modifier.testTag("parameter-${types[i].name}")) {
                    Text("<${types[i].name}>")
                }
            }
        }
    }
}
