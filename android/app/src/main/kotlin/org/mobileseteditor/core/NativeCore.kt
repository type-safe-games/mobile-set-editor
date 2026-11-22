// Mobile Set Editor - the JNI bindings of the core (ADR 0006)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.core

import android.graphics.Bitmap

/**
 * The facade's calls, implemented in src/main/cpp/mse_jni.cpp (libmse.so).
 *
 * Call these only on the core thread: use [Core], which does. The first call there, [init], makes
 * that thread the core thread (ADR 0009).
 */
internal object NativeCore {
    init {
        System.loadLibrary("mse")
    }

    external fun init(templateDir: String, fontStoreDir: String, programDataDir: String, locale: String): CoreResult
    external fun shutdown(): CoreResult
    /** Open a Set from a file descriptor opened for reading; it is read, not closed */
    external fun openSessionFd(fd: Int, workingCopyDir: String): OpenResult
    external fun closeSession(session: Long): CoreResult
    external fun listCards(session: Long): CardListResult
    external fun cardSize(session: Long, cardUid: String): CardSizeResult
    /** Draw a card into an ARGB_8888 bitmap, scaled to its width */
    external fun renderCard(session: Long, cardUid: String, bitmap: Bitmap): RenderResult

    external fun listTemplates(): TemplateListResult
    /** After an import, so that new sessions see the new Templates and fonts */
    external fun refreshTemplates(): CoreResult

    // Template import: off the core thread (ADR 0009); see TemplateImport
    external fun scanTemplatesFd(fd: Int): ScanResult
    external fun installTemplatesFd(fd: Int, packages: Array<String>, listener: ImportProgressListener?): ImportResult
}
