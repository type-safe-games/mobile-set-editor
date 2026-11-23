// Mobile Set Editor - the Keywords tab: add a keyword with a parameter, use it, edit its reminder, remove and undo (#70)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.performTextInputSelection
import androidx.compose.ui.text.TextRange
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.TargetKind

@RunWith(AndroidJUnit4::class)
class KeywordsTest {
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
    fun aKeywordWithAParameterIsUsedEditedRemovedAndUndone() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent {
            SetHomeScreen(model, rememberLazyGridState(), onOpenCard = {}, onSaveAs = {}, onClosed = {})
        }
        model.open(Uri.fromFile(TestFixture.zip("keywords.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.session != 0L }
        val session = model.state.value.session
        fun keywords() = runBlocking { Core.listKeywords(session).keywords }
        fun rules() = runBlocking { Core.getText(session, TargetKind.Card, "fixture-wrap", "rules").text }
        compose.onNodeWithText("Keywords").performClick()
        waitForTag("add-keyword")

        // add one: name, a match with a number parameter, a reminder text using it
        compose.onNodeWithTag("add-keyword").performClick()
        waitForTag("field-text")
        compose.onNodeWithTag("field-text").performTextInput("Blink")
        compose.waitUntil(10_000) { keywords().any { !it.fromGame && it.keyword == "Blink" } }
        val id = keywords().first { it.keyword == "Blink" }.id
        compose.onNodeWithTag("next-field").performClick()
        waitForTag("keyword-parameters")
        compose.onNodeWithTag("field-text").performTextInput("Blink ")
        compose.waitUntil(10_000) { runBlocking { Core.getValue(session, TargetKind.Keyword, "$id", "match").value } == "Blink " }
        compose.onNodeWithTag("parameter-number").performClick()
        compose.waitUntil(10_000) { keywords().first { it.id == id }.match == "Blink <number>" }
        compose.onNodeWithTag("next-field").performClick()
        waitForTag("field-text")
        compose.waitUntil(10_000) { compose.onAllNodesWithTag("keyword-parameters").fetchSemanticsNodes().isEmpty() }
        compose.onNodeWithTag("field-text").performTextInput("Blink {param1} times.")
        compose.waitUntil(10_000) { keywords().first { it.id == id }.reminder == "Blink {param1} times." }
        compose.onNodeWithTag("done").performClick()

        // a Card that uses it shows its reminder text
        runBlocking { model.edited(Core.setValue(session, TargetKind.Card, "fixture-wrap", "rules", "Blink 3")) }
        compose.waitUntil(10_000) { "Blink 3 times." in rules() }
        assertEquals(1, keywords().first { it.id == id }.usage)

        // editing the reminder text changes that Card
        compose.onNodeWithTag("keyword-$id").performClick()
        waitForTag("next-field")
        compose.onNodeWithTag("next-field").performClick()
        compose.onNodeWithTag("next-field").performClick()
        compose.waitUntil(10_000) { compose.onAllNodesWithText("Keyword: Reminder").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithTag("field-text").performTextInputSelection(TextRange(0))
        compose.onNodeWithTag("field-text").performTextInput("Now ")
        compose.waitUntil(10_000) { "Now Blink 3 times." in rules() }
        compose.onNodeWithTag("done").performClick()

        // remove it: the Card loses the reminder; one undo brings both back
        waitForTag("remove-keyword-$id")
        compose.onNodeWithTag("remove-keyword-$id").performClick()
        compose.waitUntil(10_000) { keywords().none { it.id == id } }
        assertTrue("times." !in rules())
        compose.onNodeWithTag("home-undo").performClick()
        compose.waitUntil(10_000) { keywords().any { it.id == id } && "Now Blink 3 times." in rules() }
    }
}
