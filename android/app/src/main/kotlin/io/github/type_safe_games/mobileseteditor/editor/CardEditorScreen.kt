// Mobile Set Editor - the swipe editor: swipe between Cards, tap a field on the Card to edit it (#62)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.editor

import androidx.activity.compose.BackHandler
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
import io.github.type_safe_games.mobileseteditor.SetViewModel
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.FieldInfo
import io.github.type_safe_games.mobileseteditor.core.TargetKind
import io.github.type_safe_games.mobileseteditor.core.SymbolMenuResult
import io.github.type_safe_games.mobileseteditor.core.TextResult
import io.github.type_safe_games.mobileseteditor.render.RenderKey
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.ui.input.pointer.PointerInputScope
import androidx.compose.ui.input.pointer.PointerInputChange
import androidx.compose.ui.input.pointer.changedToUp
import kotlinx.coroutines.withTimeoutOrNull
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import io.github.type_safe_games.mobileseteditor.AddCardMenu
import io.github.type_safe_games.mobileseteditor.StylePickerDialog
import io.github.type_safe_games.mobileseteditor.rememberGameStyles
import io.github.type_safe_games.mobileseteditor.styleLabel
import io.github.type_safe_games.mobileseteditor.core.Caret
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.draw.alpha
import androidx.compose.foundation.layout.size
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.material3.SmallFloatingActionButton
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.layout.layout
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.material3.FilterChip

/** A field's name (or an undo step's, which may contain one) as people read it: "card_color" is "card color" */
fun displayName(name: String) = name.replace('_', ' ')

/** The field being edited: which Card, which field, of the Card itself or of its style options */
data class EditTarget(val uid: String, val field: String, val kind: TargetKind = TargetKind.Card)

/**
 * Editing text on the Card itself (#83, a prototype): the text field stays, invisible, for the
 * keyboard; the caret is drawn on the Card. [caret] is where it is, [tapped] where a tap on the
 * Card asks it to go (both visible text code points).
 */
class OnCardEditing {
    var caret by mutableIntStateOf(0)
    var tapped by mutableStateOf<Int?>(null)
}

