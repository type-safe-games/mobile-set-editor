// Mobile Set Editor - text in a font nobody installed falls back to Noto Sans, shipped with the app (#91)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.graphics.Bitmap
import android.graphics.Color
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.Stores
import io.github.type_safe_games.mobileseteditor.core.TargetKind
import java.io.File

@RunWith(AndroidJUnit4::class)
class FallbackFontTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    /** How many pixels of a region are dark: text, on the fixture's light name bar */
    private fun ink(bitmap: Bitmap, x: Int, y: Int, w: Int, h: Int): Int {
        val px = IntArray(w * h).also { bitmap.getPixels(it, 0, w, x, y, w, h) }
        return px.count { Color.red(it) + Color.green(it) + Color.blue(it) < 200 }
    }

    @Test
    fun aStyleWhoseFontIsMissingStillShowsItsText() = runBlocking {
        // the fixture's standard Style, with every font a family that isn't installed anywhere
        val templates = Stores.templates(TestFixture.app)
        val from = File(templates, "fixture-standard.mse-style")
        val to = File(templates, "fixture-nofont.mse-style")
        to.deleteRecursively()
        from.copyRecursively(to)
        val style = File(to, "style")
        style.writeText(style.readText().replace("DejaVu Sans", "No Such Family"))
        assertTrue(Core.refreshTemplates().ok)

        val dir = File(TestFixture.work, "nofont").also { it.deleteRecursively() }
        val open = Core.newSession("fixture.mse-game", "fixture-nofont.mse-style", dir)
        assertTrue("open: ${open.result}", open.result.ok)
        val session = open.session
        try {
            val uid = Core.addCard(session).uids.single()
            assertTrue(Core.setValue(session, TargetKind.Card, uid, "name", "Fallback").result.ok)
            val size = Core.cardSize(session, uid)
            val bitmap = Bitmap.createBitmap(size.width.toInt(), size.height.toInt(), Bitmap.Config.ARGB_8888)
            val render = Core.renderCard(session, uid, bitmap)
            assertTrue(render.result.ok)
            val name = render.regions.first { it.field == "name" }
            val dark = ink(bitmap, name.x.toInt(), name.y.toInt(), name.width.toInt(), name.height.toInt())
            assertTrue("the name is drawn (dark pixels: $dark)", dark > 30)
        } finally {
            Core.closeSession(session)
            to.deleteRecursively()
            Core.refreshTemplates()
        }
    }
}
