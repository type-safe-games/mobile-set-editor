// Mobile Set Editor - the core, on its own thread (ADR 0009)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

import android.graphics.Bitmap
import android.os.ParcelFileDescriptor
import java.io.File
import java.util.concurrent.Executors
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.withContext

/**
 * The core: one per process, all of its calls on one thread that this object owns (ADR 0009).
 *
 * Every function suspends while the core thread runs the call, and returns the call's result with
 * the messages the core queued. Sessions are opaque handles; cards are named by uid.
 */
object Core {
    /** The core thread: a single thread, so calls run one at a time and in order */
    val dispatcher: CoroutineDispatcher =
        Executors.newSingleThreadExecutor { Thread(it, "mse-core") }.asCoroutineDispatcher()

    /** Start the core with a template store and a shared font store (call once, first) */
    suspend fun init(templateDir: File, fontStoreDir: File, programDataDir: File? = null, locale: String = ""): CoreResult =
        withContext(dispatcher) {
            NativeCore.init(templateDir.path, fontStoreDir.path, programDataDir?.path ?: "", locale)
        }

    suspend fun shutdown(): CoreResult = withContext(dispatcher) { NativeCore.shutdown() }

    /** Open a Set from a file the host opened (e.g. from the Storage Access Framework); edits go
     *  to [workingCopyDir], the file itself is never written */
    suspend fun openSession(file: ParcelFileDescriptor, workingCopyDir: File): OpenResult =
        withContext(dispatcher) { NativeCore.openSessionFd(file.fd, workingCopyDir.path) }

    suspend fun closeSession(session: Long): CoreResult = withContext(dispatcher) { NativeCore.closeSession(session) }

    /** Reopen a working copy an earlier session left; [unsaved] if it differs from the user's file */
    /** A new Set from a Game and one of its Styles (package names), in a Working copy, as the desktop's New set */
    suspend fun newSession(game: String, stylesheet: String, workingCopyDir: File): OpenResult =
        withContext(dispatcher) { NativeCore.newSession(game, stylesheet, workingCopyDir.path) }

    suspend fun openWorkingCopy(workingCopyDir: File, unsaved: Boolean): OpenResult =
        withContext(dispatcher) { NativeCore.openWorkingCopy(workingCopyDir.path, unsaved) }

    /** Autosave the working copy (cheap when nothing changed) */
    suspend fun flush(session: Long): CoreResult = withContext(dispatcher) { NativeCore.flush(session) }

    /** Write the Set as an .mse-set to [file] (opened for writing) */
    suspend fun saveTo(session: Long, file: ParcelFileDescriptor): ActionResult =
        withContext(dispatcher) { NativeCore.saveToFd(session, file.fd) }

    suspend fun sessionInfo(session: Long): SessionInfo = withContext(dispatcher) { NativeCore.sessionInfo(session) }

    suspend fun getValue(session: Long, kind: TargetKind, cardUid: String, field: String): ValueResult =
        withContext(dispatcher) { NativeCore.getValue(session, kind.ordinal, cardUid, field) }

    suspend fun setValue(session: Long, kind: TargetKind, cardUid: String, field: String, value: String): ActionResult =
        withContext(dispatcher) { NativeCore.setValue(session, kind.ordinal, cardUid, field, value) }

    suspend fun listFields(session: Long, kind: TargetKind, cardUid: String): FieldListResult =
        withContext(dispatcher) { NativeCore.listFields(session, kind.ordinal, cardUid) }

    suspend fun getText(session: Long, kind: TargetKind, cardUid: String, field: String): TextResult =
        withContext(dispatcher) { NativeCore.getText(session, kind.ordinal, cardUid, field) }

    /** Replace [start, end) of a text field's visible text, positions in code points, as typing does */
    suspend fun replaceRange(
        session: Long, kind: TargetKind, cardUid: String, field: String, start: Int, end: Int, text: String, asSymbol: Boolean = false,
    ): TextResult = withContext(dispatcher) { NativeCore.replaceRange(session, kind.ordinal, cardUid, field, start, end, text, asSymbol) }

    /** Add a new Card at [position] (-1: at the end), one undo step */
    suspend fun addCard(session: Long, position: Int = -1): NewCardsResult = withContext(dispatcher) { NativeCore.addCard(session, position) }

    /** Duplicate Cards (and the Cards linked to them), one undo step; the copies follow the last of them */
    suspend fun duplicateCards(session: Long, uids: List<String>): NewCardsResult =
        withContext(dispatcher) { NativeCore.duplicateCards(session, uids.toTypedArray()) }

    /** Remove Cards, one undo step */
    suspend fun removeCards(session: Long, uids: List<String>): ActionResult =
        withContext(dispatcher) { NativeCore.removeCards(session, uids.toTypedArray()) }

    /** Move Cards (in their Set order) so that the first of them is at [position], one undo step */
    suspend fun moveCards(session: Long, uids: List<String>, position: Int): ActionResult =
        withContext(dispatcher) { NativeCore.moveCards(session, uids.toTypedArray(), position) }

