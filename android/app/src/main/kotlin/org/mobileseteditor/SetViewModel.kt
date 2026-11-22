// Mobile Set Editor - an open Set: its Working copy, autosave, Save and Save as, its Cards
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.app.Application
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.OpenableColumns
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import java.io.File
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.mobileseteditor.core.ActionResult
import org.mobileseteditor.core.CardLink
import org.mobileseteditor.core.CardListResult
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.CoreMessage
import org.mobileseteditor.core.CoreResult
import org.mobileseteditor.core.FieldRegion
import org.mobileseteditor.core.LinkType
import org.mobileseteditor.core.OpenResult
import org.mobileseteditor.core.Severity
import org.mobileseteditor.core.Status
import org.mobileseteditor.core.Stores
import org.mobileseteditor.core.TargetKind
import org.mobileseteditor.core.WorkingCopies
import org.mobileseteditor.core.WorkingCopy
import org.mobileseteditor.render.CoreCardRenderer
import org.mobileseteditor.render.RenderKey
import org.mobileseteditor.render.RenderService

/** A Card as rendered for the screen */
class RenderedCard(val index: Int, val uid: String, val name: String, val bitmap: Bitmap, val regions: List<FieldRegion>)

/** What the Set screen shows */
data class SetState(
    val opening: Boolean = false,
    val session: Long = 0,
    /** the open Set's Working copy */
    val workingCopy: WorkingCopy? = null,
    /** has the Set changes its document hasn't? */
    val dirty: Boolean = false,
    val saving: Boolean = false,
    val cardUids: List<String> = emptyList(),
    val cardNames: List<String> = emptyList(),
    /** each Card's links, in the order of [cardUids] */
    val cardLinks: List<List<CardLink>> = emptyList(),
    /** each Card's Style (package name), whether it is the Card's own, whether it has its own style options */
    val cardStyles: List<CardStyleState> = emptyList(),
    val index: Int = 0,
    /** width / height of the Set's Cards (of its first Card's Style), for thumbnail placeholders */
    val aspect: Float = 0.716f,
    /** null while the current Card renders */
    val card: RenderedCard? = null,
    /** what the core said, newest last (errors, warnings) */
    val messages: List<String> = emptyList(),
    val canUndo: Boolean = false,
    val canRedo: Boolean = false,
    /** what Undo and Redo would undo and redo, e.g. "Typing" */
    val undoName: String = "",
    val redoName: String = "",
)

/** A Card's Style: [stylesheet] (package name), [own] if it isn't the Set's, [ownStyling] if it has its own style options */
data class CardStyleState(val stylesheet: String, val own: Boolean, val ownStyling: Boolean)

/** What to do with unsaved changes when a Set is closed */
enum class CloseChoice {
    /** write them to the document first */
    Save,
    /** throw them away, with the Working copy */
    Discard,
    /** keep them in the Working copy, to open again from the start screen */
    Keep,
}

/**
 * The open Set (ADR 0009, CONTEXT.md "Working copy"). Opening a document copies it into a Working
 * copy in app storage (or reuses the one it already has); edits go there and are autosaved a short
 * time after the last one and when the app goes to the background. Save writes the document back;
 * Save as writes a new one. The Working copy outlives the process: the app comes back to it.
 *
 * Cards render through a [RenderService] (#59): the current one first, neighbours ahead.
 */
class SetViewModel(app: Application) : AndroidViewModel(app) {
    companion object {
        /** What a new Set is called until it has a title */
        const val UNTITLED = "Untitled set"
    }

    private val _state = MutableStateFlow(SetState())
    val state: StateFlow<SetState> = _state

    /** How many Cards on each side of the current one are rendered ahead */
    var lookAhead = 2

    /** Autosave this long after the last edit */
    var autosaveDelayMillis = 1_000L

    /** The open Set's render queue and cache (null while none is open) */
    var renders: RenderService<Bitmap>? = null
        private set
    private var rendersScope: CoroutineScope? = null
    private var observeJob: Job? = null
    private var autosaveJob: Job? = null
    private var renderJob: Job? = null
    private var staleAll = false
    private val staleUids = mutableSetOf<String>()
    private var width = 0

