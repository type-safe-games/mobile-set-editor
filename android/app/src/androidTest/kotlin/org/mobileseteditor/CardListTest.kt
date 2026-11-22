// Mobile Set Editor - add, duplicate, move and delete Cards from the Set home grid, and undo each (#69)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.ui.test.assertTextEquals
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.longClick
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextReplacement
import androidx.compose.ui.test.performTouchInput
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CardListTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    private val original = listOf("fixture-wrap", "fixture-front", "fixture-back")

    private fun waitForTag(tag: String) = compose.waitUntil(30_000) {
        compose.onAllNodesWithTag(tag).fetchSemanticsNodes().isNotEmpty()
    }

    @Test
    fun addDuplicateMoveDeleteAndUndo() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        var opened = -1
        compose.setContent {
            SetHomeScreen(model, rememberLazyGridState(), onOpenCard = { opened = it }, onSaveAs = {}, onClosed = {})
        }
        model.open(Uri.fromFile(TestFixture.zip("card-list.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.cardUids.size == 3 }
        fun uids() = model.state.value.cardUids
        fun undo(expected: List<String>) {
            compose.onNodeWithTag("home-undo").performClick()
            compose.waitUntil(10_000) { uids() == expected }
        }

        // add: at the end, and opened
        compose.onNodeWithTag("add-card").performClick()
        compose.waitUntil(10_000) { uids().size == 4 }
        compose.waitUntil(10_000) { opened == 3 }
        assertEquals(original, uids().take(3))
        undo(original)

        // duplicate the first Card: the copy follows it
        waitForTag("thumbnail-fixture-wrap")
        compose.onNodeWithTag("thumbnail-fixture-wrap").performTouchInput { longClick() }
        waitForTag("selection-bar")
        compose.onNodeWithTag("selected-count").assertTextEquals("1 selected")
        compose.onNodeWithTag("duplicate").performClick()
        compose.waitUntil(10_000) { uids().size == 4 }
        assertEquals("fixture-wrap", uids()[0])
        assertEquals(listOf("fixture-front", "fixture-back"), uids().drop(2))
        undo(original)

        // move the back face to the front
        compose.onNodeWithTag("thumbnail-fixture-back").performTouchInput { longClick() }
        waitForTag("move")
        compose.onNodeWithTag("move").performClick()
        compose.onNodeWithTag("move-position").performTextReplacement("1")
        compose.onNodeWithTag("move-confirm").performClick()
        compose.waitUntil(10_000) { uids() == listOf("fixture-back", "fixture-wrap", "fixture-front") }
        undo(original)

        // delete two, and undo from the snackbar
        compose.onNodeWithTag("thumbnail-fixture-wrap").performTouchInput { longClick() }
        waitForTag("selection-bar")
        compose.onNodeWithTag("thumbnail-fixture-front").performClick()
        compose.onNodeWithTag("selected-count").assertTextEquals("2 selected")
        compose.onNodeWithTag("delete").performClick()
        compose.waitUntil(10_000) { uids() == listOf("fixture-back") }
        // the back face lost its link with the removed front
        assertEquals(emptyList<Any>(), model.state.value.cardLinks[0])
        compose.waitUntil(10_000) { compose.onAllNodesWithText("Deleted 2 Cards").fetchSemanticsNodes().isNotEmpty() }
        snackbarUndo()
        compose.waitUntil(10_000) { uids() == original }
        assertEquals(1, model.state.value.cardLinks[2].size)
    }

    private fun snackbarUndo() {
        // the snackbar's action is the Undo inside the snackbar host
        compose.onNode(
            androidx.compose.ui.test.hasText("Undo") and androidx.compose.ui.test.hasAnyAncestor(androidx.compose.ui.test.hasTestTag("snackbar")),
        ).performClick()
    }

    private fun androidx.compose.ui.test.junit4.ComposeContentTestRule.onAllNodesWithText(text: String) =
        onAllNodes(androidx.compose.ui.test.hasText(text))
}
