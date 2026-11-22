// Mobile Set Editor - the core, on its own thread (ADR 0009)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.core

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

    suspend fun undo(session: Long): ActionResult = withContext(dispatcher) { NativeCore.undo(session) }
    suspend fun redo(session: Long): ActionResult = withContext(dispatcher) { NativeCore.redo(session) }

    suspend fun listCards(session: Long): CardListResult = withContext(dispatcher) { NativeCore.listCards(session) }

    suspend fun cardSize(session: Long, cardUid: String): CardSizeResult =
        withContext(dispatcher) { NativeCore.cardSize(session, cardUid) }

    /** Draw a card into [bitmap] (ARGB_8888, the card's aspect ratio), returning its field regions */
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
