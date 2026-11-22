// Mobile Set Editor - where the app keeps Templates and fonts (ADR 0005)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.core

import android.content.Context
import java.io.File
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock

/**
 * The template store and the shared font store, in private internal storage. Both are excluded
 * from backup and device transfer (res/xml/data_extraction_rules.xml, res/xml/backup_rules.xml):
 * the user re-imports their Templates (ADR 0005).
 */
object Stores {
    fun templates(context: Context) = File(context.filesDir, "templates")
    fun fonts(context: Context) = File(context.filesDir, "fonts")

    private val mutex = Mutex()
    private var started: CoreResult? = null

    /** Start the core with the app's stores, once per process; later calls return the first result */
    suspend fun startCore(context: Context): CoreResult = mutex.withLock {
        started ?: run {
            val app = context.applicationContext
            templates(app).mkdirs()
            fonts(app).mkdirs()
            Core.init(templates(app), fonts(app)).also { started = it }
        }
    }
}
