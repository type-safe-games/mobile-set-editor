// Mobile Set Editor - New Set from the fixture's Game and a Style: search, create, Save as, reopen (#66)
// SPDX-License-Identifier: GPL-3.0-or-later

package org.mobileseteditor

import android.app.Application
import android.net.Uri
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.performClick
import androidx.compose.ui.test.performTextInput
import androidx.test.ext.junit.runners.AndroidJUnit4
import java.io.File
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mobileseteditor.core.Core
import org.mobileseteditor.core.WorkingCopies

@RunWith(AndroidJUnit4::class)
class NewSetTest {
    companion object {
        @BeforeClass
        @JvmStatic
        fun start() = TestFixture.start()
    }

    @get:Rule
    val compose = createComposeRule()

    private val app get() = TestFixture.app.applicationContext as Application

    private suspend fun SetViewModel.waitFor(condition: (SetState) -> Boolean) = withTimeout(30_000) {
        while (!condition(state.value)) delay(10)
    }

    @Test
    fun aNewSetIsSavedAsAndReopens() {
        val model = SetViewModel(app)
        var created: Pair<String, String>? = null
        compose.setContent {
            NewSetScreen(onCreate = { g, s -> created = g to s; model.newSet(g, s) }, onTemplates = {}, onBack = {})
        }
        // with one Game, straight to its Styles (other tests may have installed more: then pick it)
        compose.waitUntil(30_000) {
            compose.onAllNodesWithTag("style-fixture-standard.mse-style").fetchSemanticsNodes().isNotEmpty() ||
                compose.onAllNodesWithTag("game-fixture.mse-game").fetchSemanticsNodes().isNotEmpty()
        }
        if (compose.onAllNodesWithTag("game-fixture.mse-game").fetchSemanticsNodes().isNotEmpty()) {
            compose.onNodeWithTag("game-fixture.mse-game").performClick()
        }
        compose.waitUntil(30_000) { compose.onAllNodesWithTag("style-fixture-standard.mse-style").fetchSemanticsNodes().isNotEmpty() }
        compose.onNodeWithTag("style-search").performTextInput("altern")
        compose.waitUntil(5_000) { compose.onAllNodesWithTag("style-fixture-standard.mse-style").fetchSemanticsNodes().isEmpty() }
        compose.onNodeWithTag("style-fixture-alternate.mse-style").performClick()
        assertEquals("fixture.mse-game" to "fixture-alternate.mse-style", created)

        runBlocking {
            model.waitFor { it.session != 0L }
            val copy = model.state.value.workingCopy!!
            assertEquals(SetViewModel.UNTITLED, copy.name)
            assertNull(copy.uri)
            assertEquals(1, model.state.value.cardUids.size) // the Game's one new card, as on the desktop
            // no document yet: Save is Save as
            assertFalse(model.save())

            // the process dies: the new Set is back from its Working copy
            Core.closeSession(model.state.value.session)
            val again = SetViewModel(app)
            again.restore()
            again.waitFor { it.session != 0L }
            assertEquals(copy.id, again.state.value.workingCopy!!.id)
            assertTrue("meta ${WorkingCopies.get(app, copy.id)} info ${Core.sessionInfo(again.state.value.session).dirty}", again.state.value.dirty)

            // Save as, and open the document
            val file = File(TestFixture.work.apply { mkdirs() }, "new-set.mse-set").apply { delete() }
            val saved = again.saveAs(Uri.fromFile(file))
            assertTrue("messages ${again.state.value.messages}", saved)
            assertFalse(again.state.value.dirty)
            assertEquals("new-set.mse-set", again.state.value.workingCopy!!.name)
            again.close(CloseChoice.Keep).join()
            WorkingCopies.get(app, copy.id)?.let { WorkingCopies.delete(app, it) }
            val reopened = SetViewModel(app)
            reopened.open(Uri.fromFile(file))
            reopened.waitFor { it.session != 0L }
            assertEquals(1, reopened.state.value.cardUids.size)
            assertEquals("fixture-alternate.mse-style", Core.sessionInfo(reopened.state.value.session).stylesheet)
            reopened.close(CloseChoice.Discard).join()
        }
    }
}
