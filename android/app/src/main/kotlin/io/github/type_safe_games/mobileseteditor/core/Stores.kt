// Mobile Set Editor - where the app keeps Templates and fonts (ADR 0005)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor.core

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
    /** The program's own data (the English locale), copied from the APK's assets at each start */
    fun programData(context: Context) = File(context.noBackupFilesDir, "program-data")

    private const val LOCALE = "en"

    private val mutex = Mutex()
    private var started: CoreResult? = null

    /** Start the core with the app's stores, once per process; later calls return the first result */
    suspend fun startCore(context: Context): CoreResult = mutex.withLock {
        started ?: run {
            val app = context.applicationContext
            templates(app).mkdirs()
            fonts(app).mkdirs()
            val data = programData(app)
            data.deleteRecursively()
            copyAssets(app, "program-data", data)
            addFallbackFonts(app)
            Core.init(templates(app), fonts(app), data, LOCALE).also { started = it }
        }
    }

    /**
     * Noto Sans, the family the font registry falls back to for a font nobody installed (ADR 0004):
     * Android has no system fonts the core can use, so without it such text was not drawn at all
     * (#91). Copied into the font store when missing or changed; a Template import may bring its
     * own copy, which is the same family.
     */
    fun addFallbackFonts(context: Context) {
        val store = fonts(context)
        for (name in context.assets.list(FALLBACK_FONTS).orEmpty().filter { it.endsWith(".ttf") }) {
            val to = File(store, name)
            val bytes = context.assets.open("$FALLBACK_FONTS/$name").use { it.readBytes() }
            if (to.length() != bytes.size.toLong() || !to.readBytes().contentEquals(bytes)) to.writeBytes(bytes)
        }
    }

    private const val FALLBACK_FONTS = "fallback-fonts"

    private fun copyAssets(context: Context, path: String, to: File) {
        val children = context.assets.list(path) ?: emptyArray()
        if (children.isEmpty()) {
            to.parentFile!!.mkdirs()
            context.assets.open(path).use { input -> to.outputStream().use { input.copyTo(it) } }
        } else {
            to.mkdirs()
            for (child in children) copyAssets(context, "$path/$child", File(to, child))
        }
    }
}
