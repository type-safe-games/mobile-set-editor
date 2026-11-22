// Mobile Set Editor - the Set info tab: set info, the Set symbol from a Template, the Set's Style (#68)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.foundation.lazy.grid.rememberLazyGridState
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollToNode
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
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.TargetKind

@RunWith(AndroidJUnit4::class)
class SetInfoTest {
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

    private fun scrollTo(tag: String) = compose.waitUntil(10_000) {
        runCatching { compose.onNodeWithTag("set-info").performScrollToNode(hasTestTag(tag)) }.isSuccess
    }

    @Test
    fun titleSymbolAndStyle() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent {
            SetHomeScreen(model, rememberLazyGridState(), onOpenCard = {}, onSaveAs = {}, onClosed = {})
        }
        model.open(Uri.fromFile(TestFixture.zip("set-info.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.session != 0L }
        val session = model.state.value.session
        fun value(kind: TargetKind, field: String) = runBlocking { Core.getValue(session, kind, "", field).value }
        compose.onNodeWithText("Set info").performClick()
        waitForTag("set-info-preview")

        // the title, typed into
        scrollTo("info-title")
        compose.onNodeWithTag("info-title").performClick()
        waitForTag("field-text")
        compose.onNodeWithTag("field-text").performTextInputSelection(TextRange(0))
        compose.onNodeWithTag("field-text").performTextInput("My ")
        compose.waitUntil(10_000) { value(TargetKind.Set, "title") == "My Fixture cards" }
        compose.onNodeWithTag("done").performClick()

        // the Set symbol, from a Template's .mse-symbol file: every Card goes stale and renders again
        scrollTo("info-symbol")
        compose.onNodeWithTag("info-symbol").performClick()
        waitForTag("symbol-templates")
        compose.onNodeWithTag("symbol-templates").performClick()
        waitForTag("template-symbol-triangle.mse-symbol")
        compose.onNodeWithTag("template-symbol-triangle.mse-symbol").performClick()
        compose.waitUntil(10_000) { value(TargetKind.Set, "symbol").isNotEmpty() }
        compose.onNodeWithTag("done").performClick()
        compose.onNodeWithTag("home-undo").performClick()
        compose.waitUntil(10_000) { value(TargetKind.Set, "symbol").isEmpty() }

        // the Set's Style: every Card follows it; its options are listed
        scrollTo("change-set-style")
        compose.onNodeWithTag("change-set-style").performClick()
        waitForTag("style-fixture-alternate.mse-style")
        compose.onNodeWithTag("style-fixture-alternate.mse-style").performClick()
        compose.waitUntil(10_000) { model.state.value.cardStyles.all { it.stylesheet == "fixture-alternate.mse-style" } }
        scrollTo("info-style-accent")
        compose.onNodeWithTag("home-undo").performClick()
        compose.waitUntil(10_000) { model.state.value.cardStyles.all { it.stylesheet == "fixture-standard.mse-style" } }
        assertEquals("My Fixture cards", value(TargetKind.Set, "title"))
        assertTrue(model.state.value.canUndo)
    }
}