/** The caret nearest a point: on the line under it, else the nearest line, then the nearest x */
fun nearestCaret(carets: List<Caret>, x: Double, y: Double): Int =
    carets.indices.minByOrNull { i ->
        val c = carets[i]
        val dy = when { y < c.top -> c.top - y; y > c.bottom -> y - c.bottom; else -> 0.0 }
        dy * 1000 + kotlin.math.abs(c.x - x)
    } ?: 0

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
    val export = io.github.type_safe_games.mobileseteditor.rememberCardExport(model)
    val styles = rememberGameStyles(model)
    var adding by remember { mutableStateOf(false) }
    var pickingStyle by remember { mutableStateOf(false) }
    // the double-wide Cards showing their back face (#90)
    var backs by rememberSaveable { mutableStateOf(setOf<String>()) }
    // #83's prototype, on by default so it can be tried; the old sheet is a tap away
    val prefs = LocalContext.current.getSharedPreferences("prototypes", android.content.Context.MODE_PRIVATE)
    var onCardMode by remember { mutableStateOf(prefs.getBoolean("on-card-editing", true)) }
    val onCard = remember { OnCardEditing() }
    BackHandler { if (editing != null) editing = null else onBack() }
    val pager = rememberPagerState(initialPage = state.index.coerceIn(0, maxOf(0, state.cardUids.size - 1))) { state.cardUids.size }

    Column(Modifier.fillMaxSize().safeDrawingPadding()) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp),
        ) {
            OutlinedButton(onClick = onBack, modifier = Modifier.testTag("back-to-cards")) { Text("‹ Cards") }
            // ‹ › to the neighbouring Cards, besides swiping (#89)
            IconButton(
                onClick = { scope.launch { pager.animateScrollToPage(pager.currentPage - 1) } },
                enabled = editing == null && pager.currentPage > 0,
                modifier = Modifier.testTag("previous-card"),
            ) { Icon(Icons.AutoMirrored.Filled.KeyboardArrowLeft, contentDescription = "Previous Card") }
            Column(Modifier.weight(1f)) {
                Text(
                    if (state.cardUids.isEmpty()) "" else "${state.index + 1} / ${state.cardUids.size}  ${state.cardNames.getOrElse(state.index) { "" }}",
                    modifier = Modifier.testTag("card-position"),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                // the Card's Style, to change it here rather than at the bottom of Details (#87)
                state.cardStyles.getOrNull(state.index)?.let { cardStyle ->
                    Text(
                        styleLabel(styles, cardStyle.stylesheet) + " ▾",
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.primary,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.clickable(enabled = editing == null) { pickingStyle = true }.testTag("card-style-chip"),
                    )
                }
            }
            IconButton(
                onClick = { scope.launch { pager.animateScrollToPage(pager.currentPage + 1) } },
                enabled = editing == null && pager.currentPage < state.cardUids.size - 1,
                modifier = Modifier.testTag("next-card"),
            ) { Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, contentDescription = "Next Card") }
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
                Box {
                    TextButton(onClick = { adding = true }, modifier = Modifier.testTag("add-card-here")) { Text("Add") }
                    AddCardMenu(model, styles, adding, onDismiss = { adding = false }) { style ->
                        scope.launch {
                            val added = model.addCard(pager.currentPage, style)
                            if (added >= 0) pager.scrollToPage(added)
                        }
                    }
                }
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
                    // a double-faced Card (#90): a double-wide Style has both faces side by side in one
                    // Card, shown a face at a time; a linked Front Face / Back Face is another Card
                    var doubleWide by remember(uid) { mutableStateOf(false) }
                    LaunchedEffect(uid, state.cardStyles.getOrNull(page)) {
                        val size = Core.cardSize(state.session, uid)
                        doubleWide = size.result.ok && size.height > 0 && size.width / size.height > DOUBLE_WIDE
                        model.setDoubleWide(uid, doubleWide)
                    }
                    val otherFace = state.cardLinks.getOrNull(page).orEmpty()
                        .firstOrNull { it.relation == "Back Face" || it.relation == "Front Face" }
                        ?.let { link -> state.cardUids.indexOf(link.uid).takeIf { it >= 0 } }
                    Box(Modifier.fillMaxSize()) {
                        CardPage(
                            model, RenderKey(uid, if (doubleWide) width * 2 else width), state.cardNames.getOrElse(page) { "" }, state.aspect, viewport,
                            selected = editing?.takeIf { it.uid == uid && it.kind == TargetKind.Card }?.field,
                            onTap = { editing = EditTarget(uid, it) },
                            onLongPress = { details = uid },
                            scrolling = { pager.isScrollInProgress },
                            onCard = onCard.takeIf { onCardMode },
                            face = if (!doubleWide) null else if (uid in backs) Face.Back else Face.Front,
                        )
                        if (doubleWide || otherFace != null) {
                            SmallFloatingActionButton(
                                onClick = {
                                    if (doubleWide) { backs = if (uid in backs) backs - uid else backs + uid; editing = null }
                                    else otherFace?.let { scope.launch { pager.animateScrollToPage(it) } }
                                },
                                modifier = Modifier.align(Alignment.TopEnd).padding(8.dp).testTag("flip-$uid"),
                            ) { Text(if (doubleWide && uid in backs) "Front" else "Flip", modifier = Modifier.padding(horizontal = 8.dp)) }
                        }
                    }
                }
            }
        }
        editing?.let { target ->
            FieldSheet(
                model, target, revision, onField = { editing = target.copy(field = it) }, onClose = { editing = null },
                inline = onCard.takeIf { onCardMode },
                onCardMode = onCardMode,
                onToggleOnCard = { onCardMode = !onCardMode; prefs.edit().putBoolean("on-card-editing", onCardMode).apply() },
            )
        }
        if (state.messages.isNotEmpty()) {
            Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp).testTag("messages")) {
                for (m in state.messages.takeLast(2)) {
                    Text(m, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error, maxLines = 2)
                }
            }
        }
    }
    io.github.type_safe_games.mobileseteditor.CardExportUi(export)
    if (pickingStyle) {
        StylePickerDialog("The Card's Style", styles, onDismiss = { pickingStyle = false }) { picked ->
            pickingStyle = false
            state.cardUids.getOrNull(pager.currentPage)?.let { uid -> scope.launch { model.setCardStyle(uid, picked.name) } }
        }
    }
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
    scrolling: () -> Boolean,
    onCard: OnCardEditing? = null,
    face: Face? = null,
) {
    val scope = rememberCoroutineScope()
    val flow = remember(key) { model.observe(key) }
    val rendered = flow?.collectAsState()?.value
    val scroll = rememberScrollState()
    val highlight = MaterialTheme.colorScheme.primary
    val stroke = with(LocalDensity.current) { 3.dp.toPx() }
    val region = rendered?.regions?.firstOrNull { it.field == selected }
    // the field's carets on this render, when editing it on the Card (#83)
    var carets by remember(key) { mutableStateOf<List<Caret>>(emptyList()) }
    LaunchedEffect(rendered, selected, onCard) {
        carets = if (onCard != null && selected != null && rendered != null) {
            // laid out at the card's own size, not the screen's: the core lays the whole card out
            // and draws it for this, so at full width it cost as much as a render, every keystroke
            val session = model.state.value.session
            val natural = Core.cardSize(session, key.uid).width.toInt().coerceAtLeast(1)
            val scale = rendered.image.width.toDouble() / natural
            Core.textCarets(session, key.uid, selected, natural).map { Caret(it.x * scale, it.top * scale, it.bottom * scale) }
        } else emptyList()
    }
    val caretColor = MaterialTheme.colorScheme.primary
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
        // a double-wide Card shows one face: the image is twice the page's width, and the page shows
        // its left or right half. Taps, the highlight and the caret are in the image's own
        // coordinates, so they need nothing more
        val half = if (face == null) Modifier else Modifier.fillMaxWidth().aspectRatio(image.width / 2f / image.height).clipToBounds()
            .testTag(if (face == Face.Back) "card-face-back" else "card-face-front")
        Box(half) {
        Image(
            bitmap = image.asImageBitmap(),
            contentDescription = name,
            modifier = (if (face == null) Modifier.fillMaxWidth() else Modifier.layout { m, c ->
                    val w = c.maxWidth
                    val p = m.measure(c.copy(minWidth = 2 * w, maxWidth = 2 * w))
                    layout(w, p.height) { p.place(if (face == Face.Back) -w else 0, 0) }
                })
                .aspectRatio(image.width.toFloat() / image.height)
                .pointerInput(rendered) {
                    detectTapsNotSwipes(onLongPress = onLongPress, ignore = scrolling) { at ->
                        // the core picks the field, as the desktop does (masks included)
                        val s = image.width / size.width.toDouble()
                        scope.launch {
                            val hit = Core.fieldAt(model.state.value.session, key.uid, at.x * s, at.y * s, image.width)
                            if (!hit.result.ok || hit.field.isEmpty()) return@launch
                            // a tap in the field being edited on the Card moves the caret there (#83)
                            if (onCard != null && hit.field == selected && carets.isNotEmpty()) onCard.tapped = nearestCaret(carets, at.x * s, at.y * s)
                            else onTap(hit.field)
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
                    carets.getOrNull(onCard?.caret ?: -1)?.let { c ->
                        val s = size.width / image.width
                        drawLine(
                            caretColor, Offset(c.x.toFloat() * s, c.top.toFloat() * s), Offset(c.x.toFloat() * s, c.bottom.toFloat() * s),
                            strokeWidth = stroke * 0.8f,
                        )
                    }
                }
                .testTag("card-${key.uid}"),
        )
        }
    }
}

/** Which face of a double-wide Card is shown (#90) */
enum class Face { Front, Back }

/** Wider than this (width / height) is a double-wide Card with both faces side by side (#90); a
 *  single face is about 0.72 */
private const val DOUBLE_WIDE = 1.2

/** The sheet below the Card: the field's name, ‹ › to the Card's other editable fields, and its editor */
@Composable
fun FieldSheet(model: SetViewModel, target: EditTarget, revision: Int, onField: (String) -> Unit, onClose: () -> Unit, inline: OnCardEditing? = null, onCardMode: Boolean = false, onToggleOnCard: (() -> Unit)? = null) {
    val session = model.state.collectAsState().value.session
    var fields by remember(target.uid, target.kind) { mutableStateOf<List<FieldInfo>?>(null) }
    LaunchedEffect(target.uid, target.kind, revision) { fields = Core.listFields(session, target.kind, target.uid).fields }
    // the fields this Card shows, from its render
    val regions = if (target.kind != TargetKind.Card) null
    else model.observe(RenderKey(target.uid, model.renderWidthFor(target.uid)))?.collectAsState()?.value?.regions
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
                // editing text on the Card or in this sheet (#83); only where it makes a difference
                if (onToggleOnCard != null && info?.type == "text" && info.editable && target.kind == TargetKind.Card) {
                    TextButton(onClick = onToggleOnCard, modifier = Modifier.testTag("on-card-toggle")) { Text(if (onCardMode) "On card ✓" else "On card") }
                }
                // Done also puts the keyboard away, so the whole Card is there to tap again
                val keyboard = LocalSoftwareKeyboardController.current
                TextButton(onClick = { keyboard?.hide(); onClose() }, modifier = Modifier.testTag("done")) { Text("Done") }
            }
            when {
                info == null -> CircularProgressIndicator(Modifier.padding(8.dp))
                info.editable && info.type in VALUE_EDITOR_TYPES -> ValueEditor(model, session, target.uid, info, aspect, revision, target.kind)
                info.type != "text" || !info.editable -> Text(
                    if (!info.editable) "This field can't be edited." else "This is a ${info.type} field: editing it here comes later.",
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.padding(8.dp).testTag("not-editable"),
                )
                else -> TextEditor(model, session, target, info.multiLine, revision, inline.takeIf { target.kind == TargetKind.Card })
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
private fun TextEditor(model: SetViewModel, session: Long, target: EditTarget, multiLine: Boolean, revision: Int, onCard: OnCardEditing? = null) {
    val scope = rememberCoroutineScope()
    val holder = remember(target) { CoreText() }
    var value by remember(target) { mutableStateOf<TextFieldValue?>(null) }
    val focus = remember { FocusRequester() }
    // a combined field's parts (Magic's text: rules, then flavor; #83)
    var parts by remember(target) { mutableStateOf<List<IntRange>>(emptyList()) }
    fun show(t: TextResult) {
        holder.sync.answered(t.text, t.selectionEnd)
        holder.locked = t.locked
        parts = t.parts
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
    if (onCard != null) {
        // the caret on the Card follows this field's, and a tap on the Card moves it (#83)
        SideEffect { onCard.caret = CodePoints.fromUtf16(current.text, current.selection.end) }
        LaunchedEffect(onCard.tapped) {
            val to = onCard.tapped ?: return@LaunchedEffect
            onCard.tapped = null
            val shown = value ?: return@LaunchedEffect
            val snapped = if (shown.text == holder.sync.core) LockedSpans.snap(to, holder.locked) else to
            val at = CodePoints.toUtf16(shown.text, snapped)
            value = shown.copy(selection = TextRange(at))
            focus.requestFocus()
        }
    }
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
        // on the Card (#83): still here for the keyboard, but not seen
        modifier = (if (onCard != null) Modifier.size(1.dp).alpha(0f) else Modifier.fillMaxWidth()).focusRequester(focus).testTag("field-text"),
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
    // the parts of a combined field, to put the caret in one: an empty flavor text has no place on
    // the Card to tap, so this is the way into it
    if (parts.size > 1) {
        val caret = CodePoints.fromUtf16(current.text, current.selection.end)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.testTag("text-parts")) {
            for ((i, part) in parts.withIndex()) {
                FilterChip(
                    selected = caret in part.first..part.last + 1,
                    onClick = {
                        val shown = value ?: return@FilterChip
                        val at = CodePoints.toUtf16(shown.text, part.last + 1)
                        value = shown.copy(selection = TextRange(at))
                        focus.requestFocus()
                    },
                    label = { Text(partLabel(target.field, i, parts.size)) },
                    modifier = Modifier.testTag("text-part-$i"),
                )
            }
        }
    }
    // on the Card the symbols wait behind a button, so the Card keeps the space (#83)
    var showSymbols by remember(target) { mutableStateOf(onCard == null) }
    if (onCard != null && symbols?.hasSymbols == true) {
        TextButton(onClick = { showSymbols = !showSymbols }, modifier = Modifier.testTag("symbols-toggle")) {
            Text(if (showSymbols) "Hide symbols" else "Symbols…")
        }
    }
    symbols?.takeIf { it.hasSymbols && showSymbols }?.let { menu ->
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

/** A combined field's part: Magic's text field is rules then flavor; other fields' parts are numbered */
fun partLabel(field: String, index: Int, count: Int): String =
    if (field == "text" && count == 2) listOf("Rules", "Flavor")[index] else "Part ${index + 1}"

/** The text as the core last answered it, which edits are diffed against */
private class CoreText {
    val sync = TextSync()
    var locked: List<IntRange> = emptyList()
    val mutex = Mutex()
}

/** Buttons that insert a keyword parameter of each of the Game's types */
@Composable
private fun KeywordParameterBar(session: Long, onInsert: (String) -> Unit) {
    val types by androidx.compose.runtime.produceState(emptyList<io.github.type_safe_games.mobileseteditor.core.KeywordParamType>(), session) {
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

/**
 * Taps and long presses, but never the start or end of a swipe (#89). detectTapGestures only cancels
 * a tap when something else consumes the movement, so a slow or short swipe the pager doesn't take
 * still ended as a tap and opened a field. Here any movement past the touch slop cancels it, and so
 * does a tap while [ignore] says the pager is still moving.
 */
private suspend fun PointerInputScope.detectTapsNotSwipes(onLongPress: () -> Unit, ignore: () -> Boolean, onTap: (Offset) -> Unit) =
    awaitEachGesture {
        val down = awaitFirstDown(requireUnconsumed = false)
        if (ignore()) return@awaitEachGesture
        val slop = viewConfiguration.touchSlop
        var swiped = false
        var up: PointerInputChange? = null
        // null: still held when the long-press timeout ran out
        val ended = withTimeoutOrNull(viewConfiguration.longPressTimeoutMillis) {
            while (true) {
                val change = awaitPointerEvent().changes.firstOrNull { it.id == down.id } ?: break
                if ((change.position - down.position).getDistance() > slop) swiped = true
                if (change.changedToUp()) { up = change; break }
                if (!change.pressed) break
            }
            true
        }
        if (swiped) return@awaitEachGesture
        if (ended == null) {
            onLongPress()
            // swallow the rest of the press
            do {
                val event = awaitPointerEvent()
                event.changes.forEach { if (it.id == down.id) it.consume() }
            } while (event.changes.any { it.id == down.id && it.pressed })
        } else {
            up?.let { if (!ignore()) onTap(it.position) }
        }
    }