    /** The width in pixels the viewer renders Cards at (0 until it is known) */
    val renderWidth get() = width

    private val context get() = getApplication<Application>()

    // ------------------------------------------------------------------- Opening

    /** Open a document the user picked: in its Working copy if it has one, else in a new one */
    fun open(uri: Uri) = viewModelScope.launch {
        closeQuietly()
        _state.value = SetState(opening = true)
        if (!startCore()) return@launch
        // keep the right to read (and write, for Save) the document after a restart
        try {
            context.contentResolver.takePersistableUriPermission(
                uri, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
            )
        } catch (e: SecurityException) {
            try {
                context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            } catch (e: SecurityException) {
                // (a file:// URI, or a provider without persistable grants: fine for this run)
            }
        }
        val existing = WorkingCopies.forUri(context, uri.toString())
        if (existing != null) return@launch openCopy(existing)
        val fd = try {
            context.contentResolver.openFileDescriptor(uri, "r")
        } catch (e: Exception) {
            null
        }
        if (fd == null) {
            _state.value = SetState(messages = listOf("The file could not be opened"))
            return@launch
        }
        val copy = WorkingCopies.create(context, uri.toString(), displayName(uri))
        val open = fd.use { Core.openSession(it, copy.setDir) }
        if (!open.result.ok) {
            WorkingCopies.delete(context, copy)
            return@launch report(open.result, "The Set could not be opened")
        }
        opened(open, copy)
    }

    /**
     * A new Set from a Game and one of its Styles (package names), in a new Working copy named
     * "Untitled set" (until its title says otherwise). It has no document yet: its first Save is a
     * Save as.
     */
    fun newSet(game: String, stylesheet: String) = viewModelScope.launch {
        closeQuietly()
        _state.value = SetState(opening = true)
        if (!startCore()) return@launch
        val copy = WorkingCopies.create(context, null, UNTITLED).copy(unsaved = true).also(WorkingCopies::write)
        val open = Core.newSession(game, stylesheet, copy.setDir)
        if (!open.result.ok) {
            WorkingCopies.delete(context, copy)
            return@launch report(open.result, "The Set could not be made")
        }
        // in the Working copy at once, so it survives the process going away
        Core.flush(open.session)
        opened(open, copy)
    }

    /** Open a Working copy (from the start screen, or the one open when the app went away) */
    fun openWorkingCopy(copy: WorkingCopy) = viewModelScope.launch {
        closeQuietly()
        _state.value = SetState(opening = true)
        if (!startCore()) return@launch
        openCopy(copy)
    }

    /** After process death: reopen the Working copy that was open, if any and none is open now */
    fun restore() {
        if (_state.value.session != 0L || _state.value.opening) return
        val id = WorkingCopies.lastOpen(context) ?: return
        val copy = WorkingCopies.get(context, id) ?: return WorkingCopies.setLastOpen(context, null)
        openWorkingCopy(copy)
    }

    private suspend fun openCopy(copy: WorkingCopy) {
        val open = Core.openWorkingCopy(copy.setDir, copy.unsaved)
        if (!open.result.ok) return report(open.result, "The Working copy could not be opened")
        opened(open, copy)
    }

    private suspend fun opened(open: OpenResult, copy: WorkingCopy) {
        WorkingCopies.setLastOpen(context, copy.id)
        val cards = Core.listCards(open.session)
        val info = Core.sessionInfo(open.session)
        val size = cards.uids.firstOrNull()?.let { Core.cardSize(open.session, it) }
        // the worker off the core thread: each render is then its own dispatch onto it, so
        // calls made meanwhile, such as edits, run between renders
        val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
        rendersScope = scope
        renders = RenderService(CoreCardRenderer(open.session) { addMessages(it) }, CoreCardRenderer.defaultBudget(), scope)
        _state.value = SetState(
            session = open.session,
            workingCopy = copy,
            dirty = info.dirty,
            canUndo = info.canUndo,
            canRedo = info.canRedo,
            aspect = size?.takeIf { it.result.ok && it.height > 0 }?.let { (it.width / it.height).toFloat() } ?: 0.716f,
            cardUids = cards.uids.toList(),
            cardNames = cards.names.toList(),
            cardLinks = cards.links,
            cardStyles = cardStyles(cards),
            messages = messageLines(open.result) + messageLines(cards.result),
        )
        show()
    }

