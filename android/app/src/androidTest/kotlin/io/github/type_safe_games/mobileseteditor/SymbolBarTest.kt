// Mobile Set Editor - insert symbols from the field's symbol menu, and type in symbols (#64)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.test.click
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.performTextInputSelection
import androidx.compose.ui.test.performTouchInput
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
import io.github.type_safe_games.mobileseteditor.core.SymbolMenuItem
import io.github.type_safe_games.mobileseteditor.core.TargetKind
import io.github.type_safe_games.mobileseteditor.editor.CardEditorScreen

@RunWith(AndroidJUnit4::class)
class SymbolBarTest {
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
    fun symbolsFromTheMenuAndTypedInSymbolMode() {
        val model = SetViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { CardEditorScreen(model, onBack = {}) }
        model.open(Uri.fromFile(TestFixture.zip("symbols.mse-set", TestFixture.assetFiles("sets/fixture-cards.mse-set"))))
        compose.waitUntil(30_000) { model.state.value.card != null }
        val card = model.state.value.card!!
        val session = model.state.value.session
        fun rules() = runBlocking { Core.getValue(session, TargetKind.Card, card.uid, "rules").value }
        val menu = runBlocking { Core.symbolMenu(session, TargetKind.Card, card.uid, "rules") }
        val red = menu.items.first { it.type == SymbolMenuItem.Type.Code }
        val colors = menu.items.first { it.type == SymbolMenuItem.Type.Submenu }
        val blue = colors.items.first { it.code == "U" }

        // the name has no symbol font: no bar
        fun tap(field: String) {
            val r = card.regions.first { it.field == field }
            // the Card in the UI, not just in the model: it composes a moment later
            compose.waitUntil(30_000) { compose.onAllNodesWithTag("card-${card.uid}").fetchSemanticsNodes().isNotEmpty() }
            compose.onNodeWithTag("card-${card.uid}").performTouchInput {
                val k = width.toDouble() / card.bitmap.width
                click(Offset(((r.x + r.width / 2) * k).toFloat(), ((r.y + r.height / 2) * k).toFloat()))
            }
            waitForTag("field-text")
        }
        tap("name")
        compose.waitUntil(5_000) { compose.onAllNodesWithTag("editing-field").fetchSemanticsNodes().isNotEmpty() }
        assertTrue(compose.onAllNodesWithTag("symbol-bar").fetchSemanticsNodes().isEmpty())
        compose.onNodeWithTag("done").performClick()

        // the rules: insert the red symbol at the start, then the blue one from the submenu
        tap("rules")
        // editing on the Card (#83) keeps the symbols behind a button
        if (compose.onAllNodesWithTag("symbols-toggle").fetchSemanticsNodes().isNotEmpty()) compose.onNodeWithTag("symbols-toggle").performClick()
        waitForTag("symbol-bar")
        compose.onNodeWithTag("field-text").performTextInputSelection(TextRange(0))
        compose.onNodeWithTag("symbol-${red.label}").performClick()
        compose.waitUntil(10_000) { rules().startsWith("<sym>R</sym>") }
        compose.onNodeWithTag("symbol-menu-${colors.label}").performClick()
        waitForTag("symbol-${blue.label}")
        compose.onNodeWithTag("symbol-${blue.label}").performClick()
        compose.waitUntil(10_000) { rules().startsWith("<sym>RU</sym>") || rules().startsWith("<sym>R</sym><sym>U</sym>") }

        // typing in symbol mode makes symbols too
        compose.onNodeWithTag("symbol-mode").performClick()
        compose.onNodeWithTag("field-text").performTextInput("R")
        compose.waitUntil(10_000) { rules().replace("</sym><sym>", "").startsWith("<sym>RUR</sym>") }
        // and the Card shows them: one undo step each, back to the start
        val start = String(TestFixture.assetFiles("sets/fixture-cards.mse-set").getValue("set"), Charsets.UTF_8)
        assertTrue(start.contains("Pay <sym>R</sym><sym>U</sym>"))
        for (i in 0 until 3) compose.onNodeWithTag("undo").performClick()
        compose.waitUntil(10_000) { rules().startsWith("Pay ") }
        assertEquals(false, model.state.value.canUndo)
    }
}
