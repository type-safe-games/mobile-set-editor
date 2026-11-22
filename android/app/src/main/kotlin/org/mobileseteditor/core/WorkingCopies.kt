// Mobile Set Editor - the Working copies of the Sets the user opened (CONTEXT.md, ADR 0009)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor.core

import android.content.Context
import java.io.File
import java.util.Properties
import java.util.UUID

/**
 * A Working copy: the app's private, continuously saved copy of a Set the user opened. Edits go
 * to it; the user's document changes only when they save. It outlives the process, so a Set
 * comes back after Android kills the app.
 *
 * Each lives in its own directory under filesDir/working-copies: the Set as a directory (`set`,
 * which the core autosaves) and `meta.properties` with the document it came from, its name, when
 * it was last edited and whether it has changes the document hasn't.
 */
data class WorkingCopy(
    val id: String,
    /** the document (a content URI) it saves to; null for a Set that was never saved */
    val uri: String?,
    val name: String,
    val lastEdited: Long,
    val unsaved: Boolean,
    val root: File,
) {
    /** The Set itself, as the core opens and autosaves it */
    val setDir: File get() = File(root, "set")
}

object WorkingCopies {
    private const val LAST_OPEN = "last-open"

    fun root(context: Context) = File(context.filesDir, "working-copies")

    /** All Working copies, the most recently edited first (ones whose Set never got written are left out) */
    fun list(context: Context): List<WorkingCopy> =
        (root(context).listFiles() ?: emptyArray())
            .mapNotNull { read(it) }
            .filter { it.setDir.isDirectory }
            .sortedByDescending { it.lastEdited }

    fun get(context: Context, id: String): WorkingCopy? = read(File(root(context), id))

    /** The Working copy of a document, if there is one */
    fun forUri(context: Context, uri: String): WorkingCopy? = list(context).firstOrNull { it.uri == uri }

    /** A new, empty Working copy (the core creates its Set directory) */
    fun create(context: Context, uri: String?, name: String): WorkingCopy {
        val dir = File(root(context), UUID.randomUUID().toString())
        dir.mkdirs()
        return WorkingCopy(dir.name, uri, name, System.currentTimeMillis(), false, dir).also(::write)
    }

    fun write(copy: WorkingCopy) {
        val p = Properties()
        copy.uri?.let { p["uri"] = it }
        p["name"] = copy.name
        p["lastEdited"] = copy.lastEdited.toString()
        p["unsaved"] = copy.unsaved.toString()
        val file = File(copy.root, "meta.properties")
        val temp = File(copy.root, "meta.properties.tmp")
        temp.outputStream().use { p.store(it, "Mobile Set Editor working copy") }
        temp.renameTo(file)
    }

    fun delete(context: Context, copy: WorkingCopy) {
        copy.root.deleteRecursively()
        if (lastOpen(context) == copy.id) setLastOpen(context, null)
    }

    /** The Working copy that was open when the app last went away, to come back to */
    fun lastOpen(context: Context): String? =
        context.getSharedPreferences("working-copies", Context.MODE_PRIVATE).getString(LAST_OPEN, null)

    fun setLastOpen(context: Context, id: String?) {
        context.getSharedPreferences("working-copies", Context.MODE_PRIVATE).edit().apply {
            if (id == null) remove(LAST_OPEN) else putString(LAST_OPEN, id)
        }.commit()
    }

    private fun read(dir: File): WorkingCopy? {
        val file = File(dir, "meta.properties")
        if (!file.isFile) return null
        val p = Properties()
        try {
            file.inputStream().use { p.load(it) }
        } catch (e: Exception) {
            return null
        }
        return WorkingCopy(
            id = dir.name,
            uri = p.getProperty("uri"),
            name = p.getProperty("name") ?: dir.name,
            lastEdited = p.getProperty("lastEdited")?.toLongOrNull() ?: dir.lastModified(),
            unsaved = p.getProperty("unsaved") == "true",
            root = dir,
        )
    }
}
