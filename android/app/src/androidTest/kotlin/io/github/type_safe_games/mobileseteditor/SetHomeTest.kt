// Mobile Set Editor - the Set home's grid of thumbnails (#61)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.assertTextContains
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.assertIsEnabled
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.performTouchInput
import androidx.compose.ui.test.longClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.editor.CardEditorScreen

@RunWith(AndroidJUnit4::class)
class SetHomeTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    @Test
    fun theGridShowsEveryCardAndTappingOneOpensIt() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent {
            var card by remember { mutableStateOf<Int?>(null) }
            val grid = rememberLazyGridState()
            if (card == null) {
                SetHomeScreen(model, grid, onOpenCard = { model.go(it); model.showCard(); card = it }, onSaveAs = {}, onClosed = {})
            } else {
                CardEditorScreen(model, onBack = { card = null })
            }
        }
        model.open(Uri.fromFile(TestFixture.zip("home.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.session != 0L }
        val uids = model.state.value.cardUids
        assertEquals(3, uids.size)

        // every Card's thumbnail, rendered (not a placeholder), with its name
        for (uid in uids) compose.onNodeWithTag("thumbnail-$uid").assertIsDisplayed()
        compose.waitUntil(30_000) {
            val renders = model.renders ?: return@waitUntil false
            uids.all { uid -> renders.cachedKeys.any { it.uid == uid && it.width < 600 } }
        }
        for (name in model.state.value.cardNames) compose.onNodeWithText(name).assertIsDisplayed()
        // Linked cards have a badge
        compose.onNodeWithTag("linked-fixture-front", useUnmergedTree = true).assertExists()
        compose.onNodeWithTag("linked-fixture-back", useUnmergedTree = true).assertExists()
        assertTrue(compose.onAllNodesWithTag("linked-fixture-wrap", useUnmergedTree = true).fetchSemanticsNodes().isEmpty())
        compose.onNodeWithText("Set info").assertIsDisplayed()
        compose.onNodeWithText("Keywords").assertIsDisplayed()

        // tapping the third opens it
        compose.onNodeWithTag("thumbnail-${uids[2]}").performClick()
        compose.waitUntil(30_000) { model.state.value.card?.index == 2 }
        compose.waitUntil(30_000) { compose.onAllNodesWithTag("card-${uids[2]}").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithTag("card-${uids[2]}").assertIsDisplayed()
        compose.onNodeWithTag("card-position").assertTextContains("3 / 3", substring = true)
        // and back to the grid
        compose.onNodeWithTag("back-to-cards").performClick()
        compose.onNodeWithTag("card-grid").assertIsDisplayed()
        assertNotNull(model.renders)
    }

    @Test
    fun theAppBarHasUndoRedoSaveAndAMenu() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        var savedAs = false
        var closed = false
        compose.setContent { SetHomeScreen(model, rememberLazyGridState(), onOpenCard = {}, onSaveAs = { savedAs = true }, onClosed = { closed = true }) }
        model.open(Uri.fromFile(TestFixture.zip("app-bar.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.session != 0L }
        compose.onNodeWithTag("set-name").assertTextContains("app-bar")
        compose.onNodeWithTag("home-undo").assertIsDisplayed().assertIsNotEnabled()
        compose.onNodeWithTag("home-redo").assertIsDisplayed().assertIsNotEnabled()
        // nothing to save until an edit
        assertTrue(compose.onAllNodesWithTag("save").fetchSemanticsNodes().isEmpty())
        model.testEdit()
        compose.waitUntil(10_000) { model.state.value.dirty }
        compose.onNodeWithText("Unsaved changes").assertIsDisplayed()
        compose.onNodeWithTag("save").assertIsDisplayed()
        compose.onNodeWithTag("home-undo").assertIsEnabled()
        // Save as, Export all and Close are in the menu
        compose.onNodeWithTag("set-menu").performClick()
        for (item in listOf("Save", "Save as…", "Export all…", "Close")) compose.onNodeWithText(item).assertIsDisplayed()
        compose.onNodeWithText("Save as…").performClick()
        assertTrue(savedAs)
        assertTrue(!closed)
    }

    @Test
    fun theCardsAsAList() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        var opened = -1
        compose.setContent { SetHomeScreen(model, rememberLazyGridState(), onOpenCard = { opened = it }, onSaveAs = {}, onClosed = {}) }
        model.open(Uri.fromFile(TestFixture.zip("list.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.session != 0L }
        val uids = model.state.value.cardUids
        compose.onNodeWithTag("cards-list-view").performClick()
        try {
            // the fixture's Game shows the name in its card list (#86)
            compose.waitUntil(30_000) { compose.onAllNodesWithTag("list-row-${uids[1]}").fetchSemanticsNodes().isNotEmpty() }
            compose.onNodeWithText("Name").assertIsDisplayed()
            compose.onNodeWithTag("list-row-${uids[0]}").assertTextContains("Long Wrapping Text")
            compose.onNodeWithTag("list-row-${uids[1]}").performClick()
            compose.waitUntil(10_000) { opened == 1 }
            // a long press selects, as in the grid
            compose.onNodeWithTag("list-row-${uids[2]}").performTouchInput { longClick() }
            compose.waitUntil(10_000) { compose.onAllNodesWithTag("selection-bar").fetchSemanticsNodes().isNotEmpty() }
        } finally {
            // (the choice is remembered: back to the grid for the other tests)
            compose.onNodeWithTag("cards-grid-view").performClick()
        }
    }
}
