// Mobile Set Editor - removing installed Templates, chosen ones or all (#96)
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.type_safe_games.mobileseteditor

import android.app.Application
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.runBlocking
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import io.github.type_safe_games.mobileseteditor.core.Core
import io.github.type_safe_games.mobileseteditor.core.Stores

@RunWith(AndroidJUnit4::class)
class RemoveTemplatesTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    private fun installed() = runBlocking { Core.listTemplates().templates.map { it.name }.toSet() }

    private fun waitForTag(tag: String) = compose.waitUntil(30_000) { compose.onAllNodesWithTag(tag).fetchSemanticsNodes().isNotEmpty() }

    /** The other tests share the fixture: put it back */
    @After
    fun reinstall() {
        TestFixture.copyAssets("data", Stores.templates(TestFixture.app))
        TestFixture.copyAssets("fonts", Stores.fonts(TestFixture.app))
        assertTrue(runBlocking { Core.refreshTemplates() }.ok)
    }

    @Test
    fun aChosenStyleThenAll() {
        val model = TemplatesViewModel(TestFixture.app.applicationContext as Application)
        compose.setContent { TemplatesScreen(model) }
        compose.waitUntil(30_000) { model.installed.value.isNotEmpty() }
        // (other tests' imports may have added packages: only the fixture's matter here)
        val fixture = setOf("fixture.mse-game", "fixture-standard.mse-style", "fixture-alternate.mse-style", "fixture-symbols.mse-symbol-font")
        assertTrue(installed().containsAll(fixture))

        // one Style: the other Style still needs the Game and the symbol font, so they stay
        compose.onNodeWithTag("remove-some").performClick()
        waitForTag("remove-pick-fixture-alternate.mse-style")
        compose.onNodeWithTag("remove-pick-fixture-alternate.mse-style").performClick()
        compose.onNodeWithTag("remove-chosen").performClick()
        waitForTag("remove-confirm")
        compose.onNodeWithTag("remove-confirm").performClick()
        compose.waitUntil(30_000) { "fixture-alternate.mse-style" !in installed() }
        assertEquals(fixture - "fixture-alternate.mse-style", installed().intersect(fixture))
        compose.waitUntil(10_000) { model.installed.value.none { it.name == "fixture-alternate.mse-style" } }

        // the Game takes what needs it (the symbol font goes too unless another test's Style uses it)
        val plan = runBlocking { model.planRemoval(setOf("fixture.mse-game")) }
        assertEquals(setOf("fixture.mse-game"), plan.chosen)
        assertTrue("fixture-standard.mse-style" in plan.dependents)

        // all: the store is empty, and of the fonts only the bundled fallback is left
        compose.onNodeWithTag("remove-all").performClick()
        waitForTag("remove-all-confirm")
        compose.onNodeWithTag("remove-all-confirm").performClick()
        // (the screen's list is read again only once the stores are cleared and the core refreshed)
        compose.waitUntil(30_000) { model.installed.value.isEmpty() }
        assertTrue(installed().isEmpty())
        assertTrue(Stores.templates(TestFixture.app).listFiles().isNullOrEmpty())
        val fonts = Stores.fonts(TestFixture.app).list().orEmpty().toSet()
        assertTrue("fonts left: $fonts", fonts.isNotEmpty() && fonts.all { it.startsWith("NotoSans-") })
    }
}
