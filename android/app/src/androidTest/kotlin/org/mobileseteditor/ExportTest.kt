// Mobile Set Editor - export a Card as the desktop's PNG: 1x and 2x, with and without bleed, with its metadata (#74)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.graphics.BitmapFactory
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import org.mobileseteditor.core.Core

@RunWith(AndroidJUnit4::class)
class ExportTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    /** Width and height of a PNG */
    private fun size(png: ByteArray): Pair<Int, Int> {
        val o = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeByteArray(png, 0, png.size, o)
        return o.outWidth to o.outHeight
    }

    /** The text of the PNG's tEXt chunk with this keyword, or null */
    private fun text(png: ByteArray, keyword: String): String? {
        var i = 8
        while (i + 8 <= png.size) {
            val length = ((png[i].toInt() and 0xff) shl 24) or ((png[i + 1].toInt() and 0xff) shl 16) or
                ((png[i + 2].toInt() and 0xff) shl 8) or (png[i + 3].toInt() and 0xff)
            val type = String(png, i + 4, 4, Charsets.US_ASCII)
            if (type == "tEXt") {
                val body = String(png, i + 8, length, Charsets.UTF_8)
                if (body.substringBefore('\u0000') == keyword) return body.substringAfter('\u0000')
            }
            i += 12 + length
        }
        return null
    }

    @Test
    fun sizesBleedAndMetadata() = runBlocking<Unit> {
        val file = TestFixture.zip("export.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))
        val work = java.io.File(TestFixture.work, "export-working-copy").apply { deleteRecursively() }
        val open = android.os.ParcelFileDescriptor.open(file, android.os.ParcelFileDescriptor.MODE_READ_ONLY).use { Core.openSession(it, work) }
        assertTrue(open.result.toString(), open.result.ok)
        val s = open.session
        suspend fun export(zoom: Double, bleed: Boolean) = Core.exportCardImage(s, "fixture-wrap", zoom, bleed).also {
            assertTrue(it.result.toString(), it.result.ok)
        }.data
        val one = size(export(1.0, false))
        val two = size(export(2.0, false))
        // the fixture's Style is 300x420 at 150 dpi
        assertEquals(300 to 420, one)
        assertEquals(one.first * 2 to one.second * 2, two)
        // bleed: card dpi / 300 x 36 pixels a side, times the zoom
        val bleed = 150 * 36 / 300
        assertEquals(one.first + 2 * bleed to one.second + 2 * bleed, size(export(1.0, true)))
        assertEquals(two.first + 4 * bleed to two.second + 4 * bleed, size(export(2.0, true)))
        // the metadata the desktop reads back: the Card in a Description text chunk
        val description = text(export(1.0, false), "Description")
        assertTrue("description: $description", description != null && "<mse-card-data>" in description && "Long Wrapping Text" in description)
        Core.closeSession(s)
    }
}
