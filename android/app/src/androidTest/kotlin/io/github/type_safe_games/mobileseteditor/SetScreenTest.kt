// Mobile Set Editor - open the Test fixture's Set and see its Cards on screen (#48), swiping between them (#62)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeLeft
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.editor.CardEditorScreen

@RunWith(AndroidJUnit4::class)
class SetScreenTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    private fun showSet(name: String, files: Map<String, ByteArray>): SetViewModel {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { CardEditorScreen(model, onBack = {}) }
        model.open(Uri.fromFile(TestFixture.zip(name, files)))
        return model
    }

    private fun waitForTag(tag: String) = compose.waitUntil(30_000) {
        compose.onAllNodesWithTag(tag).fetchSemanticsNodes().isNotEmpty()
    }

    @Test
    fun showsTheCards() {
        val model = showSet("screen-cards.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))
        waitForTag("card-fixture-wrap")
        compose.onNodeWithTag("card-fixture-wrap").assertIsDisplayed()
        var state = model.state.value
        assertEquals(3, state.cardUids.size)
        val first = state.card!!
        assertEquals(0, first.index)
        assertTrue("no field regions", first.regions.isNotEmpty())
        val pixels = IntArray(first.bitmap.width * first.bitmap.height)
        first.bitmap.getPixels(pixels, 0, first.bitmap.width, 0, 0, first.bitmap.width, first.bitmap.height)
        assertTrue("the card is blank", pixels.distinct().size > 50)

        // swipe to the next Card, rendered in its turn
        compose.onNodeWithTag("card-pager").performTouchInput { swipeLeft() }
        compose.waitUntil(30_000) { model.state.value.card?.index == 1 }
        waitForTag("card-fixture-front")
        compose.onNodeWithTag("card-fixture-front").assertIsDisplayed()
        state = model.state.value
        assertEquals(state.cardUids[1], state.card!!.uid)
    }

    @Test
    fun aMissingStyleIsAMessage() {
        val files = TestFixture.assetFiles("sets/fixture-cards.mse-set").toMutableMap()
        files["set"] = String(files.getValue("set"), Charsets.UTF_8)
            .replace("stylesheet: standard", "stylesheet: no-such-style").toByteArray(Charsets.UTF_8)
        val model = showSet("screen-missing.mse-set", files)
        waitForTag("messages")
        compose.onNodeWithTag("messages").assertIsDisplayed()
        val messages = model.state.value.messages
        assertTrue("messages: $messages", messages.any { "fixture-no-such-style.mse-style" in it && "not installed" in it })
        assertEquals(0L, model.state.value.session)
    }
}
