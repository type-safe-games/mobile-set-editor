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

    suspend fun listCards(session: Long): CardListResult = withContext(dispatcher) { NativeCore.listCards(session) }

    suspend fun cardSize(session: Long, cardUid: String): CardSizeResult =
        withContext(dispatcher) { NativeCore.cardSize(session, cardUid) }

    /** Draw a card into [bitmap] (ARGB_8888, the card's aspect ratio), returning its field regions */
    suspend fun renderCard(session: Long, cardUid: String, bitmap: Bitmap): RenderResult =
        withContext(dispatcher) { NativeCore.renderCard(session, cardUid, bitmap) }

    /** The installed Templates, by name */
    suspend fun listTemplates(): TemplateListResult = withContext(dispatcher) { NativeCore.listTemplates() }

    /** After an import: new sessions see the new Templates and fonts */
    suspend fun refreshTemplates(): CoreResult = withContext(dispatcher) { NativeCore.refreshTemplates() }
}