    private suspend fun startCore(): Boolean {
        val start = Stores.startCore(context)
        if (!start.ok) report(start, "The core could not start")
        return start.ok
    }

    // ------------------------------------------------------------------- Editing and autosave

    /**
     * Apply an edit's result: mark the Set unsaved, re-render what it made stale, autosave soon.
     * While typing, [renderDelayMillis] holds the re-render back until the typing pauses (the stale
     * render stays on screen meanwhile).
     */
    fun edited(result: ActionResult, renderDelayMillis: Long = 0) {
        val s = _state.value
        val copy = s.workingCopy ?: return
        addMessages(result.result.messages.toList())
        if (!result.result.ok) return
        staleAll = staleAll || result.stale.all
        staleUids += result.stale.cardUids
        // Cards added, removed or moved: list them again at once
        if (result.stale.cardList) viewModelScope.launch { reloadCards() }
        renderJob?.cancel()
        if (renderDelayMillis <= 0) invalidateStale() else renderJob = viewModelScope.launch {
            delay(renderDelayMillis)
            invalidateStale()
        }
        val updated = copy.copy(lastEdited = System.currentTimeMillis(), unsaved = result.dirty)
        WorkingCopies.write(updated)
        _state.value = _state.value.copy(
            dirty = result.dirty,
            workingCopy = updated,
            canUndo = result.canUndo,
            canRedo = result.canRedo,
            undoName = result.undoName,
            redoName = result.redoName,
        )
        autosaveJob?.cancel()
        autosaveJob = viewModelScope.launch {
            delay(autosaveDelayMillis)
            flushNow()
        }
    }

    private fun invalidateStale() {
        renders?.invalidate(staleAll, staleUids.toList())
        staleAll = false
        staleUids.clear()
        // names and links may have changed with it
        viewModelScope.launch { refreshCards() }
    }

    /** List the Cards again: their names and links (the order and the Cards themselves stay, here) */
    private suspend fun refreshCards() {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        val cards = Core.listCards(session)
        val s = _state.value
        if (!cards.result.ok || s.session != session || cards.uids.toList() != s.cardUids) return
        _state.value = s.copy(cardNames = cards.names.toList(), cardLinks = cards.links, cardStyles = cardStyles(cards))
        nameAfterTitle()
    }

    /**
     * The card list changed (Cards added, removed, moved): list them again. The current Card stays
     * current while it exists; renders of Cards that are gone are dropped.
     */
    private suspend fun reloadCards() {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        val cards = Core.listCards(session)
        val s = _state.value
        if (!cards.result.ok || s.session != session) return
        val uids = cards.uids.toList()
        val current = s.cardUids.getOrNull(s.index)
        val index = uids.indexOf(current).takeIf { it >= 0 } ?: s.index.coerceIn(0, maxOf(0, uids.size - 1))
        val gone = s.cardUids - uids.toSet()
        if (gone.isNotEmpty()) renders?.invalidate(false, gone)
        _state.value = s.copy(
            cardUids = uids,
            cardNames = cards.names.toList(),
            cardLinks = cards.links,
            cardStyles = cardStyles(cards),
            index = index,
            card = if (index == s.index && uids.getOrNull(index) == current) s.card else null,
        )
        show()
    }

    // ------------------------------------------------------------------- Keywords

    /** Add an empty keyword to the Set, one undo step; its id (0 if that failed) */
    suspend fun addKeyword(): Long {
        val session = _state.value.session.takeIf { it != 0L } ?: return 0
        val r = Core.addKeyword(session)
        edited(r.action)
        return if (r.action.result.ok) r.id else 0
    }

