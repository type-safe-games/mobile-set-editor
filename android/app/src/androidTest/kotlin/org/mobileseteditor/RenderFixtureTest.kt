// Mobile Set Editor - render the Test fixture through the JNI layer (#45)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.content.res.AssetManager
import android.graphics.Bitmap
import android.os.ParcelFileDescriptor
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.Status

/**
 * The Test fixture (test/fixture, in this test APK's assets only, ADR 0008) installed as the
 * template store and font store, its Set opened from a file descriptor and a Card rendered into a
 * Bitmap through the Skia Canvas.
 */
@RunWith(AndroidJUnit4::class)
class RenderFixtureTest {
    companion object {
        private val testAssets: AssetManager get() = InstrumentationRegistry.getInstrumentation().context.assets
        private val root: File get() = File(InstrumentationRegistry.getInstrumentation().targetContext.filesDir, "fixture-test")

        /** The fixture Set's files, as the text of its "set" file can be changed */
        private fun setFiles(): Map<String, ByteArray> =
            testAssets.list("sets/fixture-cards.mse-set")!!.associateWith { name ->
                testAssets.open("sets/fixture-cards.mse-set/$name").use { it.readBytes() }
            }

        @BeforeClass
        @JvmStatic
        fun installFixture() {
            root.deleteRecursively()
            copyAssets("data", File(root, "templates"))
            copyAssets("fonts", File(root, "fonts"))
            val init = runBlocking { Core.init(File(root, "templates"), File(root, "fonts")) }
            assertTrue("init: $init", init.ok)
        }

        private fun copyAssets(path: String, to: File) {
            val children = testAssets.list(path) ?: emptyArray()
            if (children.isEmpty()) {
                to.parentFile!!.mkdirs()
                testAssets.open(path).use { input -> to.outputStream().use { input.copyTo(it) } }
            } else {
                to.mkdirs()
                for (child in children) copyAssets("$path/$child", File(to, child))
            }
        }

        /** A zipped .mse-set with these files */
        private fun zipSet(name: String, files: Map<String, ByteArray>): File {
            val file = File(root, name)
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

    @Test
    fun rendersAFixtureCard() = runBlocking {
        val setFile = zipSet("fixture-cards.mse-set", setFiles())
        val open = ParcelFileDescriptor.open(setFile, ParcelFileDescriptor.MODE_READ_ONLY).use { fd ->
            Core.openSession(fd, File(root, "work/fixture-cards"))
        }
        assertTrue("open: ${open.result} ${open.result.messages.toList()}", open.result.ok)

        val cards = Core.listCards(open.session)
        assertTrue("listCards: ${cards.result}", cards.result.ok)
        assertEquals(3, cards.uids.size)
        val uid = cards.uids[0]

        val size = Core.cardSize(open.session, uid)
        assertTrue("cardSize: ${size.result}", size.result.ok && size.width > 0 && size.height > 0)
        val bitmap = Bitmap.createBitmap(size.width.toInt(), size.height.toInt(), Bitmap.Config.ARGB_8888)
        val render = Core.renderCard(open.session, uid, bitmap)
        assertTrue("renderCard: ${render.result} ${render.result.messages.toList()}", render.result.ok)

        // field regions, and a card that isn't blank
        assertTrue("no field regions", render.regions.isNotEmpty())
        assertTrue("no region has a size", render.regions.any { it.width > 0 && it.height > 0 })
        val pixels = IntArray(bitmap.width * bitmap.height)
        bitmap.getPixels(pixels, 0, bitmap.width, 0, 0, bitmap.width, bitmap.height)
        assertTrue("the card is transparent", pixels.all { (it ushr 24) == 0xFF })
        assertTrue("the card is blank: ${pixels.distinct().size} colours", pixels.distinct().size > 50)

        assertTrue(Core.closeSession(open.session).ok)
    }

    @Test
    fun aMissingTemplateIsAnError() = runBlocking {
        val files = setFiles().toMutableMap()
        val set = String(files.getValue("set"), Charsets.UTF_8)
        assertTrue(set.contains("stylesheet: standard"))
        files["set"] = set.replace("stylesheet: standard", "stylesheet: no-such-style").toByteArray(Charsets.UTF_8)
        val setFile = zipSet("missing-style.mse-set", files)
        val open = ParcelFileDescriptor.open(setFile, ParcelFileDescriptor.MODE_READ_ONLY).use { fd ->
            Core.openSession(fd, File(root, "work/missing-style"))
        }
        assertFalse(open.result.ok)
        assertEquals(Status.MissingTemplate, open.result.status)
        assertTrue("missing package: ${open.result.missingPackage}", open.result.missingPackage.contains("no-such-style"))
        assertEquals(0L, open.session)
    }
}
