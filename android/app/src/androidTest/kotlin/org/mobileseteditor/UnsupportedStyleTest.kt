// Mobile Set Editor - the style picker flags Unsupported styles from a status file (#72)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.StyleSupport
import org.mobileseteditor.core.TemplateInfo

@RunWith(AndroidJUnit4::class)
class UnsupportedStyleTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    @Test
    fun theFlagShowsForAListedStyle() {
        val styles: List<TemplateInfo> = runBlocking { Core.listTemplates().templates.filter { it.game == "fixture.mse-game" && it.type == "style" } }
        val standard = styles.first { it.name == "fixture-standard.mse-style" }
        // the alternate Style failed (any version), the standard one passed in its installed version
        val status = StyleSupport.parse(
            "fixture-standard.mse-style\t${standard.version}\tsupported\nfixture-alternate.mse-style\t\tunsupported\n",
        )
        var picked: String? = null
        compose.setContent { StylePicker(styles, onPick = { picked = it.name }, support = status) }
        compose.onNodeWithTag("unsupported-fixture-alternate.mse-style", useUnmergedTree = true).assertExists()
        assertTrue(compose.onAllNodesWithTag("unsupported-fixture-standard.mse-style", useUnmergedTree = true).fetchSemanticsNodes().isEmpty())
        assertTrue(compose.onAllNodesWithTag("untested-fixture-standard.mse-style", useUnmergedTree = true).fetchSemanticsNodes().isEmpty())
        // still selectable
        compose.onNodeWithTag("style-fixture-alternate.mse-style").performClick()
        assertEquals("fixture-alternate.mse-style", picked)
    }

    @Test
    fun styleNotInTheStatusIsUntested() {
        val styles = runBlocking { Core.listTemplates().templates.filter { it.game == "fixture.mse-game" && it.type == "style" } }
        compose.setContent { StylePicker(styles, onPick = {}, support = StyleSupport.parse("")) }
        compose.onNodeWithTag("untested-fixture-standard.mse-style", useUnmergedTree = true).assertExists()
    }
}