    /** Remove one of the Set's keywords, one undo step */
    suspend fun removeKeyword(id: Long) {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        edited(Core.removeKeyword(session, id))
    }

    // ------------------------------------------------------------------- Changing the card list

    /** Add a new Card after [after] (an index; -1: at the start), one undo step; its index, or -1 */
    suspend fun addCard(after: Int): Int {
        val session = _state.value.session.takeIf { it != 0L } ?: return -1
        val r = Core.addCard(session, (after + 1).coerceIn(0, _state.value.cardUids.size))
        edited(r.action)
        reloadCards()
        return r.uids.firstOrNull()?.let { _state.value.cardUids.indexOf(it) } ?: -1
    }

    /** Duplicate Cards, one undo step; the copies' uids */
    suspend fun duplicateCards(uids: List<String>): List<String> {
        val session = _state.value.session.takeIf { it != 0L } ?: return emptyList()
        val r = Core.duplicateCards(session, uids)
        edited(r.action)
        reloadCards()
        return r.uids.toList()
    }

    /** Remove Cards, one undo step */
    suspend fun removeCards(uids: List<String>): Boolean {
        val session = _state.value.session.takeIf { it != 0L } ?: return false
        val r = Core.removeCards(session, uids)
        edited(r)
        reloadCards()
        return r.result.ok
    }

    /** Move Cards so that the first of them (in Set order) is at [position], one undo step */
    suspend fun moveCards(uids: List<String>, position: Int) {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        edited(Core.moveCards(session, uids, position))
        reloadCards()
    }

    /** A Set without a document yet is called by its title, once it has one */
    private suspend fun nameAfterTitle() {
        val s = _state.value
        val copy = s.workingCopy ?: return
        if (copy.uri != null) return
        val title = Core.getValue(s.session, TargetKind.Set, "", "title").takeIf { it.result.ok }?.value?.trim().orEmpty()
        val name = title.ifEmpty { UNTITLED }
        if (name == copy.name) return
        val renamed = copy.copy(name = name)
        WorkingCopies.write(renamed)
        _state.value = _state.value.copy(workingCopy = renamed)
    }

    private fun cardStyles(cards: CardListResult) =
        cards.uids.indices.map { CardStyleState(cards.stylesheets[it], cards.ownStylesheet[it], cards.ownStyling[it]) }

    // ------------------------------------------------------------------- Styles

    /** The Set's Game (package name) */
    suspend fun game(): String {
        val session = _state.value.session.takeIf { it != 0L } ?: return ""
        return Core.sessionInfo(session).game
    }

    /** Give a Card a Style of its own, or ("") make it follow the Set's again; one undo step */
    suspend fun setCardStyle(uid: String, stylesheet: String) {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        edited(Core.setCardStyle(session, uid, stylesheet))
        refreshCards()
    }

    /** Change the Set's Style (every Card that follows it re-renders); one undo step */
    suspend fun setSetStyle(stylesheet: String) {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        edited(Core.setSetStyle(session, stylesheet))
        refreshCards()
    }

    /** Switch a Card's own style options on or off; one undo step */
    suspend fun setOwnStyling(uid: String, own: Boolean) {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        edited(Core.setCardOwnStyling(session, uid, own))
        refreshCards()
    }

    // ------------------------------------------------------------------- Linked cards

    /** The Game's link types */
    suspend fun linkTypes(): List<LinkType> {
        val session = _state.value.session.takeIf { it != 0L } ?: return emptyList()
        return Core.listLinkTypes(session).types
    }

    /** Link two Cards (one undo step): [uid] is [relation] to [other], which is [otherRelation] */
    suspend fun link(uid: String, other: String, relation: String, otherRelation: String): Boolean {
        val session = _state.value.session.takeIf { it != 0L } ?: return false
        val result = Core.link(session, uid, other, relation, otherRelation)
        edited(result)
        refreshCards()
        return result.result.ok
    }

    /** Remove the link between two Cards (one undo step) */
    suspend fun unlink(uid: String, other: String) {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        edited(Core.unlink(session, uid, other))
        refreshCards()
    }

