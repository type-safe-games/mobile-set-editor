// Mobile Set Editor - the Card details sheet: a field with no spot on the Card, own style options, the Style (#67)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performScrollToNode
import androidx.compose.ui.test.hasTestTag
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.performTextInput
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.TargetKind
import io.github.type_safe_games.mobileseteditor.editor.CardEditorScreen

@RunWith(AndroidJUnit4::class)
class CardDetailsTest {
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

    private fun waitGone(tag: String) = compose.waitUntil(10_000) {
        compose.onAllNodesWithTag(tag).fetchSemanticsNodes().isEmpty()
    }

    private var session = 0L

    private fun scrollTo(row: String) {
        var last: Throwable? = null
        try {
            compose.waitUntil(10_000) {
                val r = runCatching { compose.onNodeWithTag("details-list").performScrollToNode(hasTestTag(row)) }
                last = r.exceptionOrNull()
                r.isSuccess
            }
        } catch (e: Throwable) {
            val texts = compose.onAllNodes(androidx.compose.ui.test.hasAnyAncestor(hasTestTag("details-list")), useUnmergedTree = true)
                .fetchSemanticsNodes().mapNotNull { it.config.getOrElseNullable(androidx.compose.ui.semantics.SemanticsProperties.Text) { null }?.joinToString() }
            throw AssertionError("scrolling to $row: $texts", e)
        }
    }

    /** Open the details sheet and pick a row in it */
    private fun details(row: String) {
        compose.onNodeWithTag("details").performClick()
        waitForTag("card-details")
        scrollTo(row)
        compose.onNodeWithTag(row).performClick()
    }

    @Test
    fun fieldsWithNoSpotStyleOptionsAndTheStyle() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { CardEditorScreen(model, onBack = {}) }
        // the first Card in the alternate Style, which draws no level
        val files = TestFixture.assetFiles("sets/fixture-cards.mse-set").toMutableMap()
        files["set"] = String(files.getValue("set"), Charsets.UTF_8)
            .replace("\tuid: fixture-wrap\n", "\tuid: fixture-wrap\n\tstylesheet: alternate\n").toByteArray(Charsets.UTF_8)
        model.open(Uri.fromFile(TestFixture.zip("details.mse-set", files)))
        compose.waitUntil(30_000) { model.state.value.card != null }
        session = model.state.value.session
        fun value(kind: TargetKind, field: String) = runBlocking { Core.getValue(session, kind, "fixture-wrap", field).value }
        assertTrue(model.state.value.card!!.regions.none { it.field == "level" })

        // the level has no spot on this Card: edit it from the sheet
        details("details-level")
        waitForTag("slider-plus")
        compose.onNodeWithTag("slider-plus").performClick()
        compose.waitUntil(10_000) { value(TargetKind.Card, "level") == "4" }
        compose.onNodeWithTag("undo").performClick()
        compose.waitUntil(10_000) { value(TargetKind.Card, "level") == "3" }
        compose.onNodeWithTag("done").performClick()

        // own style options, then one of them: this Card's only
        assertFalse(model.state.value.cardStyles[0].ownStyling)
        compose.onNodeWithTag("details").performClick()
        waitForTag("card-details")
        scrollTo("own-styling")
        compose.onNodeWithTag("own-styling").performClick()
        compose.waitUntil(10_000) { model.state.value.cardStyles[0].ownStyling }
        scrollTo("details-style-accent")
        compose.onNodeWithTag("details-style-accent").performClick()
        waitForTag("choice-blue")
        compose.onNodeWithTag("choice-blue").performClick()
        compose.waitUntil(10_000) { value(TargetKind.CardStyle, "accent") == "blue" }
        // the Set's options for the Style are untouched
        val setOptions = runBlocking { Core.setCardOwnStyling(session, "fixture-wrap", false) }
        assertTrue(setOptions.result.ok)
        assertEquals("red", value(TargetKind.CardStyle, "accent"))
        runBlocking { Core.undo(session) }
        assertEquals("blue", value(TargetKind.CardStyle, "accent"))
        compose.onNodeWithTag("done").performClick()

        // the Card's Style: back to the Set's (standard), and undo
        details("change-style")
        waitForTag("style-fixture-standard.mse-style")
        compose.onNodeWithTag("style-fixture-standard.mse-style").performClick()
        compose.waitUntil(10_000) { model.state.value.cardStyles.getOrNull(0)?.stylesheet == "fixture-standard.mse-style" }
        compose.waitUntil(30_000) { model.state.value.card?.regions?.any { it.field == "level" } == true }
    }

    @Test
    fun styleFirstAndTheFieldsOnTheCardFolded() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { CardEditorScreen(model, onBack = {}) }
        model.open(Uri.fromFile(TestFixture.zip("details-order.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.card != null }
        assertTrue(model.state.value.card!!.regions.any { it.field == "name" && it.editable })
        compose.onNodeWithTag("details").performClick()
        waitForTag("card-details")
        // the Card's Style is the first thing, no scrolling needed (#88)
        waitForTag("change-style")
        compose.onNodeWithTag("change-style").assertIsDisplayed()
        // the name has a spot on the Card: folded away until asked for
        scrollTo("details-on-card")
        assertTrue(compose.onAllNodesWithTag("details-name").fetchSemanticsNodes().isEmpty())
        compose.onNodeWithTag("details-on-card").performClick()
        scrollTo("details-name")
        compose.onNodeWithTag("details-on-card").performClick()
        compose.waitUntil(5_000) { compose.onAllNodesWithTag("details-name").fetchSemanticsNodes().isEmpty() }
        // searching shows it again
        scrollTo("details-find")
        compose.onNodeWithTag("details-find").performTextInput("name")
        scrollTo("details-name")
        compose.onNodeWithTag("details-name").assertIsDisplayed()
    }
}
