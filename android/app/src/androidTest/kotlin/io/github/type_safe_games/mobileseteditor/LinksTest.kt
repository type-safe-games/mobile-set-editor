// Mobile Set Editor - Linked cards: chips jump between faces, unlink and link with one undo step each (#65)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.swipeLeft
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.core.CardLink
import io.github.type_safe_games.mobileseteditor.editor.CardEditorScreen

@RunWith(AndroidJUnit4::class)
class LinksTest {
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

    @Test
    fun chipsJumpAndLinksAreMadeAndUndone() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { CardEditorScreen(model, onBack = {}) }
        model.open(Uri.fromFile(TestFixture.zip("links.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.card != null }
        fun links(uid: String) = model.state.value.let { s -> s.cardLinks[s.cardUids.indexOf(uid)] }
        // the fixture's two faces: the front's link names the back as its Back Face
        assertEquals(listOf(CardLink("fixture-back", "Back Face")), links("fixture-front"))
        assertEquals(listOf(CardLink("fixture-front", "Front Face")), links("fixture-back"))
        assertTrue(links("fixture-wrap").isEmpty())

        // the front's chip jumps to the back, and the back's to the front
        compose.onNodeWithTag("card-pager").performTouchInput { swipeLeft() }
        compose.waitUntil(10_000) { model.state.value.index == 1 }
        waitForTag("link-fixture-back")
        compose.onNodeWithTag("link-fixture-back").performClick()
        compose.waitUntil(10_000) { model.state.value.index == 2 }
        waitForTag("link-fixture-front")
        compose.onNodeWithTag("link-fixture-front").performClick()
        compose.waitUntil(10_000) { model.state.value.index == 1 }

        // unlink, and undo it
        compose.onNodeWithTag("link-menu-fixture-back").performClick()
        compose.onNodeWithTag("unlink-fixture-back").performClick()
        compose.waitUntil(10_000) { links("fixture-front").isEmpty() && links("fixture-back").isEmpty() }
        compose.onNodeWithTag("undo").performClick()
        compose.waitUntil(10_000) { links("fixture-front").size == 1 && links("fixture-back").size == 1 }

        // link the front to the first Card too, as its Front Face (so that Card is a Back Face), and undo
        compose.onNodeWithTag("link-to").performClick()
        compose.onNodeWithTag("link-card-fixture-wrap").performClick()
        compose.onNodeWithTag("link-type-0-false").performClick()
        compose.onNodeWithTag("link-confirm").performClick()
        compose.waitUntil(10_000) { links("fixture-wrap").isNotEmpty() }
        assertEquals(listOf(CardLink("fixture-front", "Front Face")), links("fixture-wrap"))
        assertTrue(CardLink("fixture-wrap", "Back Face") in links("fixture-front"))
        compose.onNodeWithTag("undo").performClick()
        compose.waitUntil(10_000) { links("fixture-wrap").isEmpty() }
        assertEquals(1, links("fixture-front").size)
    }
}