    /** Undo the last edit (one undo step); the result, for an editor to refresh its text */
    suspend fun undo(): ActionResult? = undoOrRedo(Core::undo)

    suspend fun redo(): ActionResult? = undoOrRedo(Core::redo)

    private suspend fun undoOrRedo(call: suspend (Long) -> ActionResult): ActionResult? {
        val session = _state.value.session.takeIf { it != 0L } ?: return null
        return call(session).also {
            edited(it)
            // (names, links and Styles: the caller doesn't wait for them)
            viewModelScope.launch { refreshCards() }
        }
    }

    /** A test-only edit (debug builds): add " (edited)" to the Set's title */
    fun testEdit() = viewModelScope.launch {
        val session = _state.value.session.takeIf { it != 0L } ?: return@launch
        val title = Core.getValue(session, TargetKind.Set, "", "title")
        edited(Core.setValue(session, TargetKind.Set, "", "title", title.value + " (edited)"))
    }

    /** Autosave now (when the app goes to the background) */
    suspend fun flushNow() {
        val session = _state.value.session.takeIf { it != 0L } ?: return
        autosaveJob?.cancel()
        withContext(NonCancellable) { addMessages(Core.flush(session).messages.toList()) }
    }

    // ------------------------------------------------------------------- Saving

    /** Write the Set back to its document; false if it has none (use Save as) or that failed */
    suspend fun save(): Boolean {
        val uri = _state.value.workingCopy?.uri ?: return false
        return writeTo(Uri.parse(uri), newDocument = false)
    }

    /** Write the Set to a new document (from ACTION_CREATE_DOCUMENT); it is the Set's document from then on */
    suspend fun saveAs(uri: Uri): Boolean = writeTo(uri, newDocument = true)

    private suspend fun writeTo(uri: Uri, newDocument: Boolean): Boolean {
        val s = _state.value
        val copy = s.workingCopy ?: return false
        _state.value = s.copy(saving = true)
        try {
            flushNow()
            // the whole file first, so a failure never leaves the document half written
            val temp = File(context.cacheDir, "saving-${copy.id}.mse-set")
            val result = ParcelFileDescriptor.open(
                temp, ParcelFileDescriptor.MODE_CREATE or ParcelFileDescriptor.MODE_TRUNCATE or ParcelFileDescriptor.MODE_WRITE_ONLY,
            ).use { Core.saveTo(s.session, it) }
            if (!result.result.ok) {
                report(result.result, "The Set could not be saved")
                return false
            }
            val copied = withContext(Dispatchers.IO) {
                try {
                    context.contentResolver.openOutputStream(uri, "wt")?.use { out -> temp.inputStream().use { it.copyTo(out) } } != null
                } catch (e: Exception) {
                    false
                } finally {
                    temp.delete()
                }
            }
            if (!copied) {
                _state.value = _state.value.copy(messages = _state.value.messages + "The document could not be written")
                return false
            }
            if (newDocument) {
                try {
                    context.contentResolver.takePersistableUriPermission(
                        uri, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                    )
                } catch (e: SecurityException) {
                }
            }
            val updated = copy.copy(
                uri = if (newDocument) uri.toString() else copy.uri,
                name = if (newDocument) displayName(uri) else copy.name,
                unsaved = result.dirty,
            )
            WorkingCopies.write(updated)
            _state.value = _state.value.copy(workingCopy = updated, dirty = result.dirty)
            return true
        } finally {
            _state.value = _state.value.copy(saving = false)
        }
    }

    // ------------------------------------------------------------------- Closing

    /** Close the Set; with unsaved changes, [choice] says what happens to them */
    fun close(choice: CloseChoice, then: () -> Unit = {}) = viewModelScope.launch {
        val s = _state.value
        val copy = s.workingCopy
        if (s.dirty && choice == CloseChoice.Save && !save()) return@launch // (stays open: the message says why)
        closeQuietly()
        if (copy != null) {
            if (choice == CloseChoice.Discard && s.dirty) WorkingCopies.delete(context, copy)
            WorkingCopies.setLastOpen(context, null)
        }
        _state.value = SetState()
        then()
    }

