// Mobile Set Editor - tap a text field on a Card, type, see the render change, undo the word (#62)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.graphics.Bitmap
import android.net.Uri
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertTextContains
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.click
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.performTextInputSelection
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.text.TextRange
import androidx.compose.ui.test.assertIsNotEnabled
import org.junit.Assert.assertTrue
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.FieldRegion
import io.github.type_safe_games.mobileseteditor.core.TargetKind
import io.github.type_safe_games.mobileseteditor.editor.CardEditorScreen

@RunWith(AndroidJUnit4::class)
class CardEditorTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    private fun waitForTag(tag: String) = compose.waitUntil(30_000) {
        compose.onAllNodesWithTag(tag).fetchSemanticsNodes().isNotEmpty()
    }

    /** The pixels of a region of a render */
    private fun pixels(bitmap: Bitmap, r: FieldRegion): IntArray {
        val w = r.width.toInt().coerceAtMost(bitmap.width - r.x.toInt())
        val h = r.height.toInt().coerceAtMost(bitmap.height - r.y.toInt())
        return IntArray(w * h).also { bitmap.getPixels(it, 0, w, r.x.toInt(), r.y.toInt(), w, h) }
    }

    @Test
    fun typingIntoTheNameRendersAndUndoTakesTheWordBack() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { CardEditorScreen(model, onBack = {}) }
        model.open(Uri.fromFile(TestFixture.zip("editor.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.card != null }
        val card = model.state.value.card!!
        val uid = card.uid
        val name = card.regions.first { it.field == "name" }
        val before = pixels(card.bitmap, name)
        val session = model.state.value.session
        val original = runBlocking { Core.getText(session, TargetKind.Card, uid, "name").text }

        // tap the name on the Card: its editor opens
        compose.onNodeWithTag("card-$uid").performTouchInput {
            val scale = width.toFloat() / card.bitmap.width
            click(Offset(((name.x + name.width / 2) * scale).toFloat(), ((name.y + name.height / 2) * scale).toFloat()))
        }
        waitForTag("field-text")
        compose.onNodeWithTag("editing-field").assertTextEquals("Name")
        compose.onNodeWithTag("field-text").assertTextContains(original)

        // type a word, a key at a time, at the start (the end may be clipped by the name's box)
        compose.onNodeWithTag("field-text").performTextInputSelection(TextRange(0))
        for (c in "Edited ") compose.onNodeWithTag("field-text").performTextInput(c.toString())
        compose.waitUntil(10_000) { runBlocking { Core.getText(session, TargetKind.Card, uid, "name").text } == "Edited $original" }
        assertEquals(true, model.state.value.dirty)

        // the Card renders again with it
        compose.waitUntil(30_000) {
            val now = model.state.value.card
            now != null && now.uid == uid && !pixels(now.bitmap, name).contentEquals(before)
        }

        // one undo takes the whole word back
        compose.onNodeWithTag("undo").assertTextContains("Undo", substring = true).performClick()
        compose.waitUntil(10_000) { runBlocking { Core.getText(session, TargetKind.Card, uid, "name").text } == original }
        compose.waitUntil(10_000) { !model.state.value.canUndo }
        assertFalse(model.state.value.canUndo)
        compose.waitUntil(10_000) {
            runCatching { compose.onNodeWithTag("field-text").assertTextEquals(original, includeEditableText = true) }.isSuccess
        }
        compose.onNodeWithTag("redo").assertIsDisplayed()
    }

    @Test
    fun arrowsMoveBetweenCardsAndADragOverAFieldDoesNotOpenIt() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { CardEditorScreen(model, onBack = {}) }
        model.open(Uri.fromFile(TestFixture.zip("arrows.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.card != null }
        val uids = model.state.value.cardUids
        // the arrows (#89): none before the first Card, none after the last
        compose.onNodeWithTag("previous-card").assertIsNotEnabled()
        compose.onNodeWithTag("next-card").performClick()
        compose.waitUntil(10_000) { model.state.value.index == 1 }
        compose.onNodeWithTag("card-position").assertTextContains("2 / 3", substring = true)
        compose.onNodeWithTag("next-card").performClick()
        compose.waitUntil(10_000) { model.state.value.index == 2 }
        compose.onNodeWithTag("next-card").assertIsNotEnabled()
        compose.onNodeWithTag("previous-card").performClick()
        compose.waitUntil(10_000) { model.state.value.index == 1 }

        // a short drag that starts on a field, too short for the pager to turn the page, opens nothing
        compose.waitUntil(30_000) { model.state.value.card?.uid == uids[1] }
        val card = model.state.value.card!!
        val name = card.regions.first { it.field == "name" }
        compose.onNodeWithTag("card-${uids[1]}").performTouchInput {
            val scale = width.toFloat() / card.bitmap.width
            val at = Offset(((name.x + name.width / 2) * scale).toFloat(), ((name.y + name.height / 2) * scale).toFloat())
            down(at)
            for (step in 1..6) moveBy(Offset(-viewConfiguration.touchSlop / 2, 0f), delayMillis = 30)
            up()
        }
        compose.waitForIdle()
        assertTrue(compose.onAllNodesWithTag("field-sheet").fetchSemanticsNodes().isEmpty())
        // while a plain tap there still does
        compose.onNodeWithTag("card-${uids[1]}").performTouchInput {
            val scale = width.toFloat() / card.bitmap.width
            click(Offset(((name.x + name.width / 2) * scale).toFloat(), ((name.y + name.height / 2) * scale).toFloat()))
        }
        waitForTag("field-sheet")
    }

    @Test
    fun theStyleChipChangesTheCardsStyle() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { CardEditorScreen(model, onBack = {}) }
        model.open(Uri.fromFile(TestFixture.zip("style-chip.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.card != null }
        assertEquals(false, model.state.value.cardStyles[0].own)
        waitForTag("card-style-chip")
        compose.onNodeWithTag("card-style-chip").performClick()
        waitForTag("style-fixture-alternate.mse-style")
        compose.onNodeWithTag("style-fixture-alternate.mse-style").performClick()
        compose.waitUntil(10_000) { model.state.value.cardStyles[0].stylesheet == "fixture-alternate.mse-style" }
        assertTrue(model.state.value.cardStyles[0].own)
    }

    @Test
    fun flipJumpsToTheLinkedFaceAndADoubleWideCardShowsAFaceAtATime() {
        // a double-wide copy of the fixture's Style: both faces side by side in one Card (#90)
        val templates = io.github.type_safe_games.mobileseteditor.core.Stores.templates(TestFixture.app)
        val wide = java.io.File(templates, "fixture-wide.mse-style")
        wide.deleteRecursively()
        java.io.File(templates, "fixture-standard.mse-style").copyRecursively(wide)
        java.io.File(wide, "style").let { it.writeText(it.readText().replace("card width: 300", "card width: 840")) }
        assertTrue(runBlocking { Core.refreshTemplates() }.ok)
        try {
            val model = SetViewModel(TestFixture.app.applicationContext as Application)
            compose.setContent { CardEditorScreen(model, onBack = {}) }
            model.open(Uri.fromFile(TestFixture.zip("flip.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
            compose.waitUntil(30_000) { model.state.value.card != null }
            val uids = model.state.value.cardUids

            // a Front Face linked to its Back Face: Flip goes there
            compose.onNodeWithTag("next-card").performClick()
            compose.waitUntil(10_000) { model.state.value.index == 1 }
            waitForTag("flip-${uids[1]}")
            compose.onNodeWithTag("flip-${uids[1]}").performClick()
            compose.waitUntil(10_000) { model.state.value.index == 2 }

            // the first Card in the double-wide Style: its front, then its back
            compose.onNodeWithTag("previous-card").performClick()
            compose.onNodeWithTag("previous-card").performClick()
            compose.waitUntil(10_000) { model.state.value.index == 0 }
            runBlocking { model.setCardStyle(uids[0], "fixture-wide.mse-style") }
            val size = runBlocking { Core.cardSize(model.state.value.session, uids[0]) }
            compose.waitUntil(10_000) { model.state.value.cardStyles[0].stylesheet == "fixture-wide.mse-style" }
            assertTrue("size ${size.width} x ${size.height}, ${size.result}; style ${model.state.value.cardStyles[0].stylesheet}", size.width / size.height > 1.2)
            waitForTag("card-face-front")
            compose.onNodeWithTag("flip-${uids[0]}").performClick()
            waitForTag("card-face-back")
            compose.onNodeWithTag("flip-${uids[0]}").performClick()
            waitForTag("card-face-front")
        } finally {
            wide.deleteRecursively()
            runBlocking { Core.refreshTemplates() }
        }
    }
}
