// Mobile Set Editor - the Test fixture installed for the instrumented tests
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.content.Context
import android.content.res.AssetManager
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertTrue
import org.mobileseteditor.core.Stores

/**
 * The core is one per process and started once, so every test class shares this setup: the app's
 * stores emptied, the Test fixture's Templates and fonts (test/fixture, in this test APK's assets
 * only, ADR 0008) copied into them, and the core started on them.
 */
object TestFixture {
    val testAssets: AssetManager get() = InstrumentationRegistry.getInstrumentation().context.assets
    val app: Context get() = InstrumentationRegistry.getInstrumentation().targetContext
    val work: File get() = File(app.cacheDir, "fixture-test")

    private var started = false

    @Synchronized
    fun start() {
        if (started) return
        Stores.templates(app).deleteRecursively()
        Stores.fonts(app).deleteRecursively()
        work.deleteRecursively()
        copyAssets("data", Stores.templates(app))
        copyAssets("fonts", Stores.fonts(app))
        val init = runBlocking { Stores.startCore(app) }
        assertTrue("init: $init", init.ok)
        started = true
    }

    fun copyAssets(path: String, to: File) {
        val children = testAssets.list(path) ?: emptyArray()
        if (children.isEmpty()) {
            to.parentFile!!.mkdirs()
            testAssets.open(path).use { input -> to.outputStream().use { input.copyTo(it) } }
        } else {
            to.mkdirs()
            for (child in children) copyAssets("$path/$child", File(to, child))
        }
    }

    /** The files of a directory in the assets, by path relative to it */
    fun assetFiles(path: String, prefix: String = ""): Map<String, ByteArray> {
        val children = testAssets.list(path) ?: emptyArray()
        if (children.isEmpty()) return mapOf(prefix.removeSuffix("/") to testAssets.open(path).use { it.readBytes() })
        return children.flatMap { assetFiles("$path/$it", "$prefix$it/").entries }.associate { it.key to it.value }
    }

    /** A zip file with these entries */
    fun zip(name: String, files: Map<String, ByteArray>): File {
        work.mkdirs()
        val file = File(work, name)
        ZipOutputStream(file.outputStream()).use { zip ->
            for ((entry, bytes) in files) {
                zip.putNextEntry(ZipEntry(entry))
                zip.write(bytes)
                zip.closeEntry()
            }
        }
        return file
    }
}