    private suspend fun closeQuietly() {
        observeJob?.cancel()
        autosaveJob?.cancel()
        renderJob?.cancel()
        staleAll = false
        staleUids.clear()
        rendersScope?.cancel()
        rendersScope = null
        renders = null
        val session = _state.value.session
        if (session != 0L) withContext(NonCancellable) {
            Core.flush(session)
            Core.closeSession(session)
        }
    }

    // ------------------------------------------------------------------- Cards

    /** The width in pixels Cards are rendered at (the screen's) */
    fun setWidth(pixels: Int) {
        if (pixels > 0 && pixels != width) {
            width = pixels
            show()
        }
    }

    fun previous() = go(_state.value.index - 1)
    fun next() = go(_state.value.index + 1)

    fun go(index: Int) {
        val s = _state.value
        if (index !in s.cardUids.indices || index == s.index) return
        _state.value = s.copy(index = index, card = null)
        show()
    }

    // ------------------------------------------------------------------- Thumbnails (the Set home grid)

    /** The render of a Card at a width, as state, for the grid's cells */
    fun observe(key: RenderKey) = renders?.observe(key)

    /**
     * The grid shows the Cards at [visible] (indices) and will soon show [ahead], thumbnails
     * [width] pixels wide: render those, visible first; pending renders of others are dropped.
     */
    fun wantThumbnails(visible: List<Int>, ahead: List<Int>, width: Int) {
        val s = _state.value
        val service = renders ?: return
        if (width <= 0) return
        fun keys(indices: List<Int>) = indices.filter { it in s.cardUids.indices }.map { RenderKey(s.cardUids[it], width) }
        service.want(keys(visible), keys(ahead))
    }

    /** The viewer is shown again: want its Card (and neighbours) again */
    fun showCard() = show()

    /** Want the current Card and its neighbours, and show the current one's render */
    private fun show() {
        val s = _state.value
        val service = renders ?: return
        if (s.cardUids.isEmpty() || width <= 0) return
        val index = s.index
        val key = RenderKey(s.cardUids[index], width)
        val ahead = (1..lookAhead).flatMap { listOf(index + it, index - it) }
            .filter { it in s.cardUids.indices }
            .map { RenderKey(s.cardUids[it], width) }
        service.want(listOf(key), ahead)
        observeJob?.cancel()
        observeJob = viewModelScope.launch {
            service.observe(key).collect { r ->
                val current = _state.value
                if (r != null && current.index == index) {
                    _state.value = current.copy(
                        card = RenderedCard(index, key.uid, current.cardNames.getOrElse(index) { "" }, r.image, r.regions),
                    )
                }
            }
        }
    }

    // ------------------------------------------------------------------- Messages

    private fun addMessages(messages: List<CoreMessage>) {
        val lines = messages.filter { it.severity != Severity.Info }.map(CoreMessage::toString)
        if (lines.isNotEmpty()) _state.value = _state.value.copy(messages = _state.value.messages + lines)
    }

    private fun report(result: CoreResult, what: String) {
        val line = when (result.status) {
            Status.MissingTemplate -> "$what: the Template ${result.missingPackage} is not installed"
            else -> "$what: ${result.error}"
        }
        _state.value = _state.value.copy(opening = false, messages = _state.value.messages + messageLines(result) + line)
    }

    private fun messageLines(result: CoreResult): List<String> =
        result.messages.filter { it.severity != Severity.Info }.map(CoreMessage::toString)

    private fun displayName(uri: Uri): String {
        try {
            context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
                if (c.moveToFirst()) c.getString(0)?.let { return it }
            }
        } catch (e: Exception) {
        }
        return uri.lastPathSegment?.substringAfterLast('/') ?: "Set"
    }

    override fun onCleared() {
        rendersScope?.cancel()
        // (viewModelScope is gone by now; the core thread saves and closes it on its own)
        val session = _state.value.session
        if (session != 0L) CoroutineScope(Core.dispatcher).launch {
            Core.flush(session)
            Core.closeSession(session)
        }
    }
}
