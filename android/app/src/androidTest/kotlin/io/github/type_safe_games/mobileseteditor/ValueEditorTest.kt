// Mobile Set Editor - the field sheet's choice, slider, color and image editors on the Test fixture (#63)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.graphics.Bitmap
import android.net.Uri
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.test.click
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.assertIsEnabled
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.File
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.TargetKind
import io.github.type_safe_games.mobileseteditor.editor.CardEditorScreen
import io.github.type_safe_games.mobileseteditor.editor.ImagePicker
import io.github.type_safe_games.mobileseteditor.editor.LocalImagePicker

@RunWith(AndroidJUnit4::class)
class ValueEditorTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    private lateinit var model: SetViewModel
    private var picked: Uri? = null

    /** Open the fixture's Set (its first Card in [style], if given) in the swipe editor */
    private fun open(name: String, style: String? = null) {
        model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent {
            CompositionLocalProvider(LocalImagePicker provides ImagePicker { it(picked) }) {
                CardEditorScreen(model, onBack = {})
            }
        }
        val files = TestFixture.assetFiles("sets/fixture-cards.mse-set").toMutableMap()
        if (style != null) {
            files["set"] = String(files.getValue("set"), Charsets.UTF_8)
                .replace("\tuid: fixture-wrap\n", "\tuid: fixture-wrap\n\tstylesheet: $style\n").toByteArray(Charsets.UTF_8)
        }
        model.open(Uri.fromFile(TestFixture.zip(name, files)))
        compose.waitUntil(30_000) { model.state.value.card != null }
    }

    private val uid get() = model.state.value.card!!.uid
    private val session get() = model.state.value.session

    private fun value(field: String) = runBlocking { Core.getValue(session, TargetKind.Card, uid, field).value }

    private fun waitForValue(field: String, expected: String) = compose.waitUntil(10_000) { value(field) == expected }

    private fun waitForTag(tag: String) = compose.waitUntil(30_000) {
        compose.onAllNodesWithTag(tag).fetchSemanticsNodes().isNotEmpty()
    }

    /** Tap the Card at a point of its Style's own size ([cardWidth] wide), and wait for the sheet */
    private fun tapCard(x: Double, y: Double, cardWidth: Double) {
        val bitmap = model.state.value.card!!.bitmap
        compose.onNodeWithTag("card-$uid").performTouchInput {
            val k = width / cardWidth
            click(Offset((x * k).toFloat(), (y * k).toFloat()))
        }
        waitForTag("field-sheet")
        check(bitmap.width > 0)
    }

    private fun tapField(field: String) {
        val r = model.state.value.card!!.regions.first { it.field == field }
        val bitmap = model.state.value.card!!.bitmap
        compose.onNodeWithTag("card-$uid").performTouchInput {
            val k = width.toDouble() / bitmap.width
            click(Offset(((r.x + r.width / 2) * k).toFloat(), ((r.y + r.height / 2) * k).toFloat()))
        }
        waitForTag("field-sheet")
    }

    private fun undo(field: String, expected: String) {
        compose.onNodeWithTag("undo").performClick()
        waitForValue(field, expected)
    }

    /** The current render's pixel at a point of the card (at its Style's [cardWidth]) */
    private fun pixel(x: Double, y: Double, cardWidth: Double): Int {
        val b = model.state.value.card!!.bitmap
        val k = b.width / cardWidth
        return b.getPixel((x * k).toInt(), (y * k).toInt())
    }

    @Test
    fun choiceAndSlider() {
        open("values-choice.mse-set")
        assertEquals("red", value("frame"))
        val before = pixel(5.0, 5.0, 300.0)
        // the frame: tap the Card's edge, away from the other fields
        tapCard(5.0, 5.0, 300.0)
        compose.onNodeWithTag("editing-field").assertExists()
        waitForTag("choice-blue")
        compose.onNodeWithTag("choice-blue").performClick()
        waitForValue("frame", "blue")
        compose.waitUntil(30_000) { model.state.value.card?.let { pixel(5.0, 5.0, 300.0) != before } == true }
        undo("frame", "red")

        // the level, a slider from 0 to 5
        compose.onNodeWithTag("done").performClick()
        tapField("level")
        assertEquals("3", value("level"))
        waitForTag("slider-plus")
        compose.onNodeWithTag("slider-plus").performClick()
        waitForValue("level", "4")
        compose.onNodeWithTag("slider-value").assertExists()
        undo("level", "3")
        assertTrue(!model.state.value.canUndo)
    }

    @Test
    fun imagePickedAndCropped() {
        // a wide green image to pick
        val file = File(TestFixture.work.apply { mkdirs() }, "picked.png")
        Bitmap.createBitmap(600, 200, Bitmap.Config.ARGB_8888).apply { eraseColor(0xFF00C000.toInt()) }
            .also { b -> file.outputStream().use { b.compress(Bitmap.CompressFormat.PNG, 100, it) } }
        picked = Uri.fromFile(file)
        open("values-image.mse-set")
        val original = value("picture")
        tapField("picture")
        waitForTag("pick-image")
        compose.onNodeWithTag("pick-image").performClick()
        waitForTag("crop-frame")
        // Use is enabled once the picked image is decoded, which can take a while on a busy device
        compose.waitUntil(30_000) { runCatching { compose.onNodeWithTag("crop-use").assertIsEnabled() }.isSuccess }
        compose.onNodeWithTag("crop-use").performClick()
        compose.waitUntil(10_000) { value("picture") != original }
        // the middle of the picture is the picked green
        compose.waitUntil(30_000) {
            val p = model.state.value.card?.let { pixel(150.0, 125.0, 300.0) } ?: 0
            (p shr 8 and 0xFF) > 150 && (p shr 16 and 0xFF) < 60
        }
        undo("picture", original)
        // and removing it
        compose.onNodeWithTag("remove-image").performClick()
        waitForValue("picture", "")
        undo("picture", original)
    }

    @Test
    fun colorAndAnExtraCardField() {
        // the alternate Style adds two extra card fields: a color border and a chapters slider
        open("values-color.mse-set", style = "alternate")
        assertEquals("rgb(0,0,0)", value("border"))
        tapCard(2.0, 200.0, 280.0)
        waitForTag("color-white")
        compose.onNodeWithTag("color-white").performClick()
        waitForValue("border", "rgb(255,255,255)")
        compose.waitUntil(30_000) { model.state.value.card?.let { pixel(1.0, 200.0, 280.0) } == 0xFFFFFFFF.toInt() }
        undo("border", "rgb(0,0,0)")
        compose.onNodeWithTag("done").performClick()
        tapField("chapters")
        waitForTag("slider-minus")
        compose.onNodeWithTag("slider-minus").performClick()
        waitForValue("chapters", "1")
        undo("chapters", "2")
    }
}
