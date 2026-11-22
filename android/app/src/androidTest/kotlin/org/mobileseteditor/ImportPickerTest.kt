// Mobile Set Editor - the Template import picker: dependencies follow, refused packages can't be chosen (#71)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.ui.test.assertIsNotEnabled
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.compose.ui.test.performTextClearance
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.Verdict

@RunWith(AndroidJUnit4::class)
class ImportPickerTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    private fun text(bytes: ByteArray) = String(bytes, Charsets.UTF_8)

    /** A Game, a Style that needs it, a symbol font on its own, and one too new to install */
    private fun pickerZip(): java.io.File {
        val files = mutableMapOf<String, ByteArray>()
        for ((k, v) in TestFixture.assetFiles("data/fixture.mse-game")) files["data/pick-game.mse-game/$k"] = v
        for ((k, v) in TestFixture.assetFiles("data/fixture-standard.mse-style")) {
            files["data/pick-style.mse-style/$k"] = if (k == "style") {
                text(v).replace("game: fixture", "game: pick-game").replace("package: fixture.mse-game", "package: pick-game.mse-game")
                    .toByteArray(Charsets.UTF_8)
            } else v
        }
        for ((k, v) in TestFixture.assetFiles("data/fixture-symbols.mse-symbol-font")) {
            files["data/pick-other.mse-symbol-font/$k"] = v
            files["data/pick-new.mse-symbol-font/$k"] =
                if (k == "symbol-font") text(v).replace("mse version: 2.6.0", "mse version: 99.0.0").toByteArray(Charsets.UTF_8) else v
        }
        return TestFixture.zip("picker.zip", files)
    }

    private fun waitForTag(tag: String) = compose.waitUntil(30_000) {
        compose.onAllNodesWithTag(tag).fetchSemanticsNodes().isNotEmpty()
    }

    private fun search(text: String) {
        compose.onNodeWithTag("import-search").performTextClearance()
        compose.onNodeWithTag("import-search").performTextInput(text)
    }

    @Test
    fun aStyleBringsItsGameAndARefusedPackageCantBeChosen() {
        val model = TemplatesViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { TemplatesScreen(model) }
        val zip = Uri.fromFile(pickerZip())
        model.picked(zip)
        waitForTag("scan")
        val scan = (model.import.value as ImportState.Scanned).scan
        assertEquals(Verdict.Refuse, scan.packages.first { it.name == "pick-new.mse-symbol-font" }.verdict)
        assertTrue("pick-game.mse-game" in scan.packages.first { it.name == "pick-style.mse-style" }.dependencies)

        // nothing, then only the Style: its Game comes with it
        compose.onNodeWithTag("choose-none").performClick()
        search("pick-style")
        waitForTag("package-pick-style.mse-style")
        compose.onNodeWithTag("package-pick-style.mse-style").performClick()
        val choice = (model.import.value as ImportState.Scanned).choice
        assertEquals(setOf("pick-style.mse-style", "pick-game.mse-game"), choice.packages)
        assertTrue(choice.size > 0)
        // the refused one can't be chosen
        search("pick-new")
        waitForTag("package-pick-new.mse-symbol-font")
        compose.onNodeWithTag("package-pick-new.mse-symbol-font").assertIsNotEnabled()

        compose.onNodeWithTag("confirm").performClick()
        waitForTag("import-summary")
        val installed = runBlocking { Core.listTemplates().templates.map { it.name } }
        assertTrue("pick-game.mse-game" in installed && "pick-style.mse-style" in installed)
        assertFalse("pick-other.mse-symbol-font" in installed)
        assertFalse("pick-new.mse-symbol-font" in installed)

        // the same zip again: what is installed is skipped, the rest is offered
        model.dismiss()
        model.picked(zip)
        waitForTag("scan")
        val again = (model.import.value as ImportState.Scanned)
        assertEquals(Verdict.SkipNotNewer, again.scan.packages.first { it.name == "pick-style.mse-style" }.verdict)
        assertEquals(setOf("pick-other.mse-symbol-font"), again.choice.packages)
        model.cancel()
    }
}