    /** The Set's keywords, then the Game's (read-only), and the Game's parameter types */
    suspend fun listKeywords(session: Long): KeywordListResult = withContext(dispatcher) { NativeCore.listKeywords(session) }

    /** Add an empty keyword to the Set, one undo step */
    suspend fun addKeyword(session: Long): NewKeywordResult = withContext(dispatcher) { NativeCore.addKeyword(session) }

    /** Remove one of the Set's keywords, one undo step */
    suspend fun removeKeyword(session: Long, keyword: Long): ActionResult = withContext(dispatcher) { NativeCore.removeKeyword(session, keyword) }

    /** Replace [start, end) of a keyword's match (code points) with a parameter, one undo step */
    suspend fun insertKeywordParameter(session: Long, keyword: Long, start: Int, end: Int, type: String): TextResult =
        withContext(dispatcher) { NativeCore.insertKeywordParameter(session, keyword, start, end, type) }

    /**
     * A Card as the finished PNG the desktop exports: [zoom] (null: the Style's own), [bleed] (null:
     * the desktop's setting), with the <mse-card-data> metadata unless [metadata] is false
     */
    suspend fun exportCardImage(session: Long, cardUid: String, zoom: Double? = null, bleed: Boolean? = null, metadata: Boolean = true): BytesResult =
        withContext(dispatcher) { NativeCore.exportCardImage(session, cardUid, zoom ?: 0.0, bleed?.let { if (it) 1 else 0 } ?: -1, metadata) }

    /** Give a Card a Style of its own ("" or the Set's: follow the Set's again), one undo step */
    suspend fun setCardStyle(session: Long, cardUid: String, stylesheet: String): ActionResult =
        withContext(dispatcher) { NativeCore.setCardStyle(session, cardUid, stylesheet) }

    /** Change the Set's Style, one undo step */
    suspend fun setSetStyle(session: Long, stylesheet: String): ActionResult = withContext(dispatcher) { NativeCore.setSetStyle(session, stylesheet) }

    /** Switch a Card's own style options on or off, one undo step */
    suspend fun setCardOwnStyling(session: Long, cardUid: String, own: Boolean): ActionResult =
        withContext(dispatcher) { NativeCore.setCardOwnStyling(session, cardUid, own) }

    /** The Game's card link types, each a pair of relations (the card's own, the linked card's) */
    suspend fun listLinkTypes(session: Long): LinkTypeListResult = withContext(dispatcher) { NativeCore.listLinkTypes(session) }

    /** Link two Cards, one undo step: [cardUid] is [relation], [otherUid] is [otherRelation] */
    suspend fun link(session: Long, cardUid: String, otherUid: String, relation: String, otherRelation: String): ActionResult =
        withContext(dispatcher) { NativeCore.link(session, cardUid, otherUid, relation, otherRelation) }

    /** Remove the link between two Cards, both ways, one undo step */
    suspend fun unlink(session: Long, cardUid: String, otherUid: String): ActionResult =
        withContext(dispatcher) { NativeCore.unlink(session, cardUid, otherUid) }

    suspend fun undo(session: Long): ActionResult = withContext(dispatcher) { NativeCore.undo(session) }
    suspend fun redo(session: Long): ActionResult = withContext(dispatcher) { NativeCore.redo(session) }

    suspend fun listCards(session: Long): CardListResult = withContext(dispatcher) { NativeCore.listCards(session) }

    suspend fun cardSize(session: Long, cardUid: String): CardSizeResult =
        withContext(dispatcher) { NativeCore.cardSize(session, cardUid) }

    /** Draw a card into [bitmap] (ARGB_8888, the card's aspect ratio), returning its field regions */
    /** A text field's insert-symbol menu (its symbol font's) */
    suspend fun symbolMenu(session: Long, kind: TargetKind, cardUid: String, field: String): SymbolMenuResult =
        withContext(dispatcher) { NativeCore.symbolMenu(session, kind.ordinal, cardUid, field) }

    /** The symbol a menu item inserts, about [size] pixels square */
    suspend fun symbolPreview(session: Long, kind: TargetKind, cardUid: String, field: String, itemId: Int, size: Int): SymbolPreviewResult =
        withContext(dispatcher) { NativeCore.symbolPreview(session, kind.ordinal, cardUid, field, itemId, size) }

    /** The field a tap at (x, y) on a render [width] pixels wide picks, as the desktop picks it ("" for none) */
    suspend fun fieldAt(session: Long, cardUid: String, x: Double, y: Double, width: Int): FieldAtResult =
        withContext(dispatcher) { NativeCore.fieldAt(session, cardUid, x, y, width) }

    suspend fun renderCard(session: Long, cardUid: String, bitmap: Bitmap): RenderResult =
        withContext(dispatcher) { NativeCore.renderCard(session, cardUid, bitmap) }

    /** The installed Templates, by name */
    suspend fun listTemplates(): TemplateListResult = withContext(dispatcher) { NativeCore.listTemplates() }

    /** After an import: new sessions see the new Templates and fonts */
    suspend fun refreshTemplates(): CoreResult = withContext(dispatcher) { NativeCore.refreshTemplates() }
}
